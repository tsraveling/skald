#include "document.h"
#include "lsp_actions.h"
#include "lsp_parse_state.h"
#include "move_resolver.h"
#include "parse_guard.h"
#include "project_index.h"
#include "skald_grammar.h"
#include <filesystem>
#include <set>
#include <sstream>
#include <tao/pegtl.hpp>

namespace fs = std::filesystem;

namespace SkaldLsp {

Document::Document(const std::string &uri, const std::string &text,
                   const Skald::Codex *codex, const ProjectIndex *project)
    : uri_(uri), text_(text), codex_(codex), project_(project) {
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

// Map a library ParseError (1-based position) to an LSP diagnostic.
static LspTypes::Diagnostic to_diagnostic(const Skald::ParseError &err) {
    LspTypes::Diagnostic diag;
    int line = err.pos.line > 0 ? static_cast<int>(err.pos.line) - 1 : 0;
    int col = err.pos.column > 0 ? static_cast<int>(err.pos.column) - 1 : 0;
    diag.range.start.line = line;
    diag.range.start.character = col;
    diag.range.end.line = line;
    diag.range.end.character = col + 1;
    diag.severity = err.severity == Skald::ParseError::WARNING
                        ? LspTypes::DiagnosticSeverity::Warning
                        : LspTypes::DiagnosticSeverity::Error;
    diag.message = err.msg;
    return diag;
}

void Document::parse() {
    // Extract a simple filename from the URI for ParseState
    std::string filename = uri_;
    auto pos = filename.rfind('/');
    if (pos != std::string::npos)
        filename = filename.substr(pos + 1);

    // Guard against the non-terminating top_matter rule (see parse_guard.h).
    {
        bool has_stray = false;
        int sl = 0, sc = 0, slen = 0;
        if (!top_matter_safe(text_, has_stray, sl, sc, slen)) {
            parse_ok_ = false;
            if (has_stray) {
                LspTypes::Diagnostic diag;
                diag.range.start = {sl, sc};
                diag.range.end = {sl, sc + slen};
                diag.severity = LspTypes::DiagnosticSeverity::Error;
                diag.message =
                    "Unexpected content before the first '# ' block. Only "
                    "@let / @testbed / @receive blocks and '---' comments may "
                    "appear here.";
                diagnostics_.push_back(diag);
            }
            // No usable module; skip parsing to avoid a hang.
            return;
        }
    }

    LspParseState state(filename, codex_);

    // Ensure text ends with newline (PEGTL grammar expects it)
    std::string source = text_;
    if (source.empty() || source.back() != '\n') {
        source += '\n';
    }

    try {
        tao::pegtl::memory_input input(source, filename);
        tao::pegtl::parse<Skald::grammar, lsp_action>(input, state);
        parse_ok_ = true;

        // The grammar ends in `opt<eof>`, so a syntax error that halts block
        // consumption still "succeeds" with the remainder of the file left
        // unparsed (and invisible to every feature). Detect leftover input and
        // flag exactly where parsing stopped, instead of silently dropping the
        // rest of the file.
        if (!input.empty()) {
            const char *p = input.current();
            size_t n = input.size();
            size_t i = 0;
            while (i < n &&
                   (p[i] == ' ' || p[i] == '\t' || p[i] == '\r' || p[i] == '\n'))
                ++i;
            if (i < n) {
                auto pos = input.position();
                LspTypes::Diagnostic diag;
                diag.range.start.line = static_cast<int>(pos.line) - 1;
                diag.range.start.character = static_cast<int>(pos.column) - 1;
                diag.range.end.line = static_cast<int>(pos.line) - 1;
                diag.range.end.character = static_cast<int>(pos.column);
                diag.severity = LspTypes::DiagnosticSeverity::Error;
                diag.message =
                    "Parsing stopped here; the rest of the file was not "
                    "analyzed. Check this line for a syntax error (e.g. a '--' "
                    "comment that should be '---', or stray text).";
                diagnostics_.push_back(diag);
            }
        }
    } catch (const tao::pegtl::parse_error &e) {
        parse_ok_ = false;
        const auto &p = e.position_object();
        LspTypes::Diagnostic diag;
        diag.range.start.line = static_cast<int>(p.line) - 1;
        diag.range.start.character = static_cast<int>(p.column) - 1;
        diag.range.end.line = static_cast<int>(p.line) - 1;
        diag.range.end.character = static_cast<int>(p.column);
        diag.severity = LspTypes::DiagnosticSeverity::Error;
        diag.message = std::string(e.message());
        diagnostics_.push_back(diag);
    } catch (const std::exception &e) {
        // The library can throw std::runtime_error mid-parse (e.g. empty rvalue
        // buffers). Surface it instead of letting it crash the server.
        parse_ok_ = false;
        LspTypes::Diagnostic diag;
        diag.severity = LspTypes::DiagnosticSeverity::Error;
        diag.message = std::string("Parse error: ") + e.what();
        diagnostics_.push_back(diag);
    }

    // Capture whatever was parsed (full or partial).
    module_ = std::move(state.module);
    symbols_ = std::move(state.symbols);

    // Surface every structured diagnostic the library collected: syntax,
    // method existence / arg-count / arg-type, declaration errors, etc. The
    // library already emits each at its intended severity — no remapping.
    for (const auto &err : state.errors) {
        diagnostics_.push_back(to_diagnostic(err));
    }

    run_semantic_checks();
}

void Document::run_semantic_checks() {
    auto push = [&](const SourceRange &r, LspTypes::DiagnosticSeverity sev,
                    const std::string &msg) {
        LspTypes::Diagnostic diag;
        diag.range.start.line = r.line;
        diag.range.start.character = r.col;
        diag.range.end.line = r.line;
        diag.range.end.character = r.end_col;
        diag.severity = sev;
        diag.message = msg;
        diagnostics_.push_back(diag);
    };

    // This module's path relative to the codex root (for thread-var lookups
    // and GO-path resolution). Empty when there is no codex.
    std::string rel_path;
    if (codex_) {
        std::error_code ec;
        std::string fs_path = uri_;
        const std::string prefix = "file://";
        if (fs_path.rfind(prefix, 0) == 0)
            fs_path = fs_path.substr(prefix.size());
        auto rel = fs::relative(fs_path, codex_->path, ec);
        if (!ec)
            rel_path = rel.string();
    }

    // Helper: is `name` a global defined in the codex?
    auto is_global = [&](const std::string &name) {
        if (!codex_)
            return false;
        for (const auto &g : codex_->global_vars)
            if (g.var.name == name)
                return true;
        return false;
    };

    // --- 1.4 Transition resolution: -> target must resolve to a block ---
    BlockHierarchy hierarchy(module_);
    for (const auto &sym : symbols_) {
        if (sym.kind == SymbolKind::BlockTag && !sym.is_definition) {
            if (!hierarchy.is_defined(sym.name)) {
                push(sym.range, LspTypes::DiagnosticSeverity::Error,
                     "Transition target '" + sym.name +
                         "' is not defined in this file");
            }
        }
    }

    // --- 1.4 GO file existence (path relative to the codex root) ---
    for (const auto &sym : symbols_) {
        if (sym.kind != SymbolKind::FileRef)
            continue;
        if (!codex_) {
            push(sym.range, LspTypes::DiagnosticSeverity::Error,
                 "GO path '" + sym.name +
                     "' cannot be resolved without a codex");
            continue;
        }
        std::error_code ec;
        // resolve_path() is non-const in the library; replicate it here.
        std::string resolved = (fs::path(codex_->path) / sym.name).string();
        if (!fs::exists(resolved, ec)) {
            push(sym.range, LspTypes::DiagnosticSeverity::Error,
                 "GO target file '" + sym.name + "' does not exist");
        }
    }

    // --- 1.3 @let shadows a global (and 1.x unused module var) ---
    // Collect variable references for the unused check.
    std::set<std::string> used_vars;
    for (const auto &sym : symbols_)
        if (sym.kind == SymbolKind::Variable && !sym.is_definition)
            used_vars.insert(sym.name);

    for (const auto &sym : symbols_) {
        if (sym.kind != SymbolKind::Variable || !sym.is_definition)
            continue;
        if (is_global(sym.name)) {
            push(sym.range, LspTypes::DiagnosticSeverity::Warning,
                 "'" + sym.name +
                     "' shadows a global, and global will always be used "
                     "first; this module var will be unused!");
            continue; // shadow message already covers it
        }
        if (used_vars.find(sym.name) == used_vars.end()) {
            push(sym.range, LspTypes::DiagnosticSeverity::Warning,
                 "Module variable '" + sym.name +
                     "' is declared but never used");
        }
    }

    // --- 1.2 action-typed method used as an rvalue / conditional ---
    if (codex_) {
        for (const auto &sym : symbols_) {
            if (sym.kind != SymbolKind::Method || !sym.is_rvalue)
                continue;
            for (const auto &def : codex_->method_defs) {
                if (def.name == sym.name &&
                    def.return_type == Skald::ValueType::ACTION) {
                    push(sym.range, LspTypes::DiagnosticSeverity::Error,
                         "'" + sym.name +
                             "' is an action method and returns no value; it "
                             "cannot be used here");
                    break;
                }
            }
        }
    }

    // --- 1.7 / 2.6 testbed sets may only target module/global/thread vars ---
    for (const auto &sym : symbols_) {
        if (sym.kind != SymbolKind::TestbedSet)
            continue;
        bool is_module = false;
        for (const auto &mv : module_.module_vars)
            if (mv.var.name == sym.name) {
                is_module = true;
                break;
            }
        bool is_thread = project_ && !rel_path.empty() &&
                         project_->has_thread_var(sym.name, rel_path);
        if (!is_module && !is_global(sym.name) && !is_thread) {
            push(sym.range, LspTypes::DiagnosticSeverity::Error,
                 "Testbed can only set existing module or global variables; '" +
                     sym.name + "' is not declared");
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

std::vector<std::string> Document::open_transitions() const {
    return SkaldLsp::open_transitions(module_, symbols_);
}

} // namespace SkaldLsp
