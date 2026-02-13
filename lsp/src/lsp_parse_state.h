#pragma once

#include "parse_state.h"
#include <string>
#include <vector>

namespace SkaldLsp {

enum class SymbolKind { BlockTag, Variable, Method, FileRef };

struct SourceRange {
    int line = 0;      // 0-based
    int col = 0;       // 0-based
    int end_col = 0;   // 0-based
};

struct SymbolOccurrence {
    std::string name;
    SymbolKind kind;
    bool is_definition;
    SourceRange range;
};

struct LspParseState : public Skald::ParseState {
    std::vector<SymbolOccurrence> symbols;
    SourceRange last_identifier_range;
    SourceRange mutate_target_range;  // Saved by op_mutate_start for mutation actions
    std::string mutate_target_name;   // Saved by op_mutate_start to avoid rvalue corruption
    std::vector<SourceRange> skipped_lines;  // Lines skipped during error recovery

    LspParseState(const std::string &filename) : ParseState(filename) {}

    void record_symbol(const std::string &name, SymbolKind kind,
                       bool is_definition, const SourceRange &range) {
        symbols.push_back({name, kind, is_definition, range});
    }
};

} // namespace SkaldLsp
