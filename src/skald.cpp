#include "../include/skald.h"
#include "debug.h"
#include "parse_state.h"
#include "skald_actions.h"
#include "skald_grammar.h"
#include "tao/pegtl/parse.hpp"
#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/trace.hpp>
#include <vector>

namespace pegtl = tao::pegtl;

namespace Skald {

// SECTION: UTIL

std::pair<Block &, BlockMember &> Engine::get_current_block_and_member() {
  dbg_out("get_current_b&m: block " << cursor.current_block_index
                                    << ", m: " << cursor.current_member_index);
  assert(current->blocks.size() > cursor.current_block_index);
  Block &block = current->blocks[cursor.current_block_index];
  assert(block.members.size() > cursor.current_member_index);
  BlockMember &member = block.members[cursor.current_member_index];
  return {block, member};
}

// SECTION: STATE

// void Engine::build_state(const Module &module) {
//   state.clear();
//   for (auto &var : module.declarations) {
//     // We don't overwrite variables that are already in state
//     if (!state.count(var.var.name))
//       state[var.var.name] = var.initial_value;
//   }
// }

bool compare(SimpleRValue ra, SimpleRValue rb,
             ConditionalAtom::Comparison comparison) {

  // Unequal types always return false in comparisons
  if (ra.index() != rb.index()) {
    return false;
  }

  // Different comparison logic per type
  return std::visit(
      [&](const auto &val_a) -> bool {
        using T = std::decay_t<decltype(val_a)>;
        const T &val_b = std::get<T>(rb);
        switch (comparison) {
        case ConditionalAtom::Comparison::EQUALS:
          return val_a == val_b;
        case ConditionalAtom::Comparison::NOT_EQUALS:
          return val_a != val_b;
        case ConditionalAtom::Comparison::MORE:
          return val_a > val_b;
        case ConditionalAtom::Comparison::LESS:
          return val_a < val_b;
        case ConditionalAtom::Comparison::MORE_EQUAL:
          return val_a >= val_b;
        case ConditionalAtom::Comparison::LESS_EQUAL:
          return val_a <= val_b;
        default:
          return false; // Any unhandled comparisons just return false
        }
      },
      ra);
}

bool equals(SimpleRValue ra, SimpleRValue rb) {
  return compare(ra, rb, ConditionalAtom::Comparison::EQUALS);
}

void Engine::warn(std::string tx, size_t ln) {
  warnings.push_back(Warning{.message = tx, .line_number = ln});
}

/** This extracts all queries needed to solve a Conditional. */
std::vector<Query> queries_for_conditional(const Conditional &cond) {

  std::vector<Query> result;

  for (const auto &item : cond.items) {
    if (auto *atom = std::get_if<ConditionalAtom>(&item)) {
      const MethodCall *a = rval_get_call(atom->a);
      const MethodCall *b = atom->b ? rval_get_call(*atom->b) : nullptr;
      if (a)
        result.push_back(Query{.call = *a,
                               .expects_response = true,
                               .line_number = a->line_number});
      if (b)
        result.push_back(Query{.call = *b,
                               .expects_response = true,
                               .line_number = b->line_number});

    } else if (auto *nested =
                   std::get_if<std::shared_ptr<Conditional>>(&item)) {
      auto queries = queries_for_conditional(**nested);
      result.insert(result.end(), queries.begin(), queries.end());
    }
  }

  return result;
}

/** This returns all the queries needed to resolve a given conditional */
std::vector<Query> queries_for_attached_condition(const AttachedCondition &c) {
  if (c.condition) {
    return queries_for_conditional(*c.condition);
  } else {
    return {};
  }
}

/** Returns all queries needed to execute a given operation */
std::vector<Query> queries_for_op(const Operation &op) {
  std::vector<Query> ret;
  auto *call = op_get_call(op);
  if (call)
    ret.push_back(Query{.call = *call,
                        .expects_response = false,
                        .line_number = call->line_number});
  return ret;
}

/** Returns all queries needed to execute a list of ops */
std::vector<Query> queries_for_operations(const std::vector<Operation> &ops) {
  std::vector<Query> ret;
  for (auto &op : ops) {
    auto q = queries_for_op(op);
    ret.insert(ret.end(), q.begin(), q.end());
  }
  return ret;
}

/** Returns all queries needed to display a list of choices. Ops are handled
 *  on player picking a choice, so aren't queried here. */
std::vector<Query> queries_for_choice_group(const ChoiceGroup &group) {
  std::vector<Query> ret;
  for (const auto &choice : group.choices) {
    auto q = queries_for_attached_condition(choice.condition);
    ret.insert(ret.end(), q.begin(), q.end());
  }
  return ret;
}

/** This is called in the Conditional beat phase, to check if the beat should
 * be processed at all */
std::vector<Query> queries_for_member_conditional(const BlockMember &mem) {
  return std::visit(
      [](const auto &value) -> std::vector<Query> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Beat>) {
          return queries_for_attached_condition(value.condition);
        } else if constexpr (std::is_same_v<T, LineOp>) {
          auto v = queries_for_attached_condition(value.condition);
          return v;
        } else if constexpr (std::is_same_v<T, ChoiceGroup>) {
          return queries_for_choice_group(value);
        }
      },
      mem);
}

