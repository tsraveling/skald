#include "document.h"
#include "lsp_actions.h"
#include "lsp_parse_state.h"
#include "skald_grammar.h"
#include <set>
#include <sstream>
#include <tao/pegtl.hpp>

namespace SkaldLsp {

Document::Document(const std::string &uri, const std::string &text)
    : uri_(uri), text_(text) {
    parse();
}

void Document::update(const std::string &text) {
    text_ = text;
    symbols_.clear();
    diagnostics_.clear();
    parse_ok_ = false;
    module_ = Skald::Module{};
    parse();
}

void Document::parse() {
    // Extract a simple filename from the URI for ParseState
    std::string filename = uri_;
    auto pos = filename.rfind('/');
    if (pos != std::string::npos)
        filename = filename.substr(pos + 1);

    LspParseState state(filename);

    // Ensure text ends with newline (PEGTL grammar expects it)
    std::string source = text_;
    if (source.empty() || source.back() != '\n') {
        source += '\n';
    }

    try {
        tao::pegtl::memory_input input(source, filename);
        tao::pegtl::parse<Skald::grammar, lsp_action>(input, state);
        parse_ok_ = true;
        module_ = std::move(state.module);
        symbols_ = std::move(state.symbols);
    } catch (const tao::pegtl::parse_error &e) {
        parse_ok_ = false;
        // Extract position from the parse error
        const auto &p = e.position_object();
        LspTypes::Diagnostic diag;
        diag.range.start.line = static_cast<int>(p.line) - 1;
        diag.range.start.character = static_cast<int>(p.column) - 1;
        diag.range.end.line = static_cast<int>(p.line) - 1;
        diag.range.end.character = static_cast<int>(p.column);
        diag.severity = LspTypes::DiagnosticSeverity::Error;
        diag.message = std::string(e.message());
        diagnostics_.push_back(diag);
        // Still try to capture partial symbols
        symbols_ = std::move(state.symbols);
        module_ = std::move(state.module);
    }

    // Report skipped lines as warnings
    for (auto &skip : state.skipped_lines) {
        LspTypes::Diagnostic diag;
        diag.range.start.line = skip.line;
        diag.range.start.character = skip.col;
        diag.range.end.line = skip.line;
        diag.range.end.character = skip.end_col;
        diag.severity = LspTypes::DiagnosticSeverity::Warning;
        diag.message = "Line could not be parsed and was skipped";
        diagnostics_.push_back(diag);
    }

    run_semantic_checks();
}

void Document::run_semantic_checks() {
    // Collect defined block tags
    std::set<std::string> defined_tags;
    for (auto &sym : symbols_) {
        if (sym.kind == SymbolKind::BlockTag && sym.is_definition) {
            defined_tags.insert(sym.name);
        }
    }

    // Check: -> tag referencing non-existent block
    for (auto &sym : symbols_) {
        if (sym.kind == SymbolKind::BlockTag && !sym.is_definition) {
            if (defined_tags.find(sym.name) == defined_tags.end()) {
                LspTypes::Diagnostic diag;
                diag.range.start.line = sym.range.line;
                diag.range.start.character = sym.range.col;
                diag.range.end.line = sym.range.line;
                diag.range.end.character = sym.range.end_col;
                diag.severity = LspTypes::DiagnosticSeverity::Error;
                diag.message =
                    "Block tag '" + sym.name + "' is not defined in this file";
                diagnostics_.push_back(diag);
            }
        }
    }

    // Check: variable declared but never used
    std::set<std::string> declared_vars;
    std::set<std::string> used_vars;
    for (auto &sym : symbols_) {
        if (sym.kind == SymbolKind::Variable) {
            if (sym.is_definition) {
                declared_vars.insert(sym.name);
            } else {
                used_vars.insert(sym.name);
            }
        }
    }
    for (auto &var : declared_vars) {
        if (used_vars.find(var) == used_vars.end()) {
            // Find the declaration symbol to get its range
            for (auto &sym : symbols_) {
                if (sym.kind == SymbolKind::Variable && sym.is_definition &&
                    sym.name == var) {
                    LspTypes::Diagnostic diag;
                    diag.range.start.line = sym.range.line;
                    diag.range.start.character = sym.range.col;
                    diag.range.end.line = sym.range.line;
                    diag.range.end.character = sym.range.end_col;
                    diag.severity = LspTypes::DiagnosticSeverity::Warning;
                    diag.message =
                        "Variable '" + var + "' is declared but never used";
                    diagnostics_.push_back(diag);
                    break;
                }
            }
        }
    }
}

std::optional<SymbolOccurrence> Document::symbol_at(int line,
                                                     int character) const {
    for (auto &sym : symbols_) {
        if (sym.range.line == line && character >= sym.range.col &&
            character < sym.range.end_col) {
            return sym;
        }
    }
    return std::nullopt;
}

std::optional<SymbolOccurrence>
Document::find_definition(const std::string &name, SymbolKind kind) const {
    for (auto &sym : symbols_) {
        if (sym.name == name && sym.kind == kind && sym.is_definition) {
            return sym;
        }
    }
    return std::nullopt;
}

std::vector<SymbolOccurrence>
Document::find_references(const std::string &name, SymbolKind kind) const {
    std::vector<SymbolOccurrence> refs;
    for (auto &sym : symbols_) {
        if (sym.name == name && sym.kind == kind) {
            refs.push_back(sym);
        }
    }
    return refs;
}

std::string Document::get_line(int line) const {
    std::istringstream stream(text_);
    std::string result;
    for (int i = 0; i <= line; ++i) {
        if (!std::getline(stream, result)) {
            return "";
        }
    }
    return result;
}

std::vector<std::string> Document::block_tags() const {
    std::vector<std::string> tags;
    std::set<std::string> seen;
    for (auto &sym : symbols_) {
        if (sym.kind == SymbolKind::BlockTag && sym.is_definition &&
            seen.insert(sym.name).second) {
            tags.push_back(sym.name);
        }
    }
    return tags;
}

std::vector<std::string> Document::variable_names() const {
    std::vector<std::string> vars;
    std::set<std::string> seen;
    for (auto &sym : symbols_) {
        if (sym.kind == SymbolKind::Variable && seen.insert(sym.name).second) {
            vars.push_back(sym.name);
        }
    }
    return vars;
}

std::vector<std::string> Document::method_names() const {
    std::vector<std::string> methods;
    std::set<std::string> seen;
    for (auto &sym : symbols_) {
        if (sym.kind == SymbolKind::Method && seen.insert(sym.name).second) {
            methods.push_back(sym.name);
        }
    }
    return methods;
}

} // namespace SkaldLsp
