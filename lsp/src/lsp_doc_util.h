#pragma once

// Shared helpers used across the LSP: ParseError -> Diagnostic conversion and
// `---` doc-comment extraction. These previously lived as near-identical copies
// in document.cpp / codex_cache.cpp (diagnostics) and analyzer.cpp /
// project_index.cpp (doc comments); keep them here so the comment syntax and
// position-clamping rules only have to change in one place.

#include "lsp_types.h"
#include "skald.h"
#include <string>

namespace SkaldLsp {

// Map a library ParseError (1-based position) to an LSP diagnostic, clamping
// non-positive line/column to 0 so a missing position never yields a negative
// (invalid) range.
inline LspTypes::Diagnostic to_diagnostic(const Skald::ParseError &err) {
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

// Hover doc for a symbol defined at 0-based `def_line`: the trailing `---`
// comment on the definition line, plus the contiguous block of `---` comment
// lines immediately above it (a blank line breaks the chain). `get_line(i)`
// returns the raw text of 0-based line i, or an empty string when out of range.
template <typename GetLine>
inline std::string extract_doc_comment(int def_line, GetLine &&get_line) {
    auto strip_marker = [](const std::string &trimmed) -> std::string {
        // `trimmed` is leading-whitespace-stripped and starts with "---".
        auto content = trimmed.substr(3);
        auto cs = content.find_first_not_of(" \t");
        return cs != std::string::npos ? content.substr(cs) : std::string{};
    };

    std::string trailing;
    if (def_line >= 0) {
        std::string def_text = get_line(def_line);
        auto pos = def_text.find("---");
        if (pos != std::string::npos)
            trailing = strip_marker(def_text.substr(pos));
    }

    std::vector<std::string> preceding;
    bool found = false;
    for (int i = def_line - 1; i >= 0; --i) {
        std::string prev = get_line(i);
        auto s = prev.find_first_not_of(" \t");
        if (s == std::string::npos) {
            if (found)
                break; // blank line ends the comment block
            continue;  // blank line between def and comment: keep looking
        }
        auto trimmed = prev.substr(s);
        if (trimmed.rfind("---", 0) == 0) {
            found = true;
            preceding.push_back(strip_marker(trimmed));
        } else {
            break;
        }
    }

    std::string result;
    for (auto it = preceding.rbegin(); it != preceding.rend(); ++it) {
        if (!result.empty())
            result += "\n";
        result += *it;
    }
    if (!trailing.empty()) {
        if (!result.empty())
            result += "\n";
        result += trailing;
    }
    return result;
}

} // namespace SkaldLsp
