#include "analyzer.h"
#include "skald.h"
#include <algorithm>
#include <cctype>

namespace SkaldLsp {

// Extract comment documentation for a symbol defined at the given line.
// Comments are `---` (three hyphens): an end-of-line comment on the definition
// line, plus contiguous `---` comment lines immediately preceding it.
static std::string extract_comment(const Document &doc, int def_line) {
    auto strip_marker = [](const std::string &s) -> std::string {
        // s is already trimmed of leading whitespace and starts with "---"
        auto content = s.substr(3);
        auto cs = content.find_first_not_of(" \t");
        return cs != std::string::npos ? content.substr(cs) : "";
    };

    std::string same_line_comment;
    std::string line_text = doc.get_line(def_line);
    auto pos = line_text.find("---");
    if (pos != std::string::npos) {
        same_line_comment = line_text.substr(pos);
        // trim to just the marker onward, then strip
        auto t = same_line_comment.find("---");
        same_line_comment = strip_marker(same_line_comment.substr(t));
    }

    // Walk upward collecting contiguous `---` comment lines, skipping blank
    // lines between the definition and the comment block.
    std::vector<std::string> preceding;
    bool found_comment = false;
    for (int i = def_line - 1; i >= 0; --i) {
        std::string prev = doc.get_line(i);
        auto start = prev.find_first_not_of(" \t");
        if (start == std::string::npos) {
            if (found_comment)
                break;
            continue;
        }
        auto trimmed = prev.substr(start);
        if (trimmed.rfind("---", 0) == 0) {
            found_comment = true;
            preceding.push_back(strip_marker(trimmed));
        } else {
            break;
        }
    }

    std::reverse(preceding.begin(), preceding.end());

    std::string result;
    for (auto &line : preceding) {
        if (!result.empty())
            result += "\n";
        result += line;
    }
    if (!same_line_comment.empty()) {
        if (!result.empty())
            result += "\n";
        result += same_line_comment;
    }
    return result;
}

CompletionContext detect_completion_context(const std::string &line_text,
                                            int character) {
    std::string before = line_text.substr(
        0, std::min(static_cast<int>(line_text.size()), character));

    auto trimmed = before;
    auto end = trimmed.find_last_not_of(' ');
    if (end != std::string::npos)
        trimmed = trimmed.substr(0, end + 1);

    // Start of a block definition: one or more '#', a space, then identifier
    // chars (# tag / ## tag / ### tag).
    if (!before.empty() && before[0] == '#') {
        size_t i = 0;
        while (i < before.size() && before[i] == '#')
            ++i;
        if (i <= 3 && i < before.size() && before[i] == ' ') {
            auto after = before.substr(i + 1);
            bool all_id = true;
            for (char c : after) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' &&
                    c != '-') {
                    all_id = false;
                    break;
                }
            }
            if (all_id)
                return CompletionContext::BlockDefinition;
        }
    }

    // After "-> ": transition target (allow dotted/relative chars).
    auto arrow_pos = before.rfind("->");
    if (arrow_pos != std::string::npos) {
        auto after_arrow = before.substr(arrow_pos + 2);
        auto as = after_arrow.find_first_not_of(' ');
        if (as != std::string::npos)
            after_arrow = after_arrow.substr(as);
        bool all_move = true;
        for (char c : after_arrow) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' &&
                c != '.' && c != '-' && c != '^') {
                all_move = false;
                break;
            }
        }
        if (all_move)
            return CompletionContext::MoveTarget;
    }

    // After "GO ": file path completion.
    auto go_pos = before.rfind("GO ");
    if (go_pos != std::string::npos) {
        auto pre_go = before.substr(0, go_pos);
        bool all_space = true;
        for (char c : pre_go)
            if (c != ' ' && c != '\t') {
                all_space = false;
                break;
            }
        if (all_space)
            return CompletionContext::GoPath;
    }

    // After ":": method completion.
    if (!before.empty() && before.back() == ':') {
        return CompletionContext::Method;
    }
    auto colon_pos = before.rfind(':');
    if (colon_pos != std::string::npos) {
        auto after_colon = before.substr(colon_pos + 1);
        bool all_id = true;
        for (char c : after_colon)
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                all_id = false;
                break;
            }
        if (all_id && !after_colon.empty())
            return CompletionContext::Method;
    }

    // After "~ ": variable completion (mutations).
    {
        auto tilde_pos = trimmed.rfind("~ ");
        if (tilde_pos != std::string::npos) {
            auto after_tilde = trimmed.substr(tilde_pos + 2);
            bool all_id = true;
            for (char c : after_tilde)
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                    all_id = false;
                    break;
                }
            if (all_id)
                return CompletionContext::Variable;
        }
    }
    // Inside (? ...) conditional.
    if (before.find("(?") != std::string::npos) {
        return CompletionContext::Variable;
    }
    // Inside { ... } injection.
    auto brace_pos = before.rfind('{');
    if (brace_pos != std::string::npos) {
        auto close = before.find('}', brace_pos);
        if (close == std::string::npos)
            return CompletionContext::Variable;
    }

    return CompletionContext::None;
}

