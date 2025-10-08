#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Skald {

struct Variable {
  std::string name;
};

// Forward declarations
struct MethodCall;
using RValue = std::variant<std::string, bool, int, float, Variable,
                            std::shared_ptr<MethodCall>>;

struct MethodCall {
  std::string method;
  std::vector<RValue> args;
  std::string dbg_desc() const; // Declare only for circular dep reasons
};

// rval_to_string must be declared before MethodCall::dbg_desc uses it
inline std::string rval_to_string(const RValue &val) {
  return std::visit(
      [](const auto &value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::string>) {
          return value;
        } else if constexpr (std::is_same_v<T, bool>) {
          return value ? "true" : "false";
        } else if constexpr (std::is_arithmetic_v<T>) {
          return std::to_string(value);
        } else if constexpr (std::is_same_v<T, Variable>) {
          return value.name;
        } else if constexpr (std::is_same_v<T, std::shared_ptr<MethodCall>>) {
          return value->dbg_desc();
        }
      },
      val);
}

// Now define MethodCall::dbg_desc after rval_to_string is available
inline std::string MethodCall::dbg_desc() const {
  std::string ret = "CALL " + method + ": ";
  for (const auto &arg : args) {
    ret += rval_to_string(arg) + " ";
  }
  return ret;
}

struct ConditionalAtom {
  enum Comparison {
    TRUTHY,
    NOT_TRUTHY,
    EQUALS,
    NOT_EQUALS,
    MORE,
    LESS,
    MORE_EQUAL,
    LESS_EQUAL
  };
  static Comparison comparison_for_operator(const std::string op) {
    if (op == "=")
      return Comparison::EQUALS;
    if (op == "!=")
      return Comparison::NOT_EQUALS;
    if (op == ">")
      return Comparison::MORE;
    if (op == "<")
      return Comparison::LESS;
    if (op == ">=")
      return Comparison::MORE_EQUAL;
    if (op == "<=")
      return Comparison::LESS_EQUAL;
    return TRUTHY;
  }
  RValue a;
  Comparison comparison;
  std::optional<RValue> b;
  std::string dbg_desc() const {
    switch (comparison) {
    case TRUTHY:
      return "IS " + rval_to_string(a) + "?";
    case NOT_TRUTHY:
      return "IS NOT " + rval_to_string(a) + "?";
    case EQUALS:
      return "IS " + rval_to_string(a) + " = " + rval_to_string(*b) + "?";
    case NOT_EQUALS:
      return "IS " + rval_to_string(a) + " != " + rval_to_string(*b) + "?";
    case MORE:
      return "IS " + rval_to_string(a) + " > " + rval_to_string(*b) + "?";
    case LESS:
      return "IS " + rval_to_string(a) + " < " + rval_to_string(*b) + "?";
    case MORE_EQUAL:
      return "IS " + rval_to_string(a) + " >= " + rval_to_string(*b) + "?";
    case LESS_EQUAL:
      return "IS " + rval_to_string(a) + " <= " + rval_to_string(*b) + "?";
    }
  }
};

struct Conditional;
using ConditionalItem =
    std::variant<ConditionalAtom, std::shared_ptr<Conditional>>;

// Forward-declared:
/** Returns the debug text describing a ConditionalItem, which can be an atom or
 * a clause. */
std::string dbg_desc_conditional_item(const ConditionalItem &item);

struct Conditional {
  enum Type { AND, OR };
  Type type = Type::AND;
  std::vector<ConditionalItem> items;

  std::string dbg_desc() const {
    std::string ret = "[IF: ";
    int i = 0;
    for (auto item : items) {
      if (i > 0)
        ret += type == Type::AND ? " AND " : " OR ";
      ret += dbg_desc_conditional_item(item);
      i++;
    }
    ret += "]";
    return ret;
  }
};

inline std::string dbg_desc_conditional_item(const ConditionalItem &item) {
  return std::visit(
      [](const auto &value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, ConditionalAtom>) {
          return value.dbg_desc();
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Conditional>>) {
          return value->dbg_desc();
        }
      },
      item);
}

struct Mutation {
  enum Type { EQUATE, SWITCH, ADD, SUBTRACT };
  std::string lvalue;
  Type type;
  std::optional<RValue> rvalue;
  std::string dbg_desc() const {
    std::string ret = "<" + lvalue + " ";
    switch (type) {
    case EQUATE:
      ret += "EQUALS";
      break;
    case SWITCH:
      ret += "SWITCH";
      break;
    case ADD:
      ret += "ADD";
      break;
    case SUBTRACT:
      ret += "SUBTRACT";
      break;
    }
    if (rvalue) {
      ret += " " + rval_to_string(*rvalue);
    }
    return ret + ">";
  }
};

struct Move {
  std::string target_tag;
};

using Operation = std::variant<Move, MethodCall, Mutation>;

struct TextInsertion {
  std::string variable_name;
};

using TextPart = std::variant<std::string, TextInsertion>;

struct TextContent {
  std::vector<TextPart> parts;
  std::string dbg_desc() const {
    std::string ret = "";
    for (auto &part : parts) {
      if (std::holds_alternative<std::string>(part)) {
        ret += std::get<std::string>(part);
      } else {
        auto ins = std::get<TextInsertion>(part);
        ret += "{" + ins.variable_name + "}";
      }
    }
    return ret;
  }
};

struct Choice {
  std::optional<Conditional> condition;
  TextContent content;
  std::vector<Operation> operations;

  std::string dbg_desc() const {
    return (condition ? condition->dbg_desc() + " " : "") + content.dbg_desc();
  }
};

struct Beat {
  std::optional<Conditional> condition;
  std::string attribution;
  std::vector<Operation> operations;
  TextContent content;

  std::string dbg_desc() const {
    return (condition ? condition->dbg_desc() + " " : "") +
           (attribution.length() > 0 ? attribution + ": " : "") +
           content.dbg_desc();
  }
};

// TODO: Support the "auto-continue" block
// (that is untagged and follows an inline choice block)
struct Block {
  std::string tag;
  std::vector<Beat> beats{};
  std::vector<Choice> choices;
};

class Module {
public:
  std::string filename;
  std::map<std::string, Block> blocks;

  Block *get_block(std::string tag) {
    auto it = blocks.find(tag);
    return (it != blocks.end()) ? &it->second : nullptr;
  }
};

class Skald {
private:
  std::unique_ptr<Module> current;

public:
  void load(std::string path);
  void trace(std::string path);
};

} // namespace Skald
