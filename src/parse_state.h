#pragma once
#include "debug.h"
#include "logger.h"
#include "skald.h"
#include "skald_grammar.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Skald {

struct ParseState {

  /** Stores if a declaration is an import or not */
  bool last_declaration_was_import = false;

  /** The module attached to the parsed file */
  Module module;

  /** The block currently under construction */
  Block *current_block = nullptr;

  /** Current operations stack */
  std::vector<Operation> operation_queue;

  /** The current text stack */
  std::vector<TextPart> text_content_queue;

  /** The current beat content stack */
  std::vector<TextPart> beat_content_queue;

  /** Tag holder, might be used by multiple entities */
  std::string current_tag;

  /** The last-parsed identifier */
  std::string last_identifier;

  /* Raw value buffers */
  std::string string_buffer;
  bool bool_buffer;

  /** Buffers the last-held rvalue */
  std::vector<RValue> rval_buffer;

  /** Returns the last buffered RValue, and pop it out of the buffer */
  RValue rval_buffer_pop() {
    dbg_out(">>> rval_buffer_pop: " << rval_buffer.size() << " -1 ");
    auto back = rval_buffer.back();
    rval_buffer.pop_back();
    return back;
  }

  SimpleRValue simple_rval_buffer_pop() {
    dbg_out(">>> simple_rval_buffer_pop: " << rval_buffer.size() << " -1 ");
    auto back = rval_buffer.back();
    rval_buffer.pop_back();

    return std::visit(
        [](auto &&val) -> SimpleRValue {
          using T = std::decay_t<decltype(val)>;
          if constexpr (std::is_same_v<T, std::string> ||
                        std::is_same_v<T, bool> || std::is_same_v<T, int> ||
                        std::is_same_v<T, float>) {
            return val;
          } else {
            throw std::runtime_error(
                "Expected simple RValue, got complex type");
          }
        },
        back);
  }

  /** Buffer for nesting conditionals */
  ConditionalAtom::Comparison current_comparison =
      ConditionalAtom::Comparison::TRUTHY;

  // FIXME: Delete all these if we can

  // /** The stack of conditional items we will use to assemble clauses and
  // whole
  //  * conditionals */
  // std::vector<ConditionalItem> checkable_queue;

  // /** True if the current checkable list is OR (vs AND by default) */
  // bool is_current_list_or = false;

  // /** Length of current checkable list we are working on */
  // int checkable_list_length = 1;

  /** The conditional stack. `.back()` is always the one that's open. */
  std::vector<Conditional> conditional_stack;
  std::optional<Conditional> conditional_buffer;
  std::vector<Choice> choice_stack;

  void conditional_step_in() { conditional_stack.push_back(Conditional{}); }

  void conditional_step_out() {
    if (conditional_stack.size() > 1) {
      // If this isn't the first item, close it into a conditional item and add
      // it to the next list up
      auto last =
          std::make_shared<Conditional>(std::move(conditional_stack.back()));
      conditional_stack.pop_back();
      conditional_stack.back().items.push_back(last);
    } else if (conditional_stack.size() > 0) {
      // If it's the only item, move it into the conditional buffer
      conditional_buffer = std::move(
          conditional_stack.back()); // Moves the struct's moveable members
      conditional_stack.pop_back();
    } else {
      Log::err("Tried to step out of a conditional but the stack is empty!");
    }
  }

  /** Will return the conditional held in the buffer if there is one, and clear
   * it out */
  auto conditional_buffer_pop() {
    return std::exchange(conditional_buffer, std::nullopt);
  }

  /** Adds an atom (concrete base checker) to the checkable queue */
  void add_conditional_atom(const ConditionalAtom &atom) {
    dbg_out("-++ Adding a conditional atom to checkable_queue: "
            << atom.dbg_desc());
    if (conditional_stack.size() < 1) {
      Log::err("Tried to add a conditional atom, but no conditional was open!");
      return;
    }
    auto &current = conditional_stack.back();
    current.items.push_back(atom);
  }

  /** Argument stack for method calls etc */
  std::vector<RValue> argument_queue;

