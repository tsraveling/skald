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
  /** The module attached to the parsed file */
  Module module;

  /** The block currently under construction */
  Block *current_block = nullptr;

  /** Current operations stack */
  std::vector<Operation> operation_queue;

  /** The current beat stack */
  std::vector<TextPart> text_content_queue;

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

  /** Buffer for nesting conditionals */
  ConditionalAtom::Comparison current_comparison =
      ConditionalAtom::Comparison::TRUTHY;

  /** The stack of conditional items we will use to assemble clauses and whole
   * conditionals */
  std::vector<ConditionalItem> checkable_queue;

  /** Adds an atom (concrete base checker) to the checkable queue */
  void add_conditional_atom(const ConditionalAtom &atom) {
    dbg_out("-++ Adding a conditional atom to checkable_queue: "
            << atom.dbg_desc());
    checkable_queue.push_back(atom);
  }

  /** True if the current checkable list is OR (vs AND by default) */
  bool is_current_list_or = false;

  /** Length of current checkable list we are working on */
  int checkable_list_length = 1;

  /** The conditional stack. `.back()` is always the one that's open. */
  std::vector<Conditional> conditional_stack;

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
    module.blocks[tag] = new_block;

    current_block = &module.blocks[tag];
  }

  /** Called whenever a text queue is concluded */
  void conclude_text() {
    dbg_out(">>> conclude_text()");
    add_beat();
  }

  /** Creates a beat and adds it to the current block */
  void add_beat() {
    dbg_out(">>> add_beat()");
    if (!current_block) {
      Log::err("Found beat but there is no current block!");
    }
    Log::verbose(" - Adding beat.");

    Beat beat;
    beat.content.parts = std::move(text_content_queue);
    beat.operations = std::move(operation_queue);
    beat.attribution = current_tag;
    current_block->beats.push_back(beat);
  }

  void add_choice() {
    if (!current_block) {
      Log::err("Found choice but there is no current block!");
      return;
    }
    Log::verbose(" - Adding choice.");
    Choice choice;
    choice.content.parts = std::move(text_content_queue);
    choice.operations = std::move(operation_queue);
    current_block->choices.push_back(choice);
  }
};

} // namespace Skald
