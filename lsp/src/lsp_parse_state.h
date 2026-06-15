#pragma once

#include "parse_state.h"
#include "skald.h"
#include <string>
#include <vector>

namespace SkaldLsp {

enum class SymbolKind { BlockTag, Variable, Method, FileRef, TestbedSet };

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
    // For methods only: was this call used as an rvalue / in a conditional
    // (true) or inline as a bare operation (false). Used to flag action-typed
    // methods used where a value is expected.
    bool is_rvalue = false;
    // For variables only: is this occurrence an assignment target (a mutation
    // lvalue, `~ name = …`)? The topmost assignment defines a local variable.
    bool is_assignment = false;
};

struct LspParseState : public Skald::ParseState {
    std::vector<SymbolOccurrence> symbols;
    SourceRange last_identifier_range;
    SourceRange move_expr_range;      // Range of the last move identifier expr
    SourceRange mutate_target_range;  // Saved by op_mutate_start for mutations
    std::string mutate_target_name;   // Saved by op_mutate_start

    LspParseState(const std::string &filename, const Skald::Codex *codex)
        : ParseState(filename, codex) {
        // These flags live in the base ParseState and are left uninitialized
        // by its constructor; garbage values produce spurious "default value
        // and type do not match" errors on the first declaration. Zero them.
        declaration_was_typed = false;
        declaration_was_valued = false;
        last_type = Skald::ValueType::STRING;
    }

    void record_symbol(const std::string &name, SymbolKind kind,
                       bool is_definition, const SourceRange &range,
                       bool is_rvalue = false, bool is_assignment = false) {
        symbols.push_back(
            {name, kind, is_definition, range, is_rvalue, is_assignment});
    }
};

} // namespace SkaldLsp