std::vector<Query> queries_for_choice_exec(const Choice &choice) {
  std::vector<Query> ret;
  if (choice.operations.size() > 0) {
    auto op = queries_for_operations(choice.operations);
    ret.insert(ret.end(), op.begin(), op.end());
  }
  return ret;
}

// SECTION: RESOLVERS AND STATE

std::array<Engine::Scope, 3> Engine::scopes() {
  return {{{"global", global_state},
           {"module", module_state},
           {"local", local_state}}};
}

/** Returns value for given var name. Checks global, then module, then ad-hoc
 *  state, in that order. Returns bool false if nothing found, and throws
 *  warning. */
SimpleRValue Engine::var_get(const std::string var_name) {
  for (auto &s : scopes()) {
    auto it = s.map.find(var_name);
    if (it != s.map.end())
      return it->second;
  }
  warn("Getting value for " + var_name +
       ", and found nothing. Defaulting to `false`.");
  return false;
}

/** Sets var. Checks types against global, then module, then local state. If
 *  none are set, sets value as local var. */
std::optional<Error> Engine::var_set(const std::string var_name,
                                     const RValue &rval, size_t ln) {
  auto val = resolve_rval_to_simple(rval);
  auto t = srval_get_type(val);

  for (auto &s : scopes()) {
    auto it = s.map.find(var_name);
    if (it == s.map.end())
      continue;
    if (srval_get_type(it->second) != t) {
      return Error(ERROR_TYPE_MISMATCH,
                   "Tried to set " + std::string(s.name) + " var " + var_name +
                       " to " + rval_to_string(val),
                   ln);
    }
    s.map[var_name] = val;
    return std::nullopt;
  }

  // Not found anywhere; define as local.
  local_state[var_name] = val;
  return std::nullopt;
}

/** Toggles a bool var in whichever scope (global, module, local) holds it. */
std::optional<Error> Engine::var_switch(const std::string var_name, size_t ln) {
  for (auto &s : scopes()) {
    auto it = s.map.find(var_name);
    if (it == s.map.end())
      continue;
    auto b = srval_get_bool(it->second);
    if (!b) {
      return Error(ERROR_TYPE_MISMATCH,
                   "Tried to switch " + var_name + ", but it is not a boolean.",
                   ln);
    }
    s.map[var_name] = !*b;
    return std::nullopt;
  }
  warn("Tried to switch " + var_name +
           ", and found nothing. Setting it as a local variable to `false`.",
       ln);
  local_state[var_name] = false;
  return std::nullopt;
}

/** Will mathematically mutate a float or int. Errors if string or bool, or if
 * arg is string or bool. floats and ints can be used interchangeably (int -
 * float will round down). If sign is false, will subtract. */
