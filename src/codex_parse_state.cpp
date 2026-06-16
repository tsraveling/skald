#include "codex_parse_state.h"
#include "debug.h"
#include "skald.h"
#include "tao/pegtl/position.hpp"

namespace Skald {

// SECTION: ABSTRACTION

std::string CodexParseState::pop_id() {
  auto r = last_identifier;
  last_identifier = "";
  return r;
}

// SECTION: RVALUES

SimpleRValue CodexParseState::simple_rval_buffer_pop(tao::pegtl::position pos) {
  auto back = rval_buffer.back();
  rval_buffer.pop_back();

  return std::visit(
      [&](auto &&val) -> SimpleRValue {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::string> ||
                      std::is_same_v<T, bool> || std::is_same_v<T, int> ||
                      std::is_same_v<T, float>) {
          return val;
        } else {
          err(pos, "Tried to pop a simple value (int, bool, string, float) but "
                   "got a complex value (variable, method) instead!");
          return false;
        }
      },
      back);
}

// SECTION: ERROR HANDLING

void CodexParseState::err(const tao::pegtl::position pos, std::string msg) {
  errors.push_back(ParseError{.pos = from_pos(pos),
                              .msg = msg,
                              .severity = ParseError::Severity::ERROR});

  dbg_out("XXX CPS ERR: " << msg);
}
void CodexParseState::warn(const tao::pegtl::position pos, std::string msg) {
  errors.push_back(ParseError{.pos = from_pos(pos),
                              .msg = msg,
                              .severity = ParseError::Severity::WARNING});
}

} // namespace Skald
