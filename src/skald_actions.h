#pragma once

#include "debug.h"
#include "parse_state.h"
#include "skald.h"
#include "skald_grammar.h"
#include "tao/pegtl/position.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Skald {

template <typename Rule> struct action {};

// SECTION: BLOCK TAGS

template <> struct action<block1_prefix> { // #
  static void apply0(ParseState &state) { state.last_tag_level = 0; }
};
template <> struct action<block2_prefix> { // ##
  static void apply0(ParseState &state) { state.last_tag_level = 1; }
};
template <> struct action<block3_prefix> { // ###
  static void apply0(ParseState &state) { state.last_tag_level = 2; }
};

// Tags will be encoded as {parent}.{child}.{grandchild}
template <> struct action<block_tag_name> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto base = input.string();
    std::string tag;
    switch (state.last_tag_level) {
    case 0: // #
      tag = base;
      state.open_parent_tag = base;
      state.open_child_tag = ""; // Close child tags from previous block
      break;
    case 1: // ##
      if (state.open_parent_tag.size() == 0) {
        state.err(input.position(),
                  "Got a child tag but no parent block was open");
        return;
      }
      tag = state.open_parent_tag + "." + base;
      state.open_child_tag = base;
      state.open_grandchild_tag = "";
      break;
    case 2: // ###
      if (state.open_child_tag.size() == 0) {
        state.err(input.position(),
                  "Got a grandchild tag but no child tag was open");
        return;
      }
      // should be handled by case 1 but just in case:
      assert(state.open_parent_tag != "");
      tag = state.open_parent_tag + "." + state.open_child_tag + "." + base;
      state.open_grandchild_tag = base;
      break;
    }
    state.start_block(tag);
  }
};

template <> struct action<identifier> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();
    state.last_identifier = text;
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
      state.err(input.position(),
                "Tried to open a testbed but another top matter "
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
      state.err(input.position(),
                "Got a testbed end, but no testbed was open!");
    }
    state.top_matter_section = ParseState::TopMatterSection::NONE;
  }
};

template <> struct action<testbed_set> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    if (state.top_matter_section != ParseState::TopMatterSection::TESTBED) {
      state.err(input.position(),
                "Got a testbed set, but no testbed was open!");
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
      state.err(input.position(),
                "Tried to open a let clause but another top matter "
                "section was already open.");
    }
    state.top_matter_section = ParseState::TopMatterSection::LET;
  }
};

template <> struct action<let_close> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    if (state.top_matter_section != ParseState::TopMatterSection::LET) {
      state.err(input.position(), "Got a let end, but no let clause was open!");
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
          input.position(),
          "Got a second let clause; a given module should only have one.");
    }
    dbg_out(">>> saving " << state.module_vars_stack.size() << " module vars");
    state.module.module_vars = std::move(state.module_vars_stack);
  }
};

/// Declarations ///

template <> struct action<declaration_default> {
  static void apply0(ParseState &state) { state.declaration_was_valued = true; }
};
template <> struct action<declaration_type> {
  static void apply0(ParseState &state) { state.declaration_was_typed = true; }
};
template <> struct action<declaration> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {

    // FIXME: declaration_was valued and _was_typed are not being set!

    // Rule: Must *either* be typed or valued (or both)
    if (!state.declaration_was_valued && !state.declaration_was_typed) {
      state.err(
          input.position(),
          "Declaration must have either a type or a default value (or both).");
      return;
    }

    ValueType t;
    SimpleRValue v;
    if (state.declaration_was_typed) {
      t = state.last_type; // grab strong type
    }
    if (state.declaration_was_valued) {
      v = state.simple_rval_buffer_pop(); // grab default and get value from it
      t = srval_get_type(v);
      if (state.declaration_was_typed) {
        if (t != state.last_type) {
          state.err(input.position(), "Default value and type do not match");
          return;
        }
      }
    } else {
      v = get_zero(t);
    }
    auto n = state.pop_id(); // grab var name
    auto var = Variable{.name = n, .type = t};

    // Add to stack
    state.module_vars_stack.push_back(
        DeclaredVar{.initial_value = v, .var = var});

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
  }
};
template <> struct action<val_int> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.rval_buffer.push_back(std::stoi(input.string()));
  }
};
template <> struct action<val_float> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.rval_buffer.push_back(std::stof(input.string()));
  }
};
template <> struct action<val_string> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.rval_buffer.push_back(state.string_buffer);
  }
};
template <> struct action<r_variable> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.rval_buffer.push_back(Variable{input.string()});
  }
};
template <> struct action<r_method> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto method_call = std::make_shared<MethodCall>(MethodCall{
        .method = state.pop_id(), .args = std::move(state.argument_queue)});
    state.validate_method(*method_call, input.position());
    state.rval_buffer.push_back(method_call);
  }
};

