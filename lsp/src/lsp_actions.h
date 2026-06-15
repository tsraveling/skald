#pragma once

#include "lsp_parse_state.h"
#include "skald_actions.h"
#include "skald_grammar.h"
#include <cctype>

namespace SkaldLsp {

// Helper: compute a SourceRange from a PEGTL action input
template <typename ActionInput>
SourceRange range_from_input(const ActionInput &input) {
  auto pos = input.position();
  return {static_cast<int>(pos.line) - 1, // PEGTL 1-based -> LSP 0-based
          static_cast<int>(pos.column) - 1,
          static_cast<int>(pos.column) - 1 + static_cast<int>(input.size())};
}

// Helper: a line-level rule (declaration, testbed_set) begins with optional
// indent then an identifier. Pull out that leading identifier's name and a
// tight range over it.
template <typename ActionInput>
static std::pair<std::string, SourceRange>
leading_identifier(const ActionInput &input) {
  std::string s = input.string();
  auto pos = input.position();
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
    ++i;
  size_t start = i;
  while (i < s.size() &&
         (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_'))
    ++i;
  std::string name = s.substr(start, i - start);
  SourceRange range{static_cast<int>(pos.line) - 1,
                    static_cast<int>(pos.column) - 1 + static_cast<int>(start),
                    static_cast<int>(pos.column) - 1 + static_cast<int>(i)};
  return {name, range};
}

// Default: inherit base action (does nothing if base has no specialization)
template <typename Rule> struct lsp_action : Skald::action<Rule> {};

// identifier: save position on every fire
template <> struct lsp_action<Skald::identifier> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    state.last_identifier_range = range_from_input(input);
    Skald::action<Skald::identifier>::apply(input, state);
  }
};

// variable_name: alias for identifier; base action<identifier> won't fire.
template <> struct lsp_action<Skald::variable_name> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    state.last_identifier_range = range_from_input(input);
    state.last_identifier = input.string();
  }
};

// block_tag_name: definition of a block tag. The library builds the full
// dotted path (parent.child.grandchild) and pushes the block; we record the
// dotted name with a range over just the leaf identifier.
template <> struct lsp_action<Skald::block_tag_name> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    auto range = range_from_input(input);
    state.last_identifier_range = range;
    size_t before = state.module.blocks.size();
    Skald::action<Skald::block_tag_name>::apply(input, state);
    // Only record if the library actually started a block (no error).
    if (state.module.blocks.size() > before) {
      state.record_symbol(state.module.blocks.back().tag, SymbolKind::BlockTag,
                          true, range);
    }
  }
};

// move_identifier_full / move_identifier_short: capture the range of the whole
// move expression. The library resolves it into an absolute dotted tag stored
// in state.move_identifier_store.
template <> struct lsp_action<Skald::move_identifier_full> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    state.move_expr_range = range_from_input(input);
    Skald::action<Skald::move_identifier_full>::apply(input, state);
  }
};
template <> struct lsp_action<Skald::move_identifier_short> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    state.move_expr_range = range_from_input(input);
    Skald::action<Skald::move_identifier_short>::apply(input, state);
  }
};

// op_move: reference to a block tag (-> target). Records the resolved absolute
// dotted tag the library computed. Fires for inline_choice_move too (it is an
// op_move), so we do not specialize that separately.
template <> struct lsp_action<Skald::op_move> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    Skald::action<Skald::op_move>::apply(input, state);
    state.record_symbol(state.move_identifier_store, SymbolKind::BlockTag, false,
                        state.move_expr_range);
  }
};

// r_variable: variable reference (rvalue / conditional / injection)
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

// declaration: a module var definition inside an @let block.
template <> struct lsp_action<Skald::declaration> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    Skald::action<Skald::declaration>::apply(input, state);
    auto [name, range] = leading_identifier(input);
    if (!name.empty())
      state.record_symbol(name, SymbolKind::Variable, true, range);
  }
};

// testbed_set: an `identifier = value` line inside a @testbed block.
template <> struct lsp_action<Skald::testbed_set> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    Skald::action<Skald::testbed_set>::apply(input, state);
    auto [name, range] = leading_identifier(input);
    if (!name.empty())
      state.record_symbol(name, SymbolKind::TestbedSet, false, range);
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

// op_mutate_*: variable mutation target reference. A mutation is also an
// assignment (it sets the variable), so flag it — the topmost assignment is
// treated as a local variable's definition.
template <> struct lsp_action<Skald::op_mutate_equate> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    state.record_symbol(state.mutate_target_name, SymbolKind::Variable, false,
                        state.mutate_target_range, false, /*is_assignment=*/true);
    Skald::action<Skald::op_mutate_equate>::apply(input, state);
  }
};
template <> struct lsp_action<Skald::op_mutate_switch> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    state.record_symbol(state.mutate_target_name, SymbolKind::Variable, false,
                        state.mutate_target_range, false, /*is_assignment=*/true);
    Skald::action<Skald::op_mutate_switch>::apply(input, state);
  }
};
template <> struct lsp_action<Skald::op_mutate_add> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    state.record_symbol(state.mutate_target_name, SymbolKind::Variable, false,
                        state.mutate_target_range, false, /*is_assignment=*/true);
    Skald::action<Skald::op_mutate_add>::apply(input, state);
  }
};
template <> struct lsp_action<Skald::op_mutate_subtract> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    state.record_symbol(state.mutate_target_name, SymbolKind::Variable, false,
                        state.mutate_target_range, false, /*is_assignment=*/true);
    Skald::action<Skald::op_mutate_subtract>::apply(input, state);
  }
};

// op_method: method call as an inline operation (:method(...))
template <> struct lsp_action<Skald::op_method> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    auto name = state.last_identifier;
    state.record_symbol(name, SymbolKind::Method, false,
                        state.last_identifier_range, /*is_rvalue=*/false);
    Skald::action<Skald::op_method>::apply(input, state);
  }
};

// r_method: method call used as an rvalue / conditional (:method(...))
template <> struct lsp_action<Skald::r_method> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, LspParseState &state) {
    auto name = state.last_identifier;
    state.record_symbol(name, SymbolKind::Method, false,
                        state.last_identifier_range, /*is_rvalue=*/true);
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
    SourceRange range{
        static_cast<int>(pos.line) - 1, static_cast<int>(pos.column) - 1,
        static_cast<int>(pos.column) - 1 + static_cast<int>(name.size())};
    state.record_symbol(name, SymbolKind::FileRef, false, range);
    Skald::action<Skald::module_path>::apply(input, state);
  }
};

} // namespace SkaldLsp
