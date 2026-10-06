#pragma once

// LLM Usage Level: Assistant

#include "skald.h"
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Skald::Emit {

/** Schema version of the compiled tree. Bump when the layout changes. */
constexpr int FORMAT_VERSION = 1;

struct Value;
using Array = std::vector<Value>;
/** Insertion-ordered; printers emit keys in this order. */
using Table = std::vector<std::pair<std::string, Value>>;

/** Language-neutral value tree. Nil is std::monostate; a table entry whose
 *  value is nil is dropped by the tree builder, never printed. */
struct Value {
  std::variant<std::monostate, bool, int64_t, double, std::string, Array, Table>
      v;

  Value() = default;
  Value(bool b) : v(b) {}
  Value(int i) : v(static_cast<int64_t>(i)) {}
  Value(int64_t i) : v(i) {}
  Value(size_t i) : v(static_cast<int64_t>(i)) {}
  Value(float f) : v(static_cast<double>(f)) {}
  Value(double d) : v(d) {}
  Value(const char *s) : v(std::string(s)) {}
  Value(std::string s) : v(std::move(s)) {}
  Value(Array a) : v(std::move(a)) {}
  Value(Table t) : v(std::move(t)) {}

  bool is_nil() const { return std::holds_alternative<std::monostate>(v); }
};

/** Builds the `_codex.lua` tree. `module_paths` are codex-relative, forward
 *  slashed, sorted. */
Value build_codex_tree(const Codex &codex,
                       const std::vector<std::string> &module_paths);

/** Builds one module's tree. `path` is written verbatim to the `path` field. */
Value build_module_tree(const Module &module, const std::string &path);

/** Lua source: header comment naming `header_path`, then `return <table>`. */
std::string print_lua(const Value &value, const std::string &header_path);

/** JSON text, two-space indent, trailing newline. */
std::string print_json(const Value &value);

/** One simple value as a Lua literal: `5`, `5.0`, `true`, `"text"`. */
std::string lua_literal(const SimpleRValue &value);

} // namespace Skald::Emit