template <> struct action<argument> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.argument_queue.push_back(state.rval_buffer_pop());
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
    state.add_text_string(text);
  }
};

// SECTION: CONDITIONALS

// This will grab and store the operation type
template <> struct action<checkable_2f_operator> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out("<.> stored comparator for " << input.string());
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
      // These comparator types only have the one rval
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

// Relative Move Steps
template <> struct action<move_child> {
  static void apply0(ParseState &state) {
    auto step =
        ParseState::RelMoveStep{.type = ParseState::RelMoveStep::Type::CHILD,
                                .identifier = state.pop_id()};
    state.rel_move_steps.push_back(step);
  }
};
template <> struct action<move_sib> {
  static void apply0(ParseState &state) {
    auto step =
        ParseState::RelMoveStep{.type = ParseState::RelMoveStep::Type::SIB,
                                .identifier = state.pop_id()};
    state.rel_move_steps.push_back(step);
  }
};
template <> struct action<move_parent> {
  static void apply0(ParseState &state) {
    auto step = ParseState::RelMoveStep{
        .type = ParseState::RelMoveStep::Type::PARENT, .identifier = ""};
    state.rel_move_steps.push_back(step);
  }
};

// Captures move identifier and transforms it into an absolute string
template <> struct action<move_identifier_short> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out("o---> SHORT: " << input.string());
    assert(state.open_parent_tag != ""); // No op possible w/out an open block

    // Assemble the starting tag
    std::array<std::string, 3> tag = {
        state.open_parent_tag, state.open_child_tag, state.open_grandchild_tag};
    int cursor = 0;
    if (state.open_child_tag != "")
      cursor = 1;
    if (state.open_grandchild_tag != "")
      cursor = 2;
    // dbg_out("   =. start: " << tag[0] << "." << tag[1] << "." << tag[2]);

    // Modify tag list via steps
    for (auto &step : state.rel_move_steps) {
      switch (step.type) {
      case ParseState::RelMoveStep::Type::PARENT:
        if (cursor == 0) {
          state.err(input.position(),
                    "Relative parent move from cursor, but already at parent!");
          return;
        }
        // dbg_out("     =]  (parent)");
        tag[cursor] = ""; // clear and step up
        cursor--;
        break;
      case ParseState::RelMoveStep::Type::SIB:
        // dbg_out("     =]  (sib) " << step.identifier);
        tag[cursor] = step.identifier; // swap out current id
        break;
      case ParseState::RelMoveStep::Type::CHILD:
        if (cursor >= 2) {
          state.err(
              input.position(),
              "Relative child move from cursor, but already at grandchild!");
          return;
        }
        // dbg_out("     =]  (child) " << step.identifier);
        cursor++;
        tag[cursor] = step.identifier;
        break;
      }
      // dbg_out("   =.  step: " << tag[0] << "." << tag[1] << "." << tag[2]);
    }
    state.rel_move_steps.clear();

    std::string joined;
    for (const auto &part : tag) {
      if (part.empty())
        continue;
      if (!joined.empty())
        joined += ".";
      joined += part;
    }
    state.move_identifier_store = joined;
    dbg_out("   => FULL: " << state.move_identifier_store);
  }
};