std::optional<Error> Engine::var_add(const std::string var_name,
                                     const RValue &rval, bool sign, size_t ln) {

  // Get the arg and make sure it's a number
  auto arg = resolve_rval_to_simple(rval);
  auto arg_type = srval_get_type(arg);
  if (arg_type != ValueType::INT && arg_type != ValueType::FLOAT) {
    return Error(ERROR_TYPE_MISMATCH,
                 "Tried to add non-numeric value " + rval_to_string(arg) +
                     " to " + var_name + ".",
                 ln);
  }

  // Convert to float because it's more flexible
  float arg_f = arg_type == ValueType::INT ? (float)*srval_get_int(arg)
                                           : *srval_get_float(arg);

  // Handle subtraction
  if (!sign)
    arg_f = -arg_f;

  // Global -> Module -> Local
  for (auto &s : scopes()) {
    auto it = s.map.find(var_name);
    if (it == s.map.end())
      continue;

    // If int, convert arg to int and add
    auto var_type = srval_get_type(it->second);
    if (var_type == ValueType::INT) {
      s.map[var_name] = *srval_get_int(it->second) + (int)arg_f;
      return std::nullopt;
    }

    // Same but for floats
    if (var_type == ValueType::FLOAT) {
      s.map[var_name] = *srval_get_float(it->second) + arg_f;
      return std::nullopt;
    }

    // If we have the var but it's not a number, error
    return Error(ERROR_TYPE_MISMATCH,
                 "Tried to add to " + std::string(s.name) + " var " + var_name +
                     ", but it is not numeric.",
                 ln);
  }

  // If var not found, error
  return Error(ERROR_VAR_UNDEFINED,
               "Tried to add to undefined var " + var_name + ".", ln);
}

/** Resolves an rvalue (potentially including method calls or variables) down
 * to a simple value based on the current state and query cache. If no key
 * exists, boolean false will be returned. */
SimpleRValue Engine::resolve_rval_to_simple(const RValue &rval) {
  return std::visit(
      [this](const auto &value) -> SimpleRValue {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::shared_ptr<MethodCall>>) {
          auto key = key_for_call(*value);
          auto it = query_cache.find(key);
          auto val = it != query_cache.end() ? it->second : SimpleRValue{false};
          if (it == query_cache.end()) {
            warn("Tried to resolve query key " + key +
                 " and got nothing; defaulting to `false`.");
          }
          return val;
        } else if constexpr (std::is_same_v<T, Variable>) {
          return var_get(value.name);
        } else {
          return value;
        }
      },
      rval);
}

bool Engine::resolve_conditional_atom(const ConditionalAtom &atom) {
  SimpleRValue ra = resolve_rval_to_simple(atom.a);

  // First, handle single-value checks
  switch (atom.comparison) {
  case ConditionalAtom::Comparison::TRUTHY:
    return is_simple_rval_truthy(ra);
  case ConditionalAtom::Comparison::NOT_TRUTHY:
    return !is_simple_rval_truthy(ra);
  default:
    break;
  }

  // Now comparisons
  SimpleRValue rb = resolve_rval_to_simple(*atom.b);
  return compare(ra, rb, atom.comparison);
}

// STUB: Record the result of the conditional resolution so we can return it

bool Engine::resolve_conditional_item(const ConditionalItem &item) {
  return std::visit(
      [&](const auto &c) -> bool {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, ConditionalAtom>) {
          return resolve_conditional_atom(c);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Conditional>>) {
          return resolve_condition(*c);
        }
      },
      item);
}

bool Engine::resolve_condition(const Conditional &cond) {
  for (auto &i : cond.items) {
    bool result = resolve_conditional_item(i);

    // If the conditional is OR, any item can be true to validate
    if (result && cond.type == Conditional::OR)
      return true;

    // If AND, *any* false item validates the whole conditional to false
    if (!result && cond.type == Conditional::AND)
      return false;
  }
  // If we're still here as an OR nothing was true; vice versa for AND.
  return cond.type == Conditional::AND;
}

bool Engine::resolve_condition(const std::optional<Conditional> &cond) {
  if (cond)
    return resolve_condition(*cond);
  return true;
}
bool Engine::resolve_condition(const AttachedCondition &cond) {
  return resolve_condition(cond.condition);
}

/** Internal helper function to print values as an engine output */
std::string string_for_val(SimpleRValue val) {
  return std::visit(
      [](const auto &value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::string>) {
          return value;
        } else {
          return std::to_string(value);
        }
      },
      val);
}

std::string Engine::resolve_simple(const SimpleInsertion &ins) {
  auto val = resolve_rval_to_simple(ins.rvalue);
  return string_for_val(val);
}

