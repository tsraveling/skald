#pragma once
#include "skald.h"
#include "tao/pegtl/position.hpp"
#include <optional>
#include <string>
#include <vector>

namespace Skald {

/** Exception class for Skald parse errors */
struct ParseError {
  tao::pegtl::position pos;
  std::string msg;
  enum Severity { WARNING, ERROR, FATAL } severity = ERROR;
};

struct ParseState {

  /** Constructor with filename */
  ParseState(const std::string &filename);

  // SECTION: MODULE LEVEL

  /** The module attached to the parsed file */
  Module module;

  // SECTION: ERROR HANDLING

  std::vector<ParseError> errors;
  void err(const tao::pegtl::position pos, std::string msg);
  void warn(const tao::pegtl::position pos, std::string msg);
  void fail(const tao::pegtl::position pos, std::string msg);

  // SECTION: TOP MATTER

  enum TopMatterSection { NONE, TESTBED, LET };
  TopMatterSection top_matter_section = TopMatterSection::NONE;

  // SECTION: BLOCKS

  /** The block currently under construction */
  Block *current_block = nullptr;

  /** Creates a block with the given tag and sets it as current in the parse
   * state */
  void start_block(const std::string &tag);

  // SECTION: BEATS

  /** The current beat attribution tag */
  std::string current_tag;

  /** The current beat content stack */
  std::vector<TextPart> beat_content_queue;

  /** Stores text content as beat text (avoiding choice text issues) */
  void store_beat_text();

  /** Creates a beat and adds it to the current block */
  Beat *add_beat();

  // SECTION: CHOICES

  /** Adds a choice */
  Choice *add_choice();

  // SECTION: OPERATIONS

  /** Current operations stack */
  std::vector<Operation> operation_queue;

  // SECTION: TEXT

  /** The current text stack */
  std::vector<TextPart> text_content_queue;

  /** Will either append to the last string if also a simple string, or add it
   * to the stack if not. */
  void add_text_string(std::string str);

  /** Called whenever a text queue is concluded */
  void conclude_text();

  /** This will hold the initial rval in an injectable so it can be used in
   * whatever format the injectable ends up being. */
  std::optional<RValue> injectable_buffer;

  /** Pops the injectable buffer and returns the RValue to use */
  RValue injectable_buffer_pop();

  /** This holds ternary options until the ternary tail is complete, at which
   * point these get committed to a given Insertion. */
  std::vector<TernaryOption> ternary_option_queue;

  // SECTION: CONDITIONALS

  /** Buffer for nesting conditionals */
  ConditionalAtom::Comparison current_comparison =
      ConditionalAtom::Comparison::TRUTHY;

  /** The conditional stack. `.back()` is always the one that's open. */
  std::vector<Conditional> conditional_stack;
  std::optional<Conditional> conditional_buffer;
  std::vector<Choice> choice_stack;

  /** Set up a new conditional and step the cursor into it */
  void conditional_step_in();

  /** Finalize the conditional on the cursor, and step out one level. Panics if
   * there's nothing outside of this one. */
  void conditional_step_out();

  /** Will return the conditional held in the buffer if there is one, and clear
   * it out */
  std::optional<Conditional> conditional_buffer_pop();

  /** Adds an atom (concrete base checker) to the checkable queue */
  void add_conditional_atom(const ConditionalAtom &atom);

  // SECTION: METHODS

  /** Argument stack for method calls etc */
  std::vector<RValue> argument_queue;

  // SECTION: GO

  /** Used to catch optional start tags for GO operations */
  bool does_go_have_start_tag = false;

  /** Stores the path for a GO command. Required, so don't need a pop method. */
  std::string path_buffer;

  // SECTION: RVALUES

  /** Buffers the last-held rvalue */
  std::vector<RValue> rval_buffer;

  /** Returns the last buffered RValue, and pop it out of the buffer */
  RValue rval_buffer_pop();

  /** Returns a value off of the rval buffer and panics if it's not simple. */
  SimpleRValue simple_rval_buffer_pop();

  // SECTION: ATOMS

  /** The last-parsed identifier */
  std::string last_identifier;

  /* Raw value buffers */
  std::string string_buffer;
  bool bool_buffer;

  /** This will return an identifier string if one is present.
   *  Use it like:
   *  `if (auto value = pop_id()) { std::cout  << *value; }`
   */
  std::optional<std::string> pop_id_cond();

  /** This returns whatever is in last_identifier and sets that value to an
   *  empty string.
   */
  std::string pop_id();
};

} // namespace Skald
