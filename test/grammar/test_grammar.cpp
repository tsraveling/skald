#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "parse_state.h"
#include "skald_actions.h"
#include "skald_grammar.h"

#include <cstddef>
#include <string_view>
#include <tao/pegtl.hpp>

namespace pegtl = tao::pegtl;
using namespace Skald;

namespace {

// True if Rule consumes the entire input. Use for "this is a complete X" cases.
template <typename Rule> bool parses_full(std::string_view in) {
  pegtl::memory_input input{in.data(), in.size(), "test"};
  return pegtl::parse<pegtl::seq<Rule, pegtl::eof>>(input);
}

// Parses Rule with actions and returns the populated ParseState for inspection.
template <typename Rule>
ParseState parse_capturing(std::string_view in, bool *ok = nullptr) {
  ParseState state{"test"};
  pegtl::memory_input input{in.data(), in.size(), "test"};
  const bool result =
      pegtl::parse<pegtl::seq<Rule, pegtl::eof>, Skald::action>(input, state);
  if (ok)
    *ok = result;
  return state;
}

} // namespace

TEST_CASE("rvalue: accepts each primitive form") {
  CHECK(parses_full<rvalue>("42"));
  CHECK(parses_full<rvalue>("-7"));
  CHECK(parses_full<rvalue>("3.14"));
  CHECK(parses_full<rvalue>("true"));
  CHECK(parses_full<rvalue>("false"));
  CHECK(parses_full<rvalue>("\"hello\""));
  CHECK(parses_full<rvalue>("some_var"));
  CHECK(parses_full<rvalue>(":a_method()"));
  CHECK(parses_full<rvalue>(":with_args(5, true, \"x\", other_var)"));
}

TEST_CASE("rvalue: rejects malformed input") {
  CHECK_FALSE(parses_full<rvalue>(""));
  CHECK_FALSE(parses_full<rvalue>("\"unterminated"));
  CHECK_FALSE(parses_full<rvalue>(":missing_parens"));
  CHECK_FALSE(parses_full<rvalue>("3.14.15"));
}

TEST_CASE("rvalue: actions yield the expected variant") {
  SUBCASE("int") {
    bool ok = false;
    auto state = parse_capturing<rvalue>("42", &ok);
    REQUIRE(ok);
    REQUIRE(state.rval_buffer.size() == 1);
    const auto *as_int = rval_get_int(state.rval_buffer.back());
    REQUIRE(as_int != nullptr);
    CHECK(*as_int == 42);
  }
  SUBCASE("string") {
    bool ok = false;
    auto state = parse_capturing<rvalue>("\"hello\"", &ok);
    REQUIRE(ok);
    REQUIRE(state.rval_buffer.size() == 1);
    const auto *as_str = rval_get_str(state.rval_buffer.back());
    REQUIRE(as_str != nullptr);
    CHECK(*as_str == "hello");
  }
  SUBCASE("variable reference") {
    bool ok = false;
    auto state = parse_capturing<rvalue>("some_var", &ok);
    REQUIRE(ok);
    REQUIRE(state.rval_buffer.size() == 1);
    const auto *as_var = rval_get_var(state.rval_buffer.back());
    REQUIRE(as_var != nullptr);
    CHECK(as_var->name == "some_var");
  }
}