std::string Engine::resolve_tern(const TernaryInsertion &tern) {
  auto check = resolve_rval_to_simple(tern.check);
  if (tern.check_truthy) {
    // This works because simple ternaries are encoded [true, false]
    bool truthy = is_simple_rval_truthy(check);
    auto val =
        resolve_rval_to_simple(std::get<1>(tern.options[truthy ? 0 : 1]));
    return string_for_val(val);
  }
  for (auto &option : tern.options) {
    auto val = resolve_rval_to_simple(std::get<0>(option));
    if (equals(check, val)) {
      auto ret = resolve_rval_to_simple(std::get<1>(option));
      return string_for_val(ret);
    }
  }
  return "";
}

std::optional<Error> Engine::do_operation(Operation &op) {
  dbg_out("    x-x " << dbg_dsc_op(op));
  std::optional<Error> ret = std::nullopt;
  std::visit(
      [&](auto &o) {
        using T = std::decay_t<decltype(o)>;
        if constexpr (std::is_same_v<T, Move>) {
          dbg_out("   ---> going to " << o.target_tag);
          cursor.queued_transition = o.target_tag;
        } else if constexpr (std::is_same_v<T, MethodCall>) {
          dbg_out("   (already called)");
          // This has already been done in the resolution phase; do nothing
        } else if constexpr (std::is_same_v<T, Mutation>) {
          // auto val = o.rvalue ? resolve_rval_to_simple(*o.rvalue) : false;
          switch (o.type) {
          case Mutation::Type::EQUATE:
            if (!o.rvalue) {
              ret = Error(ERROR_UNEXPECTED_NULL,
                          "Tried to set " + o.lvalue +
                              " to an rvalue that is unexpectedly null.",
                          o.line_number);
              return;
            }
            ret = var_set(o.lvalue, *o.rvalue, o.line_number);
            break;
          case Mutation::Type::ADD:
            ret = var_add(o.lvalue, *o.rvalue, true, o.line_number);
            break;
          case Mutation::Type::SUBTRACT:
            ret = var_add(o.lvalue, *o.rvalue, false, o.line_number);
            break;
          case Mutation::Type::SWITCH:
            ret = var_switch(o.lvalue);
            break;
          }
        } else if constexpr (std::is_same_v<T, GoModule>) {
          dbg_out("    --->> CHANGING MODULE");
          cursor.queued_go = &o;
        } else if constexpr (std::is_same_v<T, Exit>) {
          dbg_out("    ---X EXITING");
          cursor.queued_exit = &o;
        }
      },
      op);
  return ret;
}

/** Queues the member's conditional for processing. Called on cursor movement
 * and by the engine on first enter. */
void Engine::setup_member() {
  dbg_out("Engine::setup_member");
  auto [block, member] = get_current_block_and_member();
  cursor.resolution_stack = queries_for_member_conditional(member);
  cursor.is_preprocessed = true;
}

std::vector<Chunk> Engine::resolve_text(const TextContent &text_content) {
  std::vector<Chunk> ret;
  for (auto &part : text_content.parts) {

    // Resolve the part into a string
    std::string part_text = std::visit(
        [&](const auto &value) -> std::string {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, std::string>) {
            return value;
          } else if constexpr (std::is_same_v<T, SimpleInsertion>) {
            return resolve_simple(value);
          } else if constexpr (std::is_same_v<T, TernaryInsertion>) {
            return resolve_tern(value);
          }
        },
        part);

    // Add to the return
    ret.push_back(Chunk{part_text});
  }
  return ret;
}

/** Advances the cursor one beat.
 *  - from_line_number is for error logging.
 */
std::optional<Error> Engine::advance_cursor(int from_line_number) {

  // If we have a queued transition, find the relevant block and jump there,
  // consuming the queued transition tag as we do so.
  if (cursor.queued_transition.length() > 0) {
    auto new_index = current->get_block_index(cursor.queued_transition);
    if (new_index == -1)
      return Error(ERROR_MODULE_TAG_NOT_FOUND,
                   "Module tag not found: " + cursor.queued_transition,
                   from_line_number);

    cursor.current_block_index = new_index;

    // Start "above the top" of the next block in order to handle empty
    // blocks where we immediately move to the next one.
    cursor.current_member_index = -1;
    cursor.queued_transition = "";
  }

  // Now we move into the block.
  Block &block = current->blocks[cursor.current_block_index];
  cursor.current_member_index++; // aka will -> 0 after a transition

  // Are we at the end of the block?
  if (cursor.current_member_index >= block.members.size()) {
    cursor.current_block_index++;
    cursor.current_member_index = 0;
    if (cursor.current_block_index >= current->blocks.size()) {
      return Error(ERROR_EOF, "Unexpectedly reached the end of the file",
                   from_line_number);
    }
  }

  dbg_out("CURSOR --> " << cursor.current_block_index << ", "
                        << cursor.current_member_index);

  // Queue up any conditional queries etc needed to run our next member
  setup_member();

  // And return ok.
  return std::nullopt;
}

