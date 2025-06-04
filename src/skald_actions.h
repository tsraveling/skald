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

template <> struct action<beat_attribution> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();

    // Grab everything before the colon
    std::string tag = text.substr(0, text.find(':'));
    state.current_tag = input.string();

    // Trim leading whitespace
    auto start = tag.find_first_not_of(" \t");
    state.current_tag = (start != std::string::npos) ? tag.substr(start) : tag;
  }
};

template <> struct action<beat_text> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    state.add_beat_string(text);
  }
};

template <> struct action<beat_content> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.add_beat();
  }
};

} // namespace Skald
