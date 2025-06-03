#pragma once

#include "parse_state.h"
#include "skald_grammar.h"
#include <iostream>

namespace Skald {

template <typename Rule> struct action {};

template <> struct action<block_tag_name> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto tag = input.string();
    state.start_block(tag);
  }
};

// STUB: Next, set up a `current_beat_text` in state and switch to processing
// each part individually. In fact, switch beats to have an array of parts
// instead because we will need that for the injection system. Refer to this
// Claude thread for details:
// https://claude.ai/chat/28311d9b-bf70-462f-8cb1-100251ca1058

template <> struct action<beat_content> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    state.add_beat(text);
  }
};

} // namespace Skald
