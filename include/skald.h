#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Skald {

struct LineEntity {
  size_t line_number = 0;
};

struct Variable {
  std::string name;
};

// Forward declarations
struct MethodCall;
using RValue = std::variant<std::string, bool, int, float, Variable,
                            std::shared_ptr<MethodCall>>;

// Rval helper functions
inline const std::string *rval_get_str(const RValue &val) {
  return std::get_if<std::string>(&val);
}
inline const int *rval_get_int(const RValue &val) {
  return std::get_if<int>(&val);
}
inline const bool *rval_get_bool(const RValue &val) {
  return std::get_if<bool>(&val);
}
inline const float *rval_get_float(const RValue &val) {
  return std::get_if<float>(&val);
}
inline const Variable *rval_get_var(const RValue &val) {
  return std::get_if<Variable>(&val);
}
inline const MethodCall *rval_get_call(const RValue &val) {
  if (auto *p = std::get_if<std::shared_ptr<MethodCall>>(&val)) {
    return p->get();
  }
  return nullptr;
}

using SimpleRValue = std::variant<std::string, bool, int, float>;

// Simple RVal helper functions
inline const std::string *srval_get_str(const SimpleRValue &val) {
  return std::get_if<std::string>(&val);
}
inline const int *srval_get_int(const SimpleRValue &val) {
  return std::get_if<int>(&val);
}
inline const bool *srval_get_bool(const SimpleRValue &val) {
  return std::get_if<bool>(&val);
}
inline const float *srval_get_float(const SimpleRValue &val) {
  return std::get_if<float>(&val);
}

/** Attempts to cast an RValue to a SimpleRValue, returnig nullopt if this is
 * impossible. */
inline std::optional<SimpleRValue> cast_rval_to_simple(const RValue &rval) {
  return std::visit(
      [](const auto &value) -> std::optional<SimpleRValue> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_constructible_v<SimpleRValue, T>) {
          return SimpleRValue(value);
        } else {
          return std::nullopt;
        }
      },
      rval);
}

struct Declaration : LineEntity {
  Variable var;
  SimpleRValue initial_value;
  bool is_imported = false;
};

struct MethodCall : LineEntity {
  std::string method;
  std::vector<RValue> args;
  std::string dbg_desc() const; // Declare only for circular dep reasons
};

