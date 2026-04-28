#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "skald.h"

#include <optional>
#include <string>
#include <variant>

using namespace Skald;

namespace {

// Joins all chunks of a Content response into a single string.
std::string stitch(const std::vector<Chunk> &chunks) {
  std::string out;
  for (const auto &c : chunks)
    out += c.text;
  return out;
}

// Drives an Engine through a script and exposes assertion helpers.
//
// Mirrors the loop structure in src/test_main.cpp, but instead of reading
// from stdin and printing, it lets each test step the engine and check
// the resulting Response variant.
class ScriptDriver {
public:
  Engine engine;
  Response current;

  explicit ScriptDriver(const std::string &path) { engine.load(path); }

  void start() { current = engine.start(); }
  void start_at(const std::string &tag) { current = engine.start_at(tag); }

  // Advance past a Content response by selecting choice index (or 0 for
  // continue).
  void choose(int idx = 0) { current = engine.act(idx); }

  // Answer the open Query with a value, or nullopt for void calls.
  void answer(std::optional<SimpleRValue> val) {
    current = engine.answer(QueryAnswer{val});
  }

  // ---- assertions -----------------------------------------------------

  void expect_content(const std::string &expected_text) {
    REQUIRE(get_response_type(current) == ResponseType::CONTENT);
    const auto &content = std::get<Content>(current);
    CHECK(stitch(content.text) == expected_text);
  }

  void expect_query(const std::string &expected_method) {
    REQUIRE(get_response_type(current) == ResponseType::QUERY);
    CHECK(std::get<Query>(current).call.method == expected_method);
  }

  void expect_exit() {
    REQUIRE(get_response_type(current) == ResponseType::EXIT);
  }

  void expect_end() {
    REQUIRE(get_response_type(current) == ResponseType::END);
  }

  void expect_no_error() {
    if (get_response_type(current) == ResponseType::ERROR) {
      const auto &err = std::get<Error>(current);
      FAIL("Engine returned Error code " << err.code << " on line "
                                         << err.line_number << ": "
                                         << err.message);
    }
  }
};

} // namespace

TEST_CASE("test.ska: starts at `start` block with expected first beat") {
  ScriptDriver driver{SKALD_TEST_DIR "/test.ska"};
  driver.start_at("start");
  driver.expect_no_error();
  driver.expect_content("This is the first beat.");
}