// SECTION: CORE ITERATOR

/** This steps forward to whatever the next thing is that needs to happen, and
 *  as soon as any response is pending, returns it. */
Response Engine::next() {
  dbg_out("Engine::next()");
  // Processor loop
  int debug_blocker = 0;

  // This will loop forever until something returns. Basically steps through
  // the module until something happens, or until we need to return an error.
  while (true) {
    dbg_out(">>> next looping " << debug_blocker);

    // Debug stopper; while developing, lock loop iterations to 50 to keep
    // from getting stuck in a permaloop.
    debug_blocker++;
    if (debug_blocker > 50) {
      dbg_out(">>> Engine::next infinite loop exception! breaking.");
      return End("Exited due to infinite loop (50 iteration debug threshold "
                 "reached)!");
    }

    /// EXIT and GO ///

    if (cursor.queued_exit) {
      return *cursor.queued_exit;
    }
    if (cursor.queued_go) {
      return *cursor.queued_go;
    }

    /// Query Stack ///

    if (cursor.resolution_stack.size() > 0) {
      return cursor.resolution_stack.back();
    }

    /// Block Logic and Interaction ///

    auto [block, member] = get_current_block_and_member();

    assert(cursor.is_preprocessed);

    // STUB: Hood open: step through until we hit a response.

    std::optional<Response> response = std::visit(
        [&](const auto &mem) -> std::optional<Response> {
          using T = std::decay_t<decltype(mem)>;
          if constexpr (std::is_same_v<T, Beat>) {
            /// Beats ///
            auto content = Content{};
            content.text = resolve_text(mem.content);
            content.attribution = mem.attribution;
            return content;

          } else if constexpr (std::is_same_v<T, LineOp()>) {
            /// LineOps ///
            return do_operation(mem.op);
          } else if constexpr (std::is_same_v<T, ChoiceGroup()>) {
            auto grp = OptionGroup{};
            for (auto &choice : mem.choices) {
              auto opt = Option{};
              opt.text = resolve_text(choice.content);
              opt.is_available = resolve_condition(choice.condition);
              grp.options.push_back(opt);
            }
            return grp;
          }
          return std::nullopt;
        },
        member);

    // If we got something out of the member, return it; otherwise loop de
    // loop.
    if (response)
      return *response;

    // If no response, step forward
    auto err = advance_cursor();
    if (err)
      return *err;
  } // end of main next() loop.

  return Error(ERROR_UNKNOWN,
               "Exited the next() main control loop without returning a value!",
               0);
}

// SECTION: PLAYER INPUT

/** Called by client on continue (`act(0)`) or choice (`act(n)`). */
Response Engine::act(int choice_index) {
  auto [block, member] = get_current_block_and_member();
  std::optional<Error> err;
  std::visit(
      [&](const auto &mem) {
        using T = std::decay_t<decltype(mem)>;
        if constexpr (std::is_same_v<T, Beat>) {

          /// Act on a beat: CONTINUE ///

          err = advance_cursor(mem.line_number);
        } else if constexpr (std::is_same_v<T, LineOp>) {

          /// Act on a line op: ERROR ///

          err = Error(ERROR_UNEXPECTED_ACT,
                      "Got a user act input on a LineOp; state may be broken.",
                      mem.line_number);
        } else if constexpr (std::is_same_v<T, ChoiceGroup()>) {

          /// Act on a choice group: CHOICE ///

          if (choice_index >= mem.choices.size()) {
            err = Error(ERROR_CHOICE_OUT_OF_BOUNDS,
                        "You picked choice " + std::to_string(choice_index) +
                            ", but there are only " +
                            std::to_string(mem.choices.size()) +
                            " choices available!",
                        mem.line_number);
            return;
          }

          auto &choice = mem.choices[choice_index];

          // Make sure the selection is valid
          if (!resolve_condition(choice.condition)) {
            err = Error(ERROR_CHOICE_UNAVAILABLE,
                        "You picked choice " + std::to_string(choice_index) +
                            ", but it is unavailable.",
                        choice.line_number);

            return;
          }

          // Process any queries that are needed
          cursor.choice_selection = choice_index;
          cursor.resolution_stack = queries_for_choice_exec(choice);
        }
      },
      member);

  // Handle any errors thrown by the members
  if (err)
    return *err;

  return next();
}

