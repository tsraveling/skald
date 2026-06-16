#pragma once
#include "parse_state.h"
#include "skald.h"
#include <filesystem>
#include <string>

namespace Skald {

/** Parse state for codex files */
struct CodexParseState {

  Codex codex;

  // SECTION: ABSTRACT PARTS

  /** The last-parsed identifier */
  std::string last_identifier;

  /** This returns whatever is in last_identifier and sets that value to an
   *  empty string.
   */
  std::string pop_id();

  // SECTION: RVALUES

  /** Buffers the last-held rvalue */
  std::vector<RValue> rval_buffer;

  std::string string_buffer;

  /** Returns the last buffered RValue, and pop it out of the buffer */
  RValue rval_buffer_pop();

  /** Returns a value off of the rval buffer. If it's not simple, records a
   *  parse error at `pos` and returns a false default. */
  SimpleRValue simple_rval_buffer_pop(tao::pegtl::position pos);

  // SECTION: TYPING

  ValueType last_type;

  // SECTION: GLOBALS

  bool declaration_was_typed;
  bool declaration_was_valued;

  // SECTION: METHODS

  std::vector<ArgDef> arg_buffer;
  std::string method_id_buffer;

  // SECTION: CONSTRUCTION

  /** Constructor with filename. Splits the given path (which may be
   *  relative, e.g. "../test/example.codex") into the codex's directory and
   *  bare filename. */
  CodexParseState(const std::string &filename) {
    std::filesystem::path p(filename);
    codex = Codex{.path = p.parent_path().string(),
                  .filename = p.filename().string()};
  }

  // SECTION: ERROR HANDLING

  std::vector<ParseError> errors;
  void err(const tao::pegtl::position pos, std::string msg);
  void warn(const tao::pegtl::position pos, std::string msg);
};

} // namespace Skald
