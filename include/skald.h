#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace Skald {

struct Variable {
  std::string name;
};

using RValue = std::variant<std::string, bool, int, float, Variable>;

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
        } else {
          return value.name;
        }
      },
      val);
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

struct MethodCall {
  std::string method;
  std::vector<RValue> args;

  std::string dbg_desc() const {
    std::string ret = "CALL " + method + ": ";
    for (const auto &arg : args) {
      ret += rval_to_string(arg) + " ";
    }
    return ret;
  }
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
  TextContent content;
  std::vector<Operation> operations;
};

struct Beat {
  std::string attribution;
  std::vector<Operation> operations;
  TextContent content;
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