Response Engine::answer(std::optional<QueryAnswer> answer) {
  if (cursor.resolution_stack.empty()) {
    return Error(ERROR_RESOLUTION_QUEUE_EMPTY,
                 "Received an answer, but the resolution queue is empty!",
                 0); // TODO: add a "last at" line number and use it here
  }
  auto &answering = cursor.resolution_stack.back();
  if (answering.expects_response) {
    if (!answer) {
      return Error(ERROR_EXPECTED_ANSWER,
                   "Expected an answer for " + answering.get_key() +
                       ", but received none.",
                   answering.line_number);
    }
    auto key = answering.get_key();
    auto a = *answer;
    if (a.val) {
      query_cache.insert_or_assign(key, *a.val);
    } else {
      query_cache.erase(key);
    }
  }
  cursor.resolution_stack.pop_back();
  return next();
}

// SECTION: MODULE ENTRY

Response Engine::start_at(std::string tag) {
  auto start_index = current->get_block_index(tag);
  if (start_index < 0) {
    return Error(ERROR_MODULE_TAG_NOT_FOUND,
                 "No block was found for tag: " + tag, 0);
  }
  return enter(start_index, 0);
}

Response Engine::start() {
  dbg_out("engine start");
  if (current->blocks.size() < 1) {
    return Error(ERROR_EMPTY_MODULE,
                 "No blocks were found in the current module!", 0);
  }
  return enter(0, 0);
}

Response Engine::enter(int block, int index) {
  dbg_out("Engine::enter");
  cursor.reset();
  cursor.current_block_index = block;
  cursor.current_member_index = 0;
  setup_member();
  return next();
}

// SECTION: FILE LOADING AND PARSING

void Engine::load(std::string path) {
  try {
    pegtl::file_input in(path);
    dbg_out("Loaded file: " << path);

    ParseState pstate(path);

    if (pegtl::parse<grammar, action>(in, pstate)) {
      dbg_out("Parse successful!");
    } else {
      dbg_out("Parse failed!");
    }

    dbg_out(">>> Parse results:\n");

    dbg_out("MODULE VARS:");
    for (const auto &dec : pstate.module.module_vars) {
      dbg_out(" - " << dec.var.dbg_desc() << " = "
                    << rval_to_string(dec.initial_value));
    }

    dbg_out("TESTBEDS:");
    for (const auto &testbed : pstate.module.testbeds) {
      dbg_out(testbed.dbg_desc());
    }

    dbg_out("\nSTRUCTURE:");
    // Print details about each block
    for (const auto &block : pstate.module.blocks) {
      dbg_out("\n- Block '" << block.tag << "': " << block.members.size()
                            << " members");
      for (const auto &mem : block.members) {

        std::visit(
            [](const auto &member) {
              using T = std::decay_t<decltype(member)>;
              if constexpr (std::is_same_v<T, Beat>) {
                dbg_out("  - Beat: " << member.dbg_desc());
              } else if constexpr (std::is_same_v<T, ChoiceGroup>) {
                dbg_out("  - ChoiceGroup: ");
                for (const auto &choice : member.choices) {
                  dbg_out("    > Choice: " << choice.dbg_desc());
                  dbg_out(dbg_desc_ops(choice.operations));
                }
              } else if constexpr (std::is_same_v<T, LineOp>) {
                dbg_out("  - LineOp: " << member.dbg_desc());
              }
            },
            mem);
      }
    }

    // Grab the finished module from the parse state
    current = std::make_unique<Module>(std::move(pstate.module));

  } catch (const pegtl::parse_error &e) {
    dbg_out("Parse error: " << e.what());
  } catch (const std::exception &e) {
    dbg_out("Error: " << e.what());
  }
}

void Engine::trace(std::string path) {
  pegtl::file_input in(path);
  dbg_out("Loaded file: " << path << "\n");

  pegtl::standard_trace<grammar>(in);
}

} // namespace Skald
