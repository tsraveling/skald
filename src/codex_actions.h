#pragma once

#include "codex_grammar.h"
#include "codex_parse_state.h"
#include "debug.h"

namespace Skald {

template <typename Rule> struct codex_action {};

// SECTION: ABSTRACTION

template <> struct codex_action<identifier> {
  template <typename CodexActionInput>
  static void apply(const CodexActionInput &input, CodexParseState &state) {
    auto text = input.string();
    state.last_identifier = text;
  }
};

// SECTION: TYPES

template <> struct codex_action<type_int> {
  static void apply0(CodexParseState &state) {
    state.last_type = ValueType::INT;
  }
};
template <> struct codex_action<type_float> {
  static void apply0(CodexParseState &state) {
    state.last_type = ValueType::FLOAT;
  }
};
template <> struct codex_action<type_bool> {
  static void apply0(CodexParseState &state) {
    state.last_type = ValueType::BOOL;
  }
};
template <> struct codex_action<type_string> {
  static void apply0(CodexParseState &state) {
    state.last_type = ValueType::STRING;
  }
};

// SECTION: SIMPLE RVALUES
// These are used only for global state defaults.

template <> struct codex_action<string_content> {
  template <typename CodexActionInput>
  static void apply(const CodexActionInput &input, CodexParseState &state) {
    state.string_buffer = input.string();
  }
};

template <> struct codex_action<val_bool> {
  template <typename CodexActionInput>
  static void apply(const CodexActionInput &input, CodexParseState &state) {
    state.rval_buffer.push_back(input.string() == "true");
  }
};
template <> struct codex_action<val_int> {
  template <typename CodexActionInput>
  static void apply(const CodexActionInput &input, CodexParseState &state) {
    state.rval_buffer.push_back(std::stoi(input.string()));
  }
};
template <> struct codex_action<val_float> {
  template <typename CodexActionInput>
  static void apply(const CodexActionInput &input, CodexParseState &state) {
    state.rval_buffer.push_back(std::stof(input.string()));
  }
};
template <> struct codex_action<val_string> {
  template <typename CodexActionInput>
  static void apply(const CodexActionInput &input, CodexParseState &state) {
    state.rval_buffer.push_back(state.string_buffer);
  }
};

template <> struct codex_action<globals> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, CodexParseState &state) {
    auto text = input.string();
    dbg_out("GLOBALS:\n" << text);
  }
};
template <> struct codex_action<methods> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, CodexParseState &state) {
    auto text = input.string();
    dbg_out("METHODS:\n" << text);
  }
};
template <> struct codex_action<method_def> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, CodexParseState &state) {
    auto text = input.string();
    dbg_out("- method_def:\n" << text);
  }
};

/// Declarations ///

template <> struct codex_action<declaration_default> {
  static void apply0(CodexParseState &state) {
    state.declaration_was_valued = true;
  }
};
template <> struct codex_action<declaration_type> {
  static void apply0(CodexParseState &state) {
    state.declaration_was_typed = true;
  }
};
template <> struct codex_action<declaration> {
  template <typename CodexActionInput>
  static void apply(const CodexActionInput &input, CodexParseState &state) {

    // Rule: Must *either* be typed or valued (or both)
    if (!state.declaration_was_valued && !state.declaration_was_typed) {
      state.err(
          input.position(),
          "Declaration must have either a type or a default value (or both).");
      return;
    }

    ValueType t;
    SimpleRValue v;
    if (state.declaration_was_valued) {
      v = state.simple_rval_buffer_pop(); // grab default and get value from it
      t = srval_get_type(v);
      if (state.declaration_was_typed) {
        if (t != state.last_type) {
          state.err(input.position(), "Default value and type do not match");
          return;
        }
      }
    }
    if (state.declaration_was_typed) {
      t = state.last_type; // grab strong type
    }
    auto n = state.pop_id(); // grab var name
    auto var = Variable{.name = n, .type = t};

    // Add to stack
    state.codex.global_vars.push_back(
        DeclaredVar{.initial_value = v, .var = var});

    // Cleanup
    state.declaration_was_typed = false;
    state.declaration_was_valued = false;
  }
};

} // namespace Skald
