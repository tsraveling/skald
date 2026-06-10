#pragma once
#include "parse_state.h"
#include "skald.h"
#include <filesystem>
#include <string>

namespace Skald {

/** Parse state for codex files */
struct CodexParseState {

  Codex codex;

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
};

} // namespace Skald