  /** Construct with filename */
  ParseState(const std::string &filename) { module.filename = filename; }

  /** This will return an identifier string if one is present.
   *  Use it like:
   *  `if (auto value = pop_id()) { std::cout  << *value; }`
   */
  std::optional<std::string> pop_id_cond() {
    if (last_identifier.length() < 1) {
      return std::nullopt;
    }
    return pop_id();
  }

  /** This returns whatever is in last_identifier and sets that value to an
   *  empty string.
   */
  std::string pop_id() {
    auto r = last_identifier;
    last_identifier = "";
    return r;
  }

  /** Used to catch optional start tags for GO operations */
  bool does_go_have_start_tag = false;

  /** This will hold the initial rval in an injectable so it can be used in
   * whatever format the injectable ends up being. */
  std::optional<RValue> injectable_buffer;

  /** Pops the injectable buffer and returns the RValue to use */
  auto injectable_buffer_pop() {
    return *std::exchange(injectable_buffer, std::nullopt);
  }

  /** Stores the path for a GO command. Required, so don't need a pop method. */
  std::string path_buffer;

  /** This holds ternary options until the ternary tail is complete, at which
   * point these get committed to a given Insertion. */
  std::vector<TernaryOption> ternary_option_queue;

  /** Will either append to the last string if also a simple string, or add it
   * to the stack if not. */
  void add_text_string(std::string str) {
    Log::verbose("Beat queue +=", str);
    if (text_content_queue.empty() ||
        !std::holds_alternative<std::string>(text_content_queue.back())) {
      text_content_queue.push_back(str);
    } else {
      std::string &last_str = std::get<std::string>(text_content_queue.back());
      // Eliminate double spaces for comment joins
      last_str +=
          (last_str.back() == ' ' && str.front() == ' ') ? str.substr(1) : str;
    }
  }

  /** Creates a block with the given tag and sets it as current in the parse
   * state */
  void start_block(const std::string &tag) {
    Log::verbose("Starting new block:", tag);

    Block new_block;
    new_block.tag = tag;
    module.blocks.push_back(new_block);
    module.block_lookup[tag] = module.blocks.size() - 1;

    current_block = &module.blocks.back();
  }

  /** Called whenever a text queue is concluded */
  void conclude_text() {
    dbg_out(">>> conclude_text()");
    add_beat();
  }

  /** Stores text content as beat text (avoiding choice text issues) */
  void store_beat_text() { beat_content_queue = std::move(text_content_queue); }

  /** Creates a beat and adds it to the current block */
  Beat *add_beat() {
    dbg_out(">>> add_beat()");
    if (!current_block) {
      Log::err("Found beat but there is no current block!");
    }
    Log::verbose(" - Adding beat.");

    Beat beat;
    beat.condition = conditional_buffer_pop();
    beat.content.parts = std::move(beat_content_queue);
    beat.operations = std::move(operation_queue);
    beat.choices = std::move(choice_stack);
    beat.attribution = current_tag;
    current_block->beats.push_back(beat);
    return &current_block->beats.back();
  }

  /** Stores an `(else)` flag for a logic block */
  bool store_is_else = false;

  /** Creates a beat and adds it to the current block */
  Beat *add_logic_beat() {
    dbg_out(">>> add_logic_beat()");
    if (!current_block) {
      Log::err("Found a logic beat but there is no current block!");
    }
    Log::verbose(" - Adding a logic beat.");

    Beat beat;
    beat.condition = conditional_buffer_pop();
    beat.is_logic_block = true;
    beat.operations = std::move(operation_queue);
    beat.is_else = store_is_else;
    current_block->beats.push_back(beat);
    store_is_else = false;
    return &current_block->beats.back();
  }

  Choice *add_choice() {
    if (!current_block) {
      Log::err("Found choice but there is no current block!");
      return nullptr;
    }
    Log::verbose(" - Adding choice.");
    Choice choice;
    choice.content.parts = std::move(text_content_queue);
    choice.operations = std::move(operation_queue);
    choice.condition = conditional_buffer_pop();
    choice_stack.push_back(choice);
    return &choice_stack.back();
  }
};

} // namespace Skald
