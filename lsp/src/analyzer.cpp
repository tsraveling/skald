#include "analyzer.h"
#include "skald.h"
#include <algorithm>

namespace SkaldLsp {

CompletionContext detect_completion_context(const std::string &line_text,
                                            int character) {
    // Get text up to cursor
    std::string before =
        line_text.substr(0, std::min(static_cast<int>(line_text.size()), character));

    // Trim trailing spaces for context detection
    auto trimmed = before;
    auto end = trimmed.find_last_not_of(' ');
    if (end != std::string::npos)
        trimmed = trimmed.substr(0, end + 1);

    // After "#" at start of line: block definition completion
    if (!before.empty() && before[0] == '#') {
        auto after_hash = before.substr(1);
        bool all_id = true;
        for (char c : after_hash) {
            if (!std::isalnum(c) && c != '_' && c != '-') {
                all_id = false;
                break;
            }
        }
        if (all_id)
            return CompletionContext::BlockDefinition;
    }

    // After "-> ": block tag completion
    if (trimmed.size() >= 2 && trimmed.substr(trimmed.size() - 2) == "->") {
        return CompletionContext::MoveTarget;
    }
    if (before.size() >= 3 && before.substr(before.size() - 3) == "-> ") {
        return CompletionContext::MoveTarget;
    }
    // Check if we're on a line with "-> " before cursor
    auto arrow_pos = before.rfind("-> ");
    if (arrow_pos != std::string::npos &&
        arrow_pos + 3 <= static_cast<size_t>(character)) {
        // Make sure there's nothing else after the arrow besides the identifier
        // being typed
        auto after_arrow = before.substr(arrow_pos + 3);
        bool all_id = true;
        for (char c : after_arrow) {
            if (!std::isalnum(c) && c != '_') {
                all_id = false;
                break;
            }
        }
        if (all_id)
            return CompletionContext::MoveTarget;
    }

    // After "GO ": file path completion
    auto go_pos = before.rfind("GO ");
    if (go_pos != std::string::npos) {
        // Check if GO is at beginning of trimmed content
        auto pre_go = before.substr(0, go_pos);
        bool all_space = true;
        for (char c : pre_go) {
            if (c != ' ' && c != '\t') {
                all_space = false;
                break;
            }
        }
        if (all_space)
            return CompletionContext::GoPath;
    }

    // After ":": method completion
    if (!before.empty() && before.back() == ':') {
        return CompletionContext::Method;
    }
    // If we see ":" followed by partial identifier
    auto colon_pos = before.rfind(':');
    if (colon_pos != std::string::npos) {
        auto after_colon = before.substr(colon_pos + 1);
        bool all_id = true;
        for (char c : after_colon) {
            if (!std::isalnum(c) && c != '_') {
                all_id = false;
                break;
            }
        }
        if (all_id && !after_colon.empty())
            return CompletionContext::Method;
    }

    // After "~ ": variable completion (both declarations and mutations)
    {
        auto tilde_pos = trimmed.rfind("~ ");
        if (tilde_pos != std::string::npos) {
            auto after_tilde = trimmed.substr(tilde_pos + 2);
            // Only if we're typing the variable name (all identifier chars so far)
            bool all_id = true;
            for (char c : after_tilde) {
                if (!std::isalnum(c) && c != '_') { all_id = false; break; }
            }
            if (all_id) return CompletionContext::Variable;
        }
    }
    // Inside (? ...) conditional
    if (before.find("(? ") != std::string::npos ||
        before.find("(?") != std::string::npos) {
        return CompletionContext::Variable;
    }
    // Inside { ... } injection
    auto brace_pos = before.rfind('{');
    if (brace_pos != std::string::npos) {
        auto close = before.find('}', brace_pos);
        if (close == std::string::npos) {
            return CompletionContext::Variable;
        }
    }

    return CompletionContext::None;
}

std::vector<LspTypes::CompletionItem>
get_completions(const Document &doc, int line, int character,
                const std::vector<std::string> &workspace_files) {
    std::vector<LspTypes::CompletionItem> items;

    std::string line_text = doc.get_line(line);
    auto ctx = detect_completion_context(line_text, character);

    switch (ctx) {
    case CompletionContext::MoveTarget: {
        for (auto &tag : doc.block_tags()) {
            items.push_back({tag, LspTypes::CompletionItemKind::Reference,
                             "Block tag"});
        }
        break;
    }
    case CompletionContext::Variable: {
        for (auto &var : doc.variable_names()) {
            items.push_back(
                {var, LspTypes::CompletionItemKind::Variable, "Variable"});
        }
        break;
    }
    case CompletionContext::Method: {
        for (auto &method : doc.method_names()) {
            items.push_back(
                {method, LspTypes::CompletionItemKind::Method, "Method"});
        }
        break;
    }
    case CompletionContext::GoPath: {
        for (auto &file : workspace_files) {
            items.push_back(
                {file, LspTypes::CompletionItemKind::File, "Skald file"});
        }
        break;
    }
    case CompletionContext::BlockDefinition: {
        for (auto &tag : doc.undefined_block_refs()) {
            items.push_back({tag, LspTypes::CompletionItemKind::Reference,
                             "Undefined block (referenced by ->)"});
        }
        break;
    }
    case CompletionContext::None:
        break;
    }

    return items;
}

std::vector<LspTypes::DocumentSymbol>
get_document_symbols(const Document &doc) {
    std::vector<LspTypes::DocumentSymbol> symbols;

    if (!doc.parse_succeeded())
        return symbols;

    auto &mod = doc.module();

    // Declarations as top-level variable symbols
    for (auto &decl : mod.declarations) {
        LspTypes::DocumentSymbol sym;
        sym.name = decl.var.name;
        sym.kind = LspTypes::SymbolKind::Variable;
        // Find the symbol occurrence for range info
        for (auto &s : doc.symbols()) {
            if (s.name == decl.var.name && s.kind == SymbolKind::Variable &&
                s.is_definition) {
                sym.range = {{s.range.line, s.range.col},
                             {s.range.line, s.range.end_col}};
                sym.selectionRange = sym.range;
                break;
            }
        }
        symbols.push_back(sym);
    }

    // Blocks as top-level class/namespace symbols with beat children
    for (auto &block : mod.blocks) {
        LspTypes::DocumentSymbol block_sym;
        block_sym.name = "#" + block.tag;
        block_sym.kind = LspTypes::SymbolKind::Class;

        // Find block tag symbol for range
        for (auto &s : doc.symbols()) {
            if (s.name == block.tag && s.kind == SymbolKind::BlockTag &&
                s.is_definition) {
                block_sym.selectionRange = {{s.range.line, s.range.col},
                                            {s.range.line, s.range.end_col}};
                block_sym.range = block_sym.selectionRange;
                break;
            }
        }

        symbols.push_back(block_sym);
    }

    return symbols;
}

std::optional<std::string> get_hover(const Document &doc, int line,
                                      int character) {
    auto sym = doc.symbol_at(line, character);
    if (!sym)
        return std::nullopt;

    switch (sym->kind) {
    case SymbolKind::BlockTag: {
        // Show block beat count if it's a reference
        if (!sym->is_definition) {
            auto def = doc.find_definition(sym->name, SymbolKind::BlockTag);
            if (def) {
                for (auto &block : doc.module().blocks) {
                    if (block.tag == sym->name) {
                        return "Block `#" + sym->name + "` — " +
                               std::to_string(block.beats.size()) + " beat(s)";
                    }
                }
            }
            return "Block tag `#" + sym->name + "` (not defined in this file)";
        }
        for (auto &block : doc.module().blocks) {
            if (block.tag == sym->name) {
                return "Block `#" + sym->name + "` — " +
                       std::to_string(block.beats.size()) + " beat(s)";
            }
        }
        return "Block `#" + sym->name + "`";
    }
    case SymbolKind::Variable: {
        for (auto &decl : doc.module().declarations) {
            if (decl.var.name == sym->name) {
                std::string type_str =
                    decl.is_imported ? "imported" : "local";
                std::string val_str = Skald::rval_to_string(decl.initial_value);
                return "Variable `" + sym->name + "` (" + type_str +
                       ") = " + val_str;
            }
        }
        return "Variable `" + sym->name + "`";
    }
    case SymbolKind::Method:
        return "Method `:" + sym->name + "()`";
    case SymbolKind::FileRef:
        return "Module reference: `" + sym->name + "`";
    }

    return std::nullopt;
}

} // namespace SkaldLsp