// Move identifier already as absolute string
template <> struct action<move_identifier_full> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.move_identifier_store = input.string();
    dbg_out("o---> FULL: " << state.move_identifier_store);
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
    std::string start_tag =
        state.does_go_have_start_tag ? state.move_identifier_store : "";
    dbg_out(" - >>> start_tag: " << start_tag);
    state.member_body_buffer =
        GoModule{.module_path = state.path_buffer, .start_in_tag = start_tag};
  }
};

template <> struct action<op_exit> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_exit: " << input.string() << " (pushing onto queue)");
    std::optional<RValue> arg = std::nullopt;
    if (state.rval_buffer.size() > 0)
      arg = state.rval_buffer_pop();
    state.member_body_buffer = Exit{.argument = arg};
  }
};

template <> struct action<op_move> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.member_body_buffer =
        Move{input.position().line, state.move_identifier_store};
    dbg_out(">>> op_move: " << input.string());
  }
};

template <> struct action<op_method> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto mc = MethodCall{input.position().line, state.pop_id(),
                         std::move(state.argument_queue)};
    state.validate_method(mc, input.position());
    state.member_body_buffer = std::move(mc);
    dbg_out(">>> op_method: " << input.string());
  }
};

/// SECTION: MUTATIONS ///

template <> struct action<op_mutate_subtract> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_mutate_subtract: " << input.string());
    state.member_body_buffer =
        Mutation{input.position().line, state.pop_id(), Mutation::SUBTRACT,
                 state.rval_buffer_pop()};
  }
};
template <> struct action<op_mutate_add> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_mutate_add: " << input.string());
    state.member_body_buffer = Mutation{input.position().line, state.pop_id(),
                                        Mutation::ADD, state.rval_buffer_pop()};
  }
};
template <> struct action<op_mutate_equate> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out(">>> op_mutate_equate: " << input.string());
    state.member_body_buffer =
        Mutation{input.position().line, state.pop_id(), Mutation::EQUATE,
                 state.rval_buffer_pop()};
  }
};
template <> struct action<op_mutate_switch> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.member_body_buffer =
        Mutation{input.position().line, state.pop_id(), Mutation::SWITCH, {}};
  }
};

/// OPERATION CORE ///

// FIXME: Remove this once the member consumption works
// template <> struct action<op_end> {
//   template <typename ActionInput>
//   static void apply(const ActionInput &input, ParseState &state) {
//     auto text = input.string();
//     dbg_out(">>> op_line: " << text);
//     LineOp lo;
//     lo.line_number = input.position().line;
//     lo.op = state.operation_queue_pop();
//     lo.condition.condition = state.conditional_buffer_pop();
//     state.add_member(lo);
//   }
// };

// SECTION: CHOICES

template <> struct action<inline_choice_move> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    auto text = input.string();

    // This method adds the move directly to the member queue as the move is
    // never conditional.
    state.add_choice_member(
        Member{.body = Move{input.position().line, state.pop_id()}});
  }
};

template <> struct action<choice_line> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    Choice choice;
    choice.content.parts = std::move(state.text_content_queue);
    choice.condition.condition = state.conditional_buffer_pop();
    choice.line_number = input.position().line;
    state.choice_stack.push_back(choice);
  }
};

template <> struct action<choice_block> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    state.add_choice_group(input.position().line);
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
    state.current_attrib_tag =
        (start != std::string::npos) ? tag.substr(start) : tag;
  }
};

template <> struct action<beat> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    const position p = input.position();
    dbg_out(">>> BEAT: " << input.string());
    state.add_beat(input.position().line);
  }
};

// SECTION: MEMBERS

// These two methods do exactly the same thing; base_member gets called for a
// non-indented member, whereas choice_member is always indented. Could probably
// condense this down but this feels clearer.

template <> struct action<member> {
  static void apply0(ParseState &state) { dbg_out("MMM member"); }
};

