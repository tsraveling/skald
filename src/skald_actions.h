#pragma once

#include "debug.h"
#include "parse_state.h"
#include "skald_grammar.h"

namespace Skald {

template <typename Rule> struct action {};

template <> struct action<block_tag_name> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto tag = input.string();
    state.start_block(tag);
  }
};

template <> struct action<identifier> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    state.last_identfier = text;
    dbg_out(">>> last_id: " << text);
  }
};

template <> struct action<inline_choice_move> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();

    dbg_out(">>> inline_choice_move: " << text);
  }
};

template <> struct action<beat_attribution> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();

    // Grab everything before the colon
    std::string tag = text.substr(0, text.find(':'));

    // Trim leading whitespace
    auto start = tag.find_first_not_of(" \t");
    state.current_tag = (start != std::string::npos) ? tag.substr(start) : tag;
  }
};

template <> struct action<inline_text_segment> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    dbg_out(">>> inline_text_segment: " << text);
    state.add_text_string(text);
  }
};

template <> struct action<text_content> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> text_content");
    // state.add_beat();
  }
};

template <> struct action<beat_line> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> beat_line");
    state.add_beat();
  }
};

template <> struct action<choice_clause> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> choice_clause: " << input.string());
    state.add_choice();
  }
};

template <> struct action<choice_block> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> choice block: " << input.string());
    // state.add_beat();
  }
};

// template <> struct action<choic

} // namespace Skald
