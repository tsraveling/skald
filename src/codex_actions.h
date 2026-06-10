#pragma once

#include "codex_grammar.h"
#include "codex_parse_state.h"
#include "debug.h"

namespace Skald {

template <typename Rule> struct codex_action {};

template <> struct codex_action<globals> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    dbg_out("GLOBALS:\n" << text);
  }
};
template <> struct codex_action<methods> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    dbg_out("METHODS:\n" << text);
  }
};
template <> struct codex_action<method_def> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    dbg_out("- method_def:\n" << text);
  }
};

} // namespace Skald
