#pragma once

#include "debug.h"
#include "parse_state.h"
#include "skald.h"
#include "skald_grammar.h"
#include "tao/pegtl/position.hpp"
#include <optional>
#include <string>
#include <vector>

namespace Skald {

template <typename Rule> struct action {};

// SECTION: BLOCK TAGS

// STUB: Store tag level here
template <> struct action<block1_prefix> {
  static void apply0(ParseState &state) { dbg_out(">>> #"); }
};

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

// SECTION: TYPES

template <> struct action<type_int> {
  static void apply0(ParseState &state) { state.last_type = ValueType::INT; }
};
template <> struct action<type_float> {
  static void apply0(ParseState &state) { state.last_type = ValueType::FLOAT; }
};
template <> struct action<type_bool> {
  static void apply0(ParseState &state) { state.last_type = ValueType::BOOL; }
};
template <> struct action<type_string> {
  static void apply0(ParseState &state) { state.last_type = ValueType::STRING; }
};

// SECTION: TOP MATTER

/// Testbeds ///

// @testbed id <-- grabs id as testbed name
template <> struct action<testbed_open> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    if (state.top_matter_section != ParseState::TopMatterSection::NONE) {
      state.err(input.pos, "Tried to open a testbed but another top matter "
                           "section was already open.");
    }
    state.module.testbeds.push_back(Testbed{.name = state.pop_id()});
    state.top_matter_section = ParseState::TopMatterSection::TESTBED;
  }
};

template <> struct action<testbed_closed> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    if (state.top_matter_section != ParseState::TopMatterSection::TESTBED) {
      state.err(input.pos, "Got a testbed end, but no testbed was open!");
    }
    state.top_matter_section = ParseState::TopMatterSection::NONE;
  }
};

template <> struct action<testbed_set> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    if (state.top_matter_section != ParseState::TopMatterSection::TESTBED) {
      state.err(input.pos, "Got a testbed set, but no testbed was open!");
      return;
    }
    auto val = *cast_rval_to_simple(state.rval_buffer_pop());
    state.module.testbeds.back().declarations.push_back(
        TestbedSet{.variable = state.pop_id(), .test_value = val});
  }
};

template <> struct action<testbed> {
  static void apply0(ParseState &state) { dbg_out("<<< testbed"); }
};

/// Let Clauses ///

template <> struct action<let_open> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    if (state.top_matter_section != ParseState::TopMatterSection::NONE) {
      state.err(input.pos, "Tried to open a let clause but another top matter "
                           "section was already open.");
    }
    state.top_matter_section = ParseState::TopMatterSection::LET;
  }
};

template <> struct action<let_close> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    if (state.top_matter_section != ParseState::TopMatterSection::LET) {
      state.err(input.pos, "Got a let end, but no let clause was open!");
    }
    state.top_matter_section = ParseState::TopMatterSection::NONE;
  }
};

template <> struct action<let> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out("<<< let clause");
    if (state.module.module_vars.size() > 0) {
      state.err(
          input.pos,
          "Got a second let clause; a given module should only have one.");
    }
    state.module.module_vars = std::move(state.module_vars_stack);
  }
};

/// Declarations ///

template <> struct action<declaration> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {

    // Rule: Must *either* be typed or valued (or both)
    if (!state.declaration_was_valued || !state.declaration_was_typed) {
      state.err(
          input.pos,
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
          state.err(input.pos, "Default value and type do not match");
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
    state.module_vars_stack.push_back(
        ModuleVar{.initial_value = v, .var = var});

    // Cleanup
    state.declaration_was_typed = false;
    state.declaration_was_valued = false;
  }
};

// STUB: Add declarations stack and then use it to populate the let clause above

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
                         .check_truthy = true,
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
    std::optional<RValue> arg = std::nullopt;
    if (state.rval_buffer.size() > 0)
      arg = state.rval_buffer_pop();
    state.operation_queue.push_back(Exit{.argument = arg});
  }
};

template <> struct action<op_move> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.operation_queue.push_back(
        Move{input.position().line, state.pop_id()});
    dbg_out(">>> op_move: " << input.string());
  }
};

template <> struct action<op_method> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.operation_queue.push_back(
        MethodCall{input.position().line, state.pop_id(),
                   std::move(state.argument_queue)});
    dbg_out(">>> op_method: " << input.string());
  }
};

/// SECTION: MUTATIONS ///

template <> struct action<op_mutate_subtract> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_mutate_subtract: " << input.string());
    state.operation_queue.push_back(Mutation{input.position().line,
                                             state.pop_id(), Mutation::SUBTRACT,
                                             state.rval_buffer_pop()});
  }
};
template <> struct action<op_mutate_add> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_mutate_add: " << input.string());
    state.operation_queue.push_back(Mutation{input.position().line,
                                             state.pop_id(), Mutation::ADD,
                                             state.rval_buffer_pop()});
  }
};
template <> struct action<op_mutate_equate> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_mutate_equate: " << input.string());
    state.operation_queue.push_back(Mutation{input.position().line,
                                             state.pop_id(), Mutation::EQUATE,
                                             state.rval_buffer_pop()});
  }
};
template <> struct action<op_mutate_switch> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.operation_queue.push_back(
        Mutation{input.position().line, state.pop_id(), Mutation::SWITCH, {}});
  }
};

/// OPERATION CORE ///

template <> struct action<op_line> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    dbg_out(">>> op_line: " << text);
    // STUB: Formulate and add to module op
  }
};

// SECTION: CHOICES

template <> struct action<inline_choice_move> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    state.operation_queue.push_back(
        Move{input.position().line, state.pop_id()});
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
    // STUB: Build ChoiceGroup here
    dbg_out(">>> choice block: " << input.string());
    // state.add_beat();
  }
};

// SECTION: BEATS

template <> struct action<logic_beat_single> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> logic_beat_single: " << input.string());
    auto *beat = state.add_logic_beat();
    beat->line_number = input.position().line;
  }
};

// template <> struct action<logic_beat_conditional> {
//   static void apply0(ParseState &state) {
//     dbg_out(">>> logic_beat_conditional");
//   }
// };

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
    auto *beat = state.add_logic_beat();
    beat->line_number = input.position().line;
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

template <> struct action<beat_line> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    const position p = input.position();
    dbg_out("--> " << p << ": beat line:\n > " << input.string());
    state.store_beat_text();
  }
};

template <> struct action<beat_clause> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    const position p = input.position();
    dbg_out("+++ " << p << ": BEAT CLAUSE END:\n" << input.string());
    auto *beat = state.add_beat();
    beat->line_number = input.position().line;
  }
};

} // namespace Skald