std::vector<LspTypes::CompletionItem>
get_completions(const Document &doc, int line, int character,
                const std::vector<std::string> &workspace_files) {
    std::vector<LspTypes::CompletionItem> items;

    std::string line_text = doc.get_line(line);
    auto ctx = detect_completion_context(line_text, character);
    const Skald::Codex *codex = doc.codex();

    switch (ctx) {
    case CompletionContext::MoveTarget: {
        for (auto &tag : doc.block_tags())
            items.push_back(
                {tag, LspTypes::CompletionItemKind::Reference, "Block tag"});
        break;
    }
    case CompletionContext::Variable: {
        for (auto &var : doc.variable_names())
            items.push_back(
                {var, LspTypes::CompletionItemKind::Variable, "Module variable"});
        if (codex)
            for (auto &g : codex->global_vars)
                items.push_back({g.var.name,
                                 LspTypes::CompletionItemKind::Variable,
                                 "Global"});
        break;
    }
    case CompletionContext::Method: {
        // Methods come from the codex.
        if (codex)
            for (auto &def : codex->method_defs)
                items.push_back({def.name, LspTypes::CompletionItemKind::Method,
                                 def.dbg_desc()});
        break;
    }
    case CompletionContext::GoPath: {
        for (auto &file : workspace_files)
            items.push_back(
                {file, LspTypes::CompletionItemKind::File, "Skald file"});
        break;
    }
    case CompletionContext::BlockDefinition: {
        for (auto &tag : doc.open_transitions())
            items.push_back({tag, LspTypes::CompletionItemKind::Reference,
                             "Open transition (referenced by ->)"});
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

    // Module vars as top-level variable symbols.
    for (auto &mv : mod.module_vars) {
        LspTypes::DocumentSymbol sym;
        sym.name = mv.var.name;
        sym.kind = LspTypes::SymbolKind::Variable;
        for (auto &s : doc.symbols()) {
            if (s.name == mv.var.name && s.kind == SymbolKind::Variable &&
                s.is_definition) {
                sym.range = {{s.range.line, s.range.col},
                             {s.range.line, s.range.end_col}};
                sym.selectionRange = sym.range;
                break;
            }
        }
        symbols.push_back(sym);
    }

    // Blocks as top-level class symbols.
    for (auto &block : mod.blocks) {
        LspTypes::DocumentSymbol block_sym;
        block_sym.name = "#" + block.tag;
        block_sym.kind = LspTypes::SymbolKind::Class;
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

    const Skald::Codex *codex = doc.codex();

    switch (sym->kind) {
    case SymbolKind::BlockTag: {
        auto count_beats = [&](const std::string &tag) -> const Skald::Block * {
            for (auto &block : doc.module().blocks)
                if (block.tag == tag)
                    return &block;
            return nullptr;
        };
        if (!sym->is_definition) {
            auto def = doc.find_definition(sym->name, SymbolKind::BlockTag);
            if (def) {
                if (auto *block = count_beats(sym->name)) {
                    std::string hover =
                        "Block `#" + sym->name + "` — " +
                        std::to_string(block->members.size()) + " member(s)";
                    auto comment = extract_comment(doc, def->range.line);
                    if (!comment.empty())
                        hover += "\n\n---\n\n" + comment;
                    return hover;
                }
            }
            return "Transition target `" + sym->name +
                   "` (not defined in this file)";
        }
        if (auto *block = count_beats(sym->name)) {
            std::string hover = "Block `#" + sym->name + "` — " +
                                std::to_string(block->members.size()) +
                                " member(s)";
            auto comment = extract_comment(doc, sym->range.line);
            if (!comment.empty())
                hover += "\n\n---\n\n" + comment;
            return hover;
        }
        return "Block `#" + sym->name + "`";
    }
    case SymbolKind::Variable: {
        // Determine scope: global (codex), then module (@let), else local.
        if (codex)
            for (auto &g : codex->global_vars)
                if (g.var.name == sym->name)
                    return "Variable `" + sym->name + "` (global) = " +
                           Skald::rval_to_string(g.initial_value);
        for (auto &mv : doc.module().module_vars)
            if (mv.var.name == sym->name) {
                std::string hover = "Variable `" + sym->name + "` (module) = " +
                                    Skald::rval_to_string(mv.initial_value);
                auto def = doc.find_definition(sym->name, SymbolKind::Variable);
                if (def) {
                    auto comment = extract_comment(doc, def->range.line);
                    if (!comment.empty())
                        hover += "\n\n---\n\n" + comment;
                }
                return hover;
            }
        return "Variable `" + sym->name + "` (local)";
    }
    case SymbolKind::Method: {
        if (codex)
            for (auto &def : codex->method_defs)
                if (def.name == sym->name)
                    return "Method `:" + def.dbg_desc() + "`";
        return "Method `:" + sym->name + "()` (not defined in codex)";
    }
    case SymbolKind::FileRef:
        return "Module reference: `" + sym->name + "`";
    case SymbolKind::TestbedSet:
        return "Testbed set: `" + sym->name + "`";
    }

    return std::nullopt;
}

} // namespace SkaldLsp
