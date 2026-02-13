#pragma once

#include "lsp_parse_state.h"
#include "skald_actions.h"
#include "skald_grammar.h"

namespace SkaldLsp {

// Helper: compute a SourceRange from a PEGTL action input
template <typename ActionInput>
SourceRange range_from_input(const ActionInput &input) {
    auto pos = input.position();
    return {static_cast<int>(pos.line) - 1, // PEGTL 1-based -> LSP 0-based
            static_cast<int>(pos.column) - 1,
            static_cast<int>(pos.column) - 1 + static_cast<int>(input.size())};
}

// Default: inherit base action (does nothing if base has no specialization)
template <typename Rule> struct lsp_action : Skald::action<Rule> {};

// identifier: save position on every fire
// NOTE: This fires for direct `identifier` matches but NOT when matched
// through inheriting rules like block_tag_name, variable_name, etc.
template <> struct lsp_action<Skald::identifier> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        state.last_identifier_range = range_from_input(input);
        Skald::action<Skald::identifier>::apply(input, state);
    }
};

// variable_name: alias for identifier, also set last_identifier_range
template <> struct lsp_action<Skald::variable_name> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        // variable_name inherits from identifier but action<identifier> won't fire.
        // We need to manually set last_identifier and last_identifier_range.
        state.last_identifier_range = range_from_input(input);
        state.last_identifier = input.string();
    }
};

// block_tag_name: definition of a block tag (#tag)
template <> struct lsp_action<Skald::block_tag_name> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        auto name = input.string();
        auto range = range_from_input(input);
        state.last_identifier_range = range;
        state.record_symbol(name, SymbolKind::BlockTag, true, range);
        Skald::action<Skald::block_tag_name>::apply(input, state);
    }
};

// op_move: reference to a block tag (-> tag)
// The identifier inside op_move is matched directly as `identifier`, so
// last_identifier_range is set by lsp_action<identifier>.
template <> struct lsp_action<Skald::op_move> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        auto name = state.last_identifier;
        state.record_symbol(name, SymbolKind::BlockTag, false,
                            state.last_identifier_range);
        Skald::action<Skald::op_move>::apply(input, state);
    }
};

// inline_choice_move: reference to a block tag in choice (-> tag)
template <> struct lsp_action<Skald::inline_choice_move> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        auto name = state.last_identifier;
        state.record_symbol(name, SymbolKind::BlockTag, false,
                            state.last_identifier_range);
        Skald::action<Skald::inline_choice_move>::apply(input, state);
    }
};

// declaration_line: variable definition (~ var = ... or < var = ...)
// The identifier inside is matched through variable_name (not directly),
// but we handle variable_name above to set last_identifier_range.
template <> struct lsp_action<Skald::declaration_line> {
    static void apply0(LspParseState &state) {
        auto name = state.last_identifier;
        state.record_symbol(name, SymbolKind::Variable, true,
                            state.last_identifier_range);
        Skald::action<Skald::declaration_line>::apply0(state);
    }
};

// r_variable: variable reference (inherits from variable_name : identifier)
// PEGTL fires action<r_variable> but NOT action<variable_name> or
// action<identifier>.
template <> struct lsp_action<Skald::r_variable> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        auto name = input.string();
        auto range = range_from_input(input);
        state.last_identifier_range = range;
        state.record_symbol(name, SymbolKind::Variable, false, range);
        Skald::action<Skald::r_variable>::apply(input, state);
    }
};

// op_mutate_start: save the mutation target identifier name and range before
// rvalue parsing can overwrite last_identifier/last_identifier_range
template <> struct lsp_action<Skald::op_mutate_start> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        state.mutate_target_name = state.last_identifier;
        state.mutate_target_range = state.last_identifier_range;
    }
};

// op_mutate_equate: variable mutation target (~ var = ...)
template <> struct lsp_action<Skald::op_mutate_equate> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        auto name = state.mutate_target_name;
        state.record_symbol(name, SymbolKind::Variable, false,
                            state.mutate_target_range);
        Skald::action<Skald::op_mutate_equate>::apply(input, state);
    }
};

// op_mutate_switch: variable mutation (~ var =!)
template <> struct lsp_action<Skald::op_mutate_switch> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        auto name = state.mutate_target_name;
        state.record_symbol(name, SymbolKind::Variable, false,
                            state.mutate_target_range);
        Skald::action<Skald::op_mutate_switch>::apply(input, state);
    }
};

// op_mutate_add: variable mutation (~ var += ...)
template <> struct lsp_action<Skald::op_mutate_add> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        auto name = state.mutate_target_name;
        state.record_symbol(name, SymbolKind::Variable, false,
                            state.mutate_target_range);
        Skald::action<Skald::op_mutate_add>::apply(input, state);
    }
};

// op_mutate_subtract: variable mutation (~ var -= ...)
template <> struct lsp_action<Skald::op_mutate_subtract> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        auto name = state.mutate_target_name;
        state.record_symbol(name, SymbolKind::Variable, false,
                            state.mutate_target_range);
        Skald::action<Skald::op_mutate_subtract>::apply(input, state);
    }
};

// op_method: method call as operation (:method(...))
// The identifier inside is matched directly.
template <> struct lsp_action<Skald::op_method> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        auto name = state.last_identifier;
        state.record_symbol(name, SymbolKind::Method, false,
                            state.last_identifier_range);
        Skald::action<Skald::op_method>::apply(input, state);
    }
};

// r_method: method call as rvalue (:method(...))
template <> struct lsp_action<Skald::r_method> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        auto name = state.last_identifier;
        state.record_symbol(name, SymbolKind::Method, false,
                            state.last_identifier_range);
        Skald::action<Skald::r_method>::apply(input, state);
    }
};

// module_path: file reference in GO commands
template <> struct lsp_action<Skald::module_path> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        auto name = input.string();
        // Trim trailing whitespace from module_path
        auto end = name.find_last_not_of(" \t\r\n");
        if (end != std::string::npos)
            name = name.substr(0, end + 1);
        auto pos = input.position();
        SourceRange range{static_cast<int>(pos.line) - 1,
                          static_cast<int>(pos.column) - 1,
                          static_cast<int>(pos.column) - 1 +
                              static_cast<int>(name.size())};
        state.record_symbol(name, SymbolKind::FileRef, false, range);
        Skald::action<Skald::module_path>::apply(input, state);
    }
};

// skip_line: error recovery — record skipped lines for diagnostics
template <> struct lsp_action<Skald::skip_line> {
    template <typename ActionInput>
    static void apply(const ActionInput &input, LspParseState &state) {
        auto range = range_from_input(input);
        state.skipped_lines.push_back(range);
    }
};

} // namespace SkaldLsp
