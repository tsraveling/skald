#pragma once

#include "debug.h"
#include "parse_state.h"
#include "skald.h"
#include "skald_grammar.h"
#include <string>

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
    state.last_identifier = text;
    dbg_out(">>> last_id: " << text);
  }
};

// SECTION: RAW VALUES

template <> struct action<val_bool_true> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.bool_buffer = true;
  }
};
template <> struct action<val_bool_false> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.bool_buffer = false;
  }
};
template <> struct action<string_content> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.string_buffer = input.string();
  }
};

// SECTION: RVALUES AND ARGS

template <> struct action<val_bool> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.rval_buffer = state.bool_buffer;
    dbg_out("<<< val_bool: " << state.bool_buffer);
  }
};
template <> struct action<val_int> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.rval_buffer = std::stoi(input.string());
    dbg_out("<<< val_int: " << input.string());
  }
};
template <> struct action<val_float> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.rval_buffer = std::stof(input.string());
    dbg_out("<<< val_float: " << input.string());
  }
};
template <> struct action<val_string> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.rval_buffer = state.string_buffer;
    dbg_out("<<< val_string: " << state.string_buffer);
  }
};
template <> struct action<r_variable> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.rval_buffer = Variable{input.string()};
    dbg_out("<<< r_variable: " << input.string());
  }
};

template <> struct action<argument> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.argument_queue.push_back(state.rval_buffer);
    dbg_out("<<< argument (adding to queue): " << input.string());
  }
};

// SECTION: TEXT

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

// SECTION: OPERATION LINES

template <> struct action<operation> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    dbg_out(">>> operation: " << text);
  }
};

template <> struct action<op_move> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.operation_queue.push_back(Move{state.pop_id()});
    dbg_out(">>> op_move: " << input.string());
  }
};

template <> struct action<op_method> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.operation_queue.push_back(
        MethodCall{state.pop_id(), std::move(state.argument_queue)});
    dbg_out(">>> op_method: " << input.string());
  }
};

/// MUTATIONS ///

template <> struct action<op_mutate_subtract> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_mutate_subtract: " << input.string());
    state.operation_queue.push_back(
        Mutation{state.pop_id(), Mutation::SUBTRACT, state.rval_buffer});
  }
};
template <> struct action<op_mutate_add> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_mutate_add: " << input.string());
    state.operation_queue.push_back(
        Mutation{state.pop_id(), Mutation::ADD, state.rval_buffer});
  }
};
template <> struct action<op_mutate_equate> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_mutate_equate: " << input.string());
    state.operation_queue.push_back(
        Mutation{state.pop_id(), Mutation::EQUATE, state.rval_buffer});
  }
};
template <> struct action<op_mutate_switch> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.operation_queue.push_back(
        Mutation{state.pop_id(), Mutation::SWITCH, {}});
  }
};

/// OPERATION CORE ///

template <> struct action<op_line> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    dbg_out(">>> op_line: " << text);
  }
};

// SECTION: CHOICES

template <> struct action<inline_choice_move> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    state.operation_queue.push_back(Move{state.pop_id()});
    dbg_out(">>> inline_choice_move: " << text);
  }
};

// FIXME: Delete this later
template <> struct action<choice_line> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> choice_line: " << input.string());
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

// SECTION: BEATS

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

template <> struct action<beat_line> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> beat_line");
    state.add_beat();
  }
};

} // namespace Skald
