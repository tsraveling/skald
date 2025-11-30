#pragma once

#include "debug.h"
#include "parse_state.h"
#include "skald.h"
#include "skald_grammar.h"
#include <optional>
#include <string>
#include <vector>

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

// SECTION: TESTBEDS

template <> struct action<testbed_open> {
  static void apply0(ParseState &state) {
    dbg_out("<<< testbed_open");
    state.module.testbeds.push_back(Testbed{.name = state.pop_id()});
  }
};
template <> struct action<testbed_declaration> {
  static void apply0(ParseState &state) {
    dbg_out("<<< testbed_dec");
    // This code assumes that a testbed has been opened, and that only simple
    // rvalues will come through; this is grammar-enforced which is fortunate,
    // because otherwise this would crash quite badly!
    auto val = *cast_rval_to_simple(state.rval_buffer_pop());
    state.module.testbeds.back().declarations.push_back(
        TestbedDeclaration{.variable = state.pop_id(), .test_value = val});
  }
};
template <> struct action<rvalue_testbed> {
  static void apply0(ParseState &state) { dbg_out("<<< testbed_rval"); }
};
template <> struct action<testbed_closed> {
  static void apply0(ParseState &state) { dbg_out("<<< testbed_closed"); }
};
template <> struct action<testbed> {
  static void apply0(ParseState &state) { dbg_out("<<< testbed"); }
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
template <> struct action<r_method> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto method_call = std::make_shared<MethodCall>(MethodCall{
        .method = state.pop_id(), .args = std::move(state.argument_queue)});
    state.rval_buffer.push_back(method_call);
    dbg_out(
        "<<< rval_buffer -> push_back: r_method: " << method_call->dbg_desc());
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

template <> struct action<injectable_rvalue> {
  static void apply0(ParseState &state) {
    dbg_out(">-+ injectable_rvalue: ");

    state.injectable_buffer = state.rval_buffer_pop();
  }
};

template <> struct action<switch_option> {
  static void apply0(ParseState &state) {
    auto val = state.rval_buffer_pop();
    auto check = state.rval_buffer_pop();
    state.ternary_option_queue.push_back(TernaryOption{check, val});
    dbg_out(">>> switch_option committed.");
  }
};

template <> struct action<switch_tail> {
  static void apply0(ParseState &state) {
    state.text_content_queue.push_back(
        TernaryInsertion{.check = state.injectable_buffer_pop(),
                         .options = std::move(state.ternary_option_queue)});
    dbg_out(">>> switch_tail committed.");
  }
};

template <> struct action<ternary_tail> {
  static void apply0(ParseState &state) {
    auto false_val = state.rval_buffer_pop();
    auto true_val = state.rval_buffer_pop();
    state.text_content_queue.push_back(
        TernaryInsertion{.check = state.injectable_buffer_pop(),
                         .options = {TernaryOption{true, true_val},
                                     TernaryOption{false, false_val}}});
    dbg_out(">>> ternary_tail committed.");
  }
};

template <> struct action<injectable> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    dbg_out(">>> injectable: " << text);
    // This will only be filled if it wasn't a switch or ternary
    if (state.injectable_buffer) {
      dbg_out(("  --> saving as simple insertion!"));
      state.text_content_queue.push_back(
          SimpleInsertion{.rvalue = state.injectable_buffer_pop()});
    }
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

// SECTION: CONDITIONALS

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

template <> struct action<module_path> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> module_path: " << input.string() << " (stored in buffer)");
    state.path_buffer = input.string();
  }
};

// This checks if the currently processed GO line has a start tag on the end
template <> struct action<op_go_start_tag> {
  static void apply0(ParseState &state) { state.does_go_have_start_tag = true; }
};

template <> struct action<op_go> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_go: " << input.string() << " (pushing onto queue)");
    std::string start_tag = state.does_go_have_start_tag ? state.pop_id() : "";
    dbg_out(" - >>> start_tag: " << start_tag);
    state.operation_queue.push_back(
        GoModule{.module_path = state.path_buffer, .start_in_tag = start_tag});
  }
};

template <> struct action<op_exit> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_exit: " << input.string() << " (pushing onto queue)");
    state.operation_queue.push_back(Exit{.argument = state.rval_buffer_pop()});
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

/// SECTION: DECLARATIONS ///

template <> struct action<declaration_initial> {
  static void apply0(ParseState &state) {
    state.last_declaration_was_import = false;
  }
};
template <> struct action<declaration_import> {
  static void apply0(ParseState &state) {
    state.last_declaration_was_import = true;
  }
};
template <> struct action<declaration_line> {
  static void apply0(ParseState &state) {
    state.module.declarations.push_back(
        Declaration{.var = {state.pop_id()},
                    .initial_value = state.simple_rval_buffer_pop(),
                    .is_imported = state.last_declaration_was_import});
  }
};

/// SECTION: MUTATIONS ///

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

template <> struct action<logic_beat_single> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> logic_beat_single: " << input.string());
    state.add_logic_beat();
  }
};

template <> struct action<logic_beat_conditional> {
  static void apply0(ParseState &state) {
    dbg_out(">>> logic_beat_conditional");
  }
};

template <> struct action<logic_beat_else> {
  static void apply0(ParseState &state) {
    dbg_out(">>> logic_beat_else");
    state.store_is_else = true;
  }
};

template <> struct action<logic_beat_clause> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> logic_beat_clause:\n" << input.string());
    state.add_logic_beat();
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

template <> struct action<beat_clause> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> beat_line");
    state.add_beat();
  }
};

} // namespace Skald