template <> struct action<base_member> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    assert(state.member_body_buffer); // Must have member body stored
    auto body = std::exchange(state.member_body_buffer, std::nullopt);
    auto mem = Member{.body = std::move(*body)};
    mem.ac.condition = state.conditional_buffer_pop();
    mem.line_number = input.position().line;
    dbg_out("BASE MEMBER on " << mem.line_number);
    state.add_member(std::move(mem));
  }
};

template <> struct action<choice_member> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    assert(state.member_body_buffer); // Must have member body stored
    auto body = std::exchange(state.member_body_buffer, std::nullopt);
    auto mem = Member{.body = std::move(*body)};
    mem.ac.condition = state.conditional_buffer_pop();
    mem.line_number = input.position().line;
    dbg_out("CHOICE MEMBER on " << mem.line_number);
    state.add_choice_member(std::move(mem));
  }
};

// SECTION: CONDITIONAL CHAINS

// Opens a conditional chain. Locks on keyword so conds work right.
template <> struct action<keyword_if> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out("@cond_chain_if");
    if (state.open_chain != nullptr) {
      state.err(input.position(),
                "Conditional chain already open but got another if!");
      return;
    }

    // Set up the new chain
    state.open_chain = std::make_unique<ConditionalChain>();

    // Add a block to push beats onto
    auto cb = ConditionalBlock{};
    cb.line_number = input.position().line;
    state.open_chain->cond_blocks.push_back(cb);

    // Set up the conditional
    state.conditional_step_in();
  }
};

template <> struct action<cond_chain_if_block> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out("@cond_chain_if_block");
    assert(state.open_chain != nullptr); // must always follow cond_chain_iff
    assert(state.open_chain->cond_blocks.size() > 0);

    // Attach captured conditional
    state.conditional_step_out(); // closes conditional following @if
    state.open_chain->cond_blocks.back().cond.condition =
        state.conditional_buffer_pop();
    assert(state.open_chain->cond_blocks.back().cond); // must have cond
  }
};

template <> struct action<keyword_elseif> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out("@cond_chain_elseif");
    if (state.open_chain == nullptr) {
      state.err(input.position(),
                "Tried to process elseif block but no @if statement was open");
      return;
    }
    assert(state.open_chain->cond_blocks.size() > 0); // must not be first

    // Add a block to push beats onto
    auto cb = ConditionalBlock{};
    cb.line_number = input.position().line;
    state.open_chain->cond_blocks.push_back(cb);

    // Set up cond
    state.conditional_step_in();
  }
};

template <> struct action<cond_chain_elseif_block> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out("@cond_chain_elseif_block");
    if (state.open_chain == nullptr)
      return; // Handled at elseif open

    // Attach captured conditional
    state.conditional_step_out();
    state.open_chain->cond_blocks.back().cond.condition =
        state.conditional_buffer_pop();
    assert(state.open_chain->cond_blocks.back().cond); // must have cond
  }
};

template <> struct action<cond_chain_else> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out("@cond_chain_else");
    if (state.open_chain == nullptr) {
      state.err(input.position(),
                "Tried to process elseif block but no @if statement was open");
      return;
    }
    assert(state.open_chain->cond_blocks.size() > 0); // must not be first
    auto cb = ConditionalBlock{};
    cb.line_number = input.position().line;
    state.open_chain->cond_blocks.push_back(cb);
  }
};

// FIXME: delete later
template <> struct action<cond_chain_endif> {
  static void apply0(ParseState &state) { dbg_out("@endif"); }
};

template <> struct action<cond_chain> {
  template <typename ActionInput>
  static void apply(const ActionInput &input, ParseState &state) {
    dbg_out("@cond_chain");
    assert(state.open_chain != nullptr); // must close an if clause
    assert(state.current_block != nullptr);
    state.current_block->members.push_back(
        MainBlockMember{std::move(*state.open_chain)});
    state.open_chain.reset();
  }
};

} // namespace Skald
