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
    state.rval_buffer.push_back(state.bool_buffer);
    dbg_out("<<< rval_buffer -> push_back: val_bool: " << state.bool_buffer);
  }
};
template <> struct action<val_int> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.rval_buffer.push_back(std::stoi(input.string()));
    dbg_out("<<< rval_buffer -> push_back: val_int: " << input.string());
  }
};
template <> struct action<val_float> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.rval_buffer.push_back(std::stof(input.string()));
    dbg_out("<<< rval_buffer -> push_back: val_float: " << input.string());
  }
};
template <> struct action<val_string> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.rval_buffer.push_back(state.string_buffer);
    dbg_out(
        "<<< rval_buffer -> push_back: val_string: " << state.string_buffer);
  }
};
template <> struct action<r_variable> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.rval_buffer.push_back(Variable{input.string()});
    dbg_out("<<< rval_buffer -> push_back: r_variable: " << input.string());
  }
};

template <> struct action<argument> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.argument_queue.push_back(state.rval_buffer_pop());
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

// SECTION: CONDITIONALS
//
template <> struct action<checkable_method> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    // state.operation_queue.push_back(
    //     MethodCall{state.pop_id(), std::move(state.argument_queue)});
    dbg_out(">>> checkable_method: " << input.string());
  }
};
template <> struct action<checkable_not_method> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    // state.operation_queue.push_back(
    //     MethodCall{state.pop_id(), std::move(state.argument_queue)});
    dbg_out(">>> checkable_not_method: " << input.string());
  }
};

// This will grab and store the operation type
template <> struct action<checkable_2f_operator> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out("<.> stored comparitor for " << input.string());
    state.current_comparison =
        ConditionalAtom::comparison_for_operator(input.string());
  }
};

// This specifically checks the non-truthy case
template <> struct action<checkable_not_truthy> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out("<.> checkable_not_truthy: " << input.string());
    state.current_comparison = ConditionalAtom::Comparison::NOT_TRUTHY;
  }
};
// This assembles the base checkables (aka direct checks, not subclauses)
template <> struct action<checkable_base> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> base: " << input.string());
    RValue left;
    std::optional<RValue> right = {};
    if (state.current_comparison == ConditionalAtom::Comparison::TRUTHY ||
        state.current_comparison == ConditionalAtom::Comparison::NOT_TRUTHY) {
      // These comparitor types only have the one rval
      left = state.rval_buffer_pop();
    } else {
      // Grab the rvals in order, first right (most recent) then left
      right = state.rval_buffer_pop();
      left = state.rval_buffer_pop();
    }
    state.add_conditional_atom(
        ConditionalAtom{left, state.current_comparison, right});
    state.current_comparison = ConditionalAtom::TRUTHY;
  }
};
template <> struct action<checkable_or_tail> {
  static void apply0(ParseState &state) {
    dbg_out("||| checkable_or_tail: SET TO OR");
    state.conditional_stack.back().type = Conditional::OR;
  }
};
template <> struct action<subclause_opener> {
  static void apply0(ParseState &state) {
    dbg_out(">>> checkable_opener");
    state.conditional_step_in();
  }
};
template <> struct action<subclause_closer> {
  static void apply0(ParseState &state) {
    dbg_out(">>> checkable_closer");
    state.conditional_step_out();
  }
};
template <> struct action<conditional_opener> {
  static void apply0(ParseState &state) {
    dbg_out(">>> cond_open");
    state.conditional_step_in();
  }
};
template <> struct action<conditional_closer> {
  static void apply0(ParseState &state) {
    dbg_out(">>> cond_close");
    state.conditional_step_out();
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
        Mutation{state.pop_id(), Mutation::SUBTRACT, state.rval_buffer_pop()});
  }
};
template <> struct action<op_mutate_add> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_mutate_add: " << input.string());
    state.operation_queue.push_back(
        Mutation{state.pop_id(), Mutation::ADD, state.rval_buffer_pop()});
  }
};
template <> struct action<op_mutate_equate> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_mutate_equate: " << input.string());
    state.operation_queue.push_back(
        Mutation{state.pop_id(), Mutation::EQUATE, state.rval_buffer_pop()});
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
