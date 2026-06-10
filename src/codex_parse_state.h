#pragma once
#include "parse_state.h"
#include "skald.h"
#include <string>

namespace Skald {

/** Parse state for codex files */
struct CodexParseState {

  Codex codex;

  /** Constructor with filename */
  CodexParseState(const std::string &filename) {
    codex = Codex{.filename = filename};
  }

  // SECTION: ERROR HANDLING

  std::vector<ParseError> errors;
};

} // namespace Skald