// rval_to_string must be declared before MethodCall::dbg_desc uses it
template <typename VariantType>
inline std::string rval_to_string(const VariantType &val) {
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

/** The key used to encode a query for answer caching */
inline std::string key_for_call(MethodCall &call) {
  std::string ret = call.method;
  for (auto &arg : call.args) {
    ret += "|" + rval_to_string(arg);
  }
  return ret;
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

struct Conditional : LineEntity {
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

struct TestbedDeclaration : LineEntity {
  std::string variable;
  SimpleRValue test_value;
  std::string dbg_desc() const {
    return "  -> " + variable + " = " + rval_to_string(test_value);
  }
};

struct Testbed : LineEntity {
  std::string name;
  std::vector<TestbedDeclaration> declarations;
  std::string dbg_desc() const {
    std::string ret = "@" + name + ":";
    for (auto &dec : declarations) {
      ret += "\n" + dec.dbg_desc();
    }
    return ret;
  }
};

struct Mutation : LineEntity {
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

struct GoModule : LineEntity {
  std::string module_path;
  std::string dbg_desc() const {
    return module_path +
           (start_in_tag.length() > 0 ? " > " + start_in_tag : "");
  }
  std::string start_in_tag;
};

struct Exit : LineEntity {
  std::optional<RValue> argument;
  std::string dbg_desc() const {
    if (argument) {
      return rval_to_string(*argument);
    } else {
      return "<no argument>";
    }
  }
};

struct Move : LineEntity {
  std::string target_tag;
};

using Operation = std::variant<Move, MethodCall, Mutation, GoModule, Exit>;

inline const MethodCall *op_get_call(const Operation &val) {
  return std::get_if<MethodCall>(&val);
}

/* DEBUG OUTPUT STUFF TO DELETE LATER */

struct OpDebugProcessor {
  std::string operator()(const Move &move) {
    return "\n    * MOVE TO: " + move.target_tag;
  }
  std::string operator()(const MethodCall &method_call) {
    return "\n    * CALL: " + method_call.dbg_desc();
  }
  std::string operator()(const Mutation &mutation) {
    return "\n    * MUTATE: " + mutation.dbg_desc();
  }
  std::string operator()(const GoModule &go) {
    return "\n    * GO TO: " + go.dbg_desc();
  }
  std::string operator()(const Exit &exit) {
    return "\n    * EXIT: " + exit.dbg_desc();
  }
};

inline std::string dbg_desc_ops(const std::vector<Operation> &ops) {
  std::string ret = "";
  for (auto &op : ops) {
    ret += std::visit(OpDebugProcessor{}, op);
  }
  return ret;
}

using TernaryOption = std::tuple<RValue, RValue>;

struct TernaryInsertion {
  RValue check;
  std::vector<TernaryOption> options;
  std::string dbg_desc() const {
    std::string ret = "{> " + rval_to_string(check) + "? ";
    for (auto &opt : options) {
      ret += rval_to_string(std::get<0>(opt)) + ":" +
             rval_to_string(std::get<1>(opt));
    }
    ret += "}";
    return ret;
  }
};

struct ChanceOption {
  int weight;
  RValue value;
  std::string dbg_desc() {
    return std::to_string(weight) + ":" + rval_to_string(value);
  }
};

struct ChanceInsertion {
  std::vector<ChanceOption> options;
  std::string dbg_desc() {
    std::string ret = "{> DICE ? ";
    for (auto &opt : options) {
      ret += opt.dbg_desc();
    }
    ret += "}";
    return ret;
  }
};

struct SimpleInsertion {
  RValue rvalue;
  std::string dbg_desc() { return rval_to_string(rvalue); }
};

using TextPart = std::variant<std::string, SimpleInsertion, TernaryInsertion>;

struct TextContent {
  std::vector<TextPart> parts;
  std::string dbg_desc() const {
    std::string ret = "";
    for (auto &part : parts) {
      if (std::holds_alternative<std::string>(part)) {
        ret += std::get<std::string>(part);
      } else if (std::holds_alternative<TernaryInsertion>(part)) {
        auto ins = std::get<TernaryInsertion>(part);
        ret += "{" + ins.dbg_desc() + "}";
      } else {
        auto ins = std::get<SimpleInsertion>(part);
        ret += "{" + ins.dbg_desc() + "}";
      }
    }
    return ret;
  }
};

struct Choice : LineEntity {
  std::optional<Conditional> condition;
  TextContent content;
  std::vector<Operation> operations;

  std::string dbg_desc() const {
    return (condition ? condition->dbg_desc() + " " : "") + content.dbg_desc() +
           " (" + std::to_string(operations.size()) + " ops)";
  }
};

struct Beat : LineEntity {
  std::optional<Conditional> condition;
  std::string attribution;
  std::vector<Operation> operations;
  TextContent content;
  bool is_logic_block = false;
  bool is_else = false;
  std::vector<Choice> choices;

  std::string dbg_desc() const {
    return (condition ? condition->dbg_desc() + " " : "") +
           (attribution.length() > 0 ? attribution + ": " : "") +
           (is_logic_block ? (is_else ? "<ELSE>" : "<LOGIC>")
                           : content.dbg_desc()) +
           (operations.size() > 0 ? dbg_desc_ops(operations) : "");
  }
};

// TODO: Support the "auto-continue" block
// (that is untagged and follows an inline choice block)
struct Block : LineEntity {
  std::string tag;
  std::vector<Beat> beats{};
};

class Module {
public:
  std::string filename;
  std::vector<Declaration> declarations;
  std::vector<Testbed> testbeds;
  std::vector<Block> blocks;
  std::unordered_map<std::string, size_t> block_lookup;

  size_t get_block_index(std::string tag) {
    auto it = block_lookup.find(tag);
    return it != block_lookup.end() ? it->second : -1;
  }
};

// SECTION: Gameplay structs

struct Chunk {
  std::string text;
};

struct Option {
  std::vector<Chunk> text;
  bool is_available;
};

/** This contains actual Skald content */
struct Content {
  std::string attribution = "";
  std::vector<Chunk> text;
  std::vector<Option> options;
};

/** This contains a query from the Skald engine out to the client. This is
 * mostly method calls. */
struct Query {
  MethodCall call;
  std::string get_key() { return key_for_call(call); }
};

/** This carries error information for anything that goes so wrong that the
 *  engine has to stop */
const uint ERROR_UNKNOWN = 0;
const uint ERROR_EOF = 1;
const uint ERROR_EMPTY_MODULE = 2;
const uint ERROR_MODULE_TAG_NOT_FOUND = 3;
struct Error {
  uint code = 0;
  std::string message;
  size_t line_number;

  Error(uint code, std::string message, size_t line_number)
      : code(code), message(std::move(message)), line_number(line_number) {}
};

/** Will be sent back by the client as an answer to the current open query --
 *  will be undefined if no value is returned. Undefined will not be keyed into
 *  the map, which will be treated as falsy if used in a conditional. */
struct QueryAnswer {
  std::optional<SimpleRValue> val;
};

/** Empty struct signifying that the script is concluded. */
struct End {};

/** Will contain either a Content struct or a Query */
using Response = std::variant<Content, Query, Exit, GoModule, End, Error>;

/** Will be sent back by the client following a response, to indicate the
 * next action */
struct Action {
  int selection;
};

/** This is the phase of processing for a given beat. */
enum class ProcessPhase { Conditional, Resolution, Presentation, Application };

/** Marks where we are in the module, and what is expected from the client
 * next
 */
struct Cursor {
  ProcessPhase current_phase = ProcessPhase::Conditional;
  int current_block_index = 0;
  int current_beat_index = 0;
  std::vector<Query> resolution_stack;
};

enum ProgressResult {
  OK,
  END_OF_FILE,
};

class Engine {
private:
  std::unique_ptr<Module> current;
  std::unordered_map<std::string, SimpleRValue> state;
  std::unordered_map<std::string, SimpleRValue> query_cache;

  void build_state(const Module &module);

  // Util
  std::pair<Block &, Beat &> getCurrentBlockAndBeat();

  // Entrance and nav
  Response enter(int block, int beat);
  ProgressResult advance_cursor();

  // 1. Conditional phase

  void setup_beat();

  // 2. Resolution phase

  void process_beat();

  // 3. Presentation phase

  // 4. Application phase

  bool resolve_condition(const Conditional &cond);
  // STUB: Add a perform_operation() method
  std::string resolve_simple(const SimpleInsertion &ins);
  std::string resolve_tern(const TernaryInsertion &tern);
  std::vector<Chunk> resolve_text(const TextContent &text_content);
  Option resolve_option(const Choice &choice);

  Cursor cursor;
  // STUB: Next: see TODO>CURRENT
  Response next();

public:
  void load(std::string path);
  void trace(std::string path);

  // Actions
  /** Start the Skald engine at a particular tag. This sets the cursor to the
   * first beat in this block. */
  Response start_at(std::string tag);

  /** Start the engine at the first block in the file. This sets the cursor to
   * the first beat in the file as well. */
  Response start();

  /** Get the next beat in line, optionally making a choice as well */
  Response act(int choice_index = 0);

  /** Get the current response that's awaiting action */
  Response get_current();

  /** Provide an answer if the current response is a Query */
  Response answer(QueryAnswer a);

  std::string dbg_print_cache() {
    std::string ret;
    for (const auto &[key, value] : query_cache) {
      ret += key + ": " + rval_to_string(value) + "\n";
    }
    return ret;
  }
};

} // namespace Skald
