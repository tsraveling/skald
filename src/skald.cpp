#include "../include/skald.h"
#include "codex_actions.h"
#include "codex_grammar.h"
#include "codex_parse_state.h"
#include "debug.h"
#include "parse_state.h"
#include "skald_actions.h"
#include "skald_grammar.h"
#include "tao/pegtl/parse.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/trace.hpp>
#include <variant>
#include <vector>

namespace pegtl = tao::pegtl;

namespace Skald {

// SECTION: UTIL

/** Gets the current block for the cursor */
Block &Engine::cursor_block() {
  return current->blocks.at(cursor.current_block_index);
}

/** Gets current MainBlockMember (BM or CT) where we already know block */
MainBlockMember &Engine::cursor_mbm(Block &block) {
  return block.members.at(cursor.current_member_index);
}

/** Gets current MainBlockMember (BM or CT) from scratch */
MainBlockMember &Engine::cursor_mbm() {
  auto &block = cursor_block();
  return cursor_mbm(block);
}

/** Gets current BlockMember (Mem or CG), including in a CT, where we already
 * know the parent MBM (which either is this, or is the parent) */
BlockMember &Engine::cursor_bm(MainBlockMember &mbm) {
  // Return BM if that's what this is
  if (auto *bm = std::get_if<BlockMember>(&mbm)) {
    return *bm;
  }

  // Otherwise step into the CG
  assert(cursor.entered_thread_block); // Must have resolved a cond
  auto &chain = std::get<ConditionalChain>(mbm);
  auto &cb = chain.cond_blocks.at(cursor.thread_block);
  return cb.members.at(cursor.thread_member);
}

/** Gets current BlockMember (Mem or CG), including in a CT, where we don't
 * know the parent MBM (which either is this, or is the parent) */
BlockMember &Engine::cursor_bm() {
  auto &mbm = cursor_mbm();
  return cursor_bm(mbm);
}

/** Gets current member; may be MBM:BM:Mem, may be child, or may be child of a
 * choice in a CG. In this case we know the parent / superclass BM. */
Member &Engine::cursor_mem(BlockMember &bm) {
  if (auto *cg = std::get_if<ChoiceGroup>(&bm)) {
    assert(cursor.choice_selection >= 0); // Must be in a choice
    auto &choice = cg->choices.at(cursor.choice_selection); // get choice
    return choice.members.at(cursor.choice_thread_index);
  }
  return std::get<Member>(bm);
}

/** Gets current member; may be MBM:BM:Mem, may be child, or may be child of a
 * choice in a CG. */
Member &Engine::cursor_mem() {
  auto &bm = cursor_bm();
  return cursor_mem(bm);
}

// SECTION: STATE
void Engine::init_state() {
  local_state.clear();
  module_state.clear();
  global_state.clear();
  if (codex) {
    for (auto &var : codex->global_vars) {
      global_state[var.var.name] = var.initial_value;
    }
  }
}

void Engine::build_state(const Module &module) {
  local_state.clear();
  for (auto &var : module.module_vars) {
    auto it = module_state.find(var.var.name);
    if (it == module_state.end()) {
      module_state[var.var.name] = var.initial_value;
      continue;
    }
    // SimpleRValue index order matches ValueType enum order
    // (string, bool, int, float).
    auto existing_type = static_cast<ValueType>(it->second.index());
    if (existing_type != var.var.type) {
      warn("Module var '" + var.var.name +
               "' redeclared with different type; keeping existing value.",
           var.line_number);
    }
  }
}

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
std::vector<MethodCallGet> queries_for_conditional(const Conditional &cond) {

  std::vector<MethodCallGet> result;

  for (const auto &item : cond.items) {
    if (auto *atom = std::get_if<ConditionalAtom>(&item)) {
      const MethodCall *a = rval_get_call(atom->a);
      const MethodCall *b = atom->b ? rval_get_call(*atom->b) : nullptr;
      if (a)
        result.push_back(
            MethodCallGet{.call = *a, .line_number = a->line_number});
      if (b)
        result.push_back(
            MethodCallGet{.call = *b, .line_number = b->line_number});

    } else if (auto *nested =
                   std::get_if<std::shared_ptr<Conditional>>(&item)) {
      auto queries = queries_for_conditional(**nested);
      result.insert(result.end(), queries.begin(), queries.end());
    }
  }

  return result;
}

/** This returns all the queries needed to resolve an AC (handles null case) */
std::vector<MethodCallGet>
queries_for_attached_condition(const AttachedCondition &c) {
  if (c.condition) {
    return queries_for_conditional(*c.condition);
  } else {
    return {};
  }
}

std::vector<MethodCallGet> queries_for_mutation(const Mutation &m) {
  if (m.rvalue) {
    if (const MethodCall *call = rval_get_call(*m.rvalue)) {
      return {MethodCallGet{.call = *call, .line_number = call->line_number}};
    }
  }
  return {};
}

/** Returns all queries needed to display a list of choices. Ops are handled
 *  on player picking a choice, so aren't queried here. */
std::vector<MethodCallGet> queries_for_choice_group(const ChoiceGroup &group) {
  std::vector<MethodCallGet> ret;
  for (const auto &choice : group.choices) {
    auto q = queries_for_attached_condition(choice.condition);
    ret.insert(ret.end(), q.begin(), q.end());
  }
  return ret;
}

/** This is called in the Conditional beat phase, to check if the beat should
 * be processed at all */
std::vector<MethodCallGet>
queries_for_member_conditional(const BlockMember &mem) {
  return std::visit(
      [](const auto &value) -> std::vector<MethodCallGet> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Member>) {
          return queries_for_attached_condition(value.ac);
        } else if constexpr (std::is_same_v<T, ChoiceGroup>) {
          return queries_for_choice_group(value);
        }
        return {};
      },
      mem);
}

// SECTION: RESOLVERS AND STATE

std::array<Engine::ScopeMap, 3> Engine::scopes() {
  return {{{VarScope::GLOBAL, global_state},
           {VarScope::MODULE, module_state},
           {VarScope::LOCAL, local_state}}};
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
std::variant<Error, VarScope> Engine::var_set(const std::string var_name,
                                              const SimpleRValue &rval,
                                              size_t ln) {
  auto t = srval_get_type(rval);

  for (auto &s : scopes()) {
    auto it = s.map.find(var_name);
    if (it == s.map.end())
      continue;
    if (srval_get_type(it->second) != t) {
      return Error(ERROR_TYPE_MISMATCH,
                   "Tried to set " + std::string(scope_to_string(s.scope)) +
                       " var " + var_name + " to " + rval_to_string(rval),
                   ln);
    }
    s.map[var_name] = rval;
    return s.scope;
  }

  // Not found anywhere; define as local.
  local_state[var_name] = rval;
  return VarScope::LOCAL;
}

/** Toggles a bool var in whichever scope (global, module, local) holds it. */
std::variant<Error, VarScope> Engine::var_switch(const std::string var_name,
                                                 size_t ln) {
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
    return s.scope;
  }
  warn("Tried to switch " + var_name +
           ", and found nothing. Setting it as a local variable to `false`.",
       ln);
  local_state[var_name] = false;
  return VarScope::LOCAL;
}

/** Will mathematically mutate a float or int. Errors if string or bool, or if
 * arg is string or bool. floats and ints can be used interchangeably (int -
 * float will round down). If sign is false, will subtract. */
std::variant<Error, VarScope> Engine::var_add(const std::string var_name,
                                              const SimpleRValue &rval,
                                              bool sign, size_t ln) {

  // Make sure it's a number
  auto arg_type = srval_get_type(rval);
  if (arg_type != ValueType::INT && arg_type != ValueType::FLOAT) {
    return Error(ERROR_TYPE_MISMATCH,
                 "Tried to add non-numeric value " + rval_to_string(rval) +
                     " to " + var_name + ".",
                 ln);
  }

  // Convert to float because it's more flexible
  float arg_f = arg_type == ValueType::INT ? (float)*srval_get_int(rval)
                                           : *srval_get_float(rval);

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
      return s.scope;
    }

    // Same but for floats
    if (var_type == ValueType::FLOAT) {
      s.map[var_name] = *srval_get_float(it->second) + arg_f;
      return s.scope;
    }

    // If we have the var but it's not a number, error
    return Error(ERROR_TYPE_MISMATCH,
                 "Tried to add to " + std::string(scope_to_string(s.scope)) +
                     " var " + var_name + ", but it is not numeric.",
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

std::variant<Error, Notification> Engine::do_mutation(Mutation &o) {
  std::variant<Error, VarScope> res = VarScope::LOCAL;
  std::optional<SimpleRValue> rv;
  if (o.rvalue)
    rv = resolve_rval_to_simple(*o.rvalue);
  switch (o.type) {
  case Mutation::Type::EQUATE:
    assert(rv); // parser must supply this
    res = var_set(o.lvalue, *rv, o.line_number);
    break;
  case Mutation::Type::ADD:
    assert(rv); // parser must supply this
    res = var_add(o.lvalue, *rv, true, o.line_number);
    break;
  case Mutation::Type::SUBTRACT:
    assert(rv); // parser must supply this
    res = var_add(o.lvalue, *rv, false, o.line_number);
    break;
  case Mutation::Type::SWITCH:
    res = var_switch(o.lvalue, o.line_number);
    break;
  }
  if (auto *err = std::get_if<Error>(&res)) {
    return *err;
  }
  VarScope s = *std::get_if<VarScope>(&res);
  return Notification{
      .var_name = o.lvalue,
      .mut_type = o.type,
      .rval = rv,
      .scope = s,
  };
}

std::optional<Response> Engine::do_member(Member &mem) {
  std::optional<Response> ret;
  std::visit(
      [&](auto &m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, Beat>) {
          /// BEAT ///
          auto content = Content{};
          content.text = resolve_text(m.content);
          content.attribution = m.attribution;
          ret = std::move(content);
        } else if constexpr (std::is_same_v<T, Move>) {
          /// MOVE ///
          cursor.queued_transition = m.target_tag;
        } else if constexpr (std::is_same_v<T, MethodCall>) {
          /// METHOD ///
          dbg_out("   -()() METHOD CALL POST");
          ret = MethodCallPost{.call = m, .line_number = m.line_number};
        } else if constexpr (std::is_same_v<T, Mutation>) {
          /// MUTATION ///
          auto mres = do_mutation(m);
          std::visit([&](auto &v) { ret = v; }, mres);
          dbg_out("    -o-o MUTATION");
        } else if constexpr (std::is_same_v<T, GoModule>) {
          /// GO ///
          dbg_out("    --->> CHANGING MODULE");
          cursor.queued_go = &m;
          ret = *cursor.queued_go;
        } else if constexpr (std::is_same_v<T, Exit>) {
          /// EXIT ///
          dbg_out("    ---X EXITING");
          cursor.queued_exit = &m;
          ret = *cursor.queued_exit;
        }
      },
      mem.body);
  return ret;
}

/** Set up member (AC, mutation RValues. method args not supported yet) */
void Engine::setup_member(Member &member) {
  cursor.add_to_res_stack(queries_for_attached_condition(member.ac));
  if (auto *mut = std::get_if<Mutation>(&member.body)) {
    cursor.add_to_res_stack(queries_for_mutation(*mut));
  }
  dbg_out("Engine::setup_member");
  cursor.is_preprocessed = true;
}

/** Sets up a block member (CG or Mem) directly */
void Engine::setup_bm(BlockMember &member) {
  if (auto *cg = std::get_if<ChoiceGroup>(&member)) {
    cursor.add_to_res_stack(queries_for_choice_group(*cg));
  }

  dbg_out("Engine::setup_bm");
  cursor.resolution_stack = queries_for_member_conditional(member);
  cursor.is_preprocessed = true;
}

/** Queues the member's conditional for processing. Called on cursor movement
 * and by the engine on first enter. */
void Engine::setup_mbm(MainBlockMember &mbm) {
  dbg_out("Engine::setup_mbm");

  /// ENTER CONDITIONAL CHAIN ///
  if (auto *cc = std::get_if<ConditionalChain>(&mbm)) {
    assert(!cursor.entered_thread_block); // must not already be in cond thread
    cursor.thread_block = 0;
    cursor.thread_member = 0;
    cursor.entered_thread_block = false;
    auto &cb = cc->cond_blocks[cursor.thread_block];
    assert(cb.cond); // First cond block must not be an else
    cursor.resolution_stack = queries_for_attached_condition(cb.cond);
    cursor.is_preprocessed = true;
    return;
  }

  /// OTHERWISE: BLOCK MEMBER ///
  auto &bm = std::get<BlockMember>(mbm);
  setup_bm(bm);
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

  /// HANDLE TRANSITIONS ///

  if (cursor.queued_transition.length() > 0) {
    auto new_index = current->get_block_index(cursor.queued_transition);
    if (new_index == -1)
      return Error(ERROR_MODULE_TAG_NOT_FOUND,
                   "Module tag not found: " + cursor.queued_transition,
                   from_line_number);

    cursor.current_block_index = new_index;
    cursor.entered_thread_block = false;
    cursor.thread_block = 0;
    cursor.thread_member = 0;
    cursor.choice_selection = -1; // Drop out of choice as well
    cursor.choice_thread_index = 0;

    // Start "above the top" of the next block in order to handle empty
    // blocks where we immediately move to the next one.
    cursor.current_member_index = -1;
    cursor.queued_transition = "";
  }

  /// GO TO MODULE ///

  if (cursor.queued_go) {
    auto res = load(cursor.queued_go->module_path);
    if (!res.ok) {
      // Just use first error; that's what failed it
      for (auto &ex : res.exceptions) {
        if (ex.severity == ParseError::ERROR) {
          return Error(ERROR_LOADING_MODULE, ex.msg, ex.pos.line);
        }
      }
      return Error(
          ERROR_LOADING_MODULE,
          "Unknown error loading module: " + cursor.queued_go->module_path, 0);
    }
    auto tag = cursor.queued_go->start_in_tag;
    if (tag.length() > 0) {
      start_at(tag);
    } else {
      start();
    }
    cursor.current_member_index = -1;
  }

  // CHECK: Does a CC work if it's the first child following a transition?

  /// CONDITIONAL CHAINS ///
  if (cursor.current_member_index >= 0) {
    //
    auto &cc_mbm = cursor_mbm();
    if (auto *cc = std::get_if<ConditionalChain>(&cc_mbm)) {
      // Then advance through chain
      assert(cc->cond_blocks.size() > cursor.thread_block);
      auto &cb = cc->cond_blocks[cursor.thread_block];
      assert(cb.members.size() > cursor.thread_member); // must start in bounds
      cursor.thread_member++;
      if (cb.members.size() > cursor.thread_member) {
        auto &mem = cb.members[cursor.thread_member];
        setup_bm(mem);
        return std::nullopt;
      } else {
        // End of block; we exit.
        cursor.entered_thread_block = false;
        cursor.thread_member = 0;
        cursor.thread_block = 0;
      }
    }
  }

  /// NORMAL MEMBERS ///
  cursor.current_member_index++; // aka will -> 0 after a transition

  /// END OF BLOCKS ///
  Block *block = &cursor_block();
  while (cursor.current_member_index >= block->members.size()) {
    cursor.current_block_index++;
    cursor.current_member_index = 0;
    if (cursor.current_block_index >= current->blocks.size()) {
      return Error(ERROR_EOF, "Unexpectedly reached the end of the file",
                   from_line_number);
    }
    block = &cursor_block();
  }

  // If we get here, that means the block has members.

  dbg_out("   +C => " << cursor.current_block_index << ", "
                      << cursor.current_member_index);

  // Queue up any conditional queries etc needed to run our next member
  auto &mbm = cursor_mbm(*block);
  setup_mbm(mbm);

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

    // Debug stopper; while developing, lock loop iterations to 50 to keep
    // from getting stuck in a permaloop.
    debug_blocker++;
    if (debug_blocker > 50) {
      return Error{
          ERROR_UNKNOWN,
          "Module was caught in an infinite loop; bailed out at 50 iterations.",
          0};
    }

    /// EXIT and GO ///

    if (cursor.queued_exit) {
      return *cursor.queued_exit;
    }
    if (cursor.queued_go) {
      dbg_out(">>> next: queued_go!");
      return *cursor.queued_go;
    }

    /// Query Stack ///

    if (cursor.resolution_stack.size() > 0) {
      return cursor.resolution_stack.back();
    }

    assert(cursor.is_preprocessed); // Queries already must be handled

    /// Conditional Chains ///

    auto &mbm = cursor_mbm();
    if (auto *cc = std::get_if<ConditionalChain>(&mbm)) {
      // Get current cond block
      assert(cc->cond_blocks.size() > cursor.thread_block);
      auto &cb = cc->cond_blocks[cursor.thread_block];

      // Resolve conditional (auto handles else also)
      if (resolve_condition(cb.cond)) {

        // Enter into this block (cursor.thread_block)
        cursor.entered_thread_block = true;
      } else {
        cursor.thread_block++;

        // If at end of thread, just drop out
        if (cursor.thread_block >= cc->cond_blocks.size()) {
          auto err = advance_cursor();
          if (err)
            return *err;
          continue;
        } else {
          // Get the next block in line
          auto &nb = cc->cond_blocks[cursor.thread_block];

          // else block (no conditional)?
          if (!nb.cond) {
            cursor.entered_thread_block = true;
          } else {
            // elseif block: add to res stack and loop de loop.
            cursor.resolution_stack = queries_for_attached_condition(nb.cond);
            continue; // Will get resolved on next main loop iteration
          }
        }
      }
    }

    /// Block Logic and Interaction ///
    auto &bm = cursor_bm(mbm);

    std::optional<Response> response = std::visit(
        [&](auto &mem) -> std::optional<Response> {
          using T = std::decay_t<decltype(mem)>;
          if constexpr (std::is_same_v<T, Member>) {
            /// NORMAL MEMBERS ///
            return do_member(mem);
          } else if constexpr (std::is_same_v<T, ChoiceGroup>) {
            /// CHOICE GROUPS ///
            dbg_out(
                "next -> visit ChoiceGroup. c_s=" << cursor.choice_selection);

            // Execute choice if we made one
            if (cursor.choice_selection >= 0) {
              assert(mem.choices.size() >
                     cursor.choice_selection); // Must be selectable
              Choice &choice = mem.choices[cursor.choice_selection];

              if (cursor.choice_thread_index >= choice.members.size()) {
                dbg_out("passed end of choice member list, resetting and "
                        "moving on (c_t_i = 0)");
                cursor.choice_selection = -1;
                cursor.choice_thread_index = 0;
                return std::nullopt;
              }

              // Automatically step through members as we hit this. Note that
              // this means a transition will kick us fully out of this process.
              assert(cursor.choice_thread_index < choice.members.size());
              dbg_out(">>> processing choice member at c_t_i:"
                      << cursor.choice_thread_index);
              auto res = do_member(choice.members[cursor.choice_thread_index]);
              cursor.choice_thread_index++;
              return res;
            }

            dbg_out("next(): hit a ChoiceGroup w/ sel = -1, returning OG");
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
        bm);

    // If we got something out of the member, return it; otherwise loop de
    // loop.
    if (response)
      return *response;

    // If no response, step forward
    dbg_out("----> advancing cursor ...");
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
  dbg_out("\n>! Engine::act(" << choice_index << ")");
  auto &bm = cursor_bm();
  std::optional<Error> err;
  std::visit(
      [&](const auto &mem) {
        using T = std::decay_t<decltype(mem)>;
        if constexpr (std::is_same_v<T, Member>) {

          /// Act on a beat: CONTINUE ///
          err = advance_cursor(mem.line_number);

        } else if constexpr (std::is_same_v<T, ChoiceGroup>) {

          // If choice_selection is already made, this is stepping through.
          if (cursor.choice_selection >= 0) {
            // TODO: Consider turning this into an actual error
            assert(choice_index == 0); // No choices available in a choice block
            return;
          }

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
          cursor.choice_thread_index = 0;
        }
      },
      bm);

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
  build_state(*current);
  cursor.current_block_index = block;
  cursor.current_member_index = 0;

  // Ensure this has members
  auto &b = cursor_block();
  if (b.members.size() == 0) {
    return Error{ERROR_START_EMPTY_BLOCK,
                 "You cannot enter into an empty block!", b.line_number};
  }

  setup_mbm(cursor_mbm());
  return next();
}

// SECTION: FILE LOADING AND PARSING

// STUB: Initialize with module.

// TODO: Serialize

// Build basic fs reader to use by default
std::optional<std::string> default_source_reader(const std::string &path) {
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    return std::nullopt;
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

void Engine::set_source_reader(SourceReader reader) {
  reader_ = std::move(reader);
}

ParseResult Engine::setup(std::string path) {
  try {
    std::optional<std::string> source =
        reader_ ? reader_(path) : default_source_reader(path);
    if (!source) {
      return ParseResult::fail("File not found: " + path);
    }
    pegtl::memory_input in(*source, path);
    CodexParseState pstate(path);

    dbg_out("------- CODEX PARSING ------");
    if (pegtl::parse<codex_grammar, codex_action>(in, pstate)) {
      dbg_out("----------------------------");
      dbg_out("Codex parse successful!");
    } else {
      dbg_out("----------------------------");
      dbg_out("Codex parse failed!");
      return ParseResult::fail("Codex parse failed!");
    }

    dbg_out(">>> Parse results:\n");

    dbg_out("GLOBAL VARS:");
    for (const auto &dec : pstate.codex.global_vars) {
      dbg_out(" - " << dec.var.dbg_desc() << " = "
                    << rval_to_string(dec.initial_value));
    }
    dbg_out("METHOD DEFS:");
    for (const auto &def : pstate.codex.method_defs) {
      dbg_out(" - " << def.dbg_desc());
    }

    // Grab the finished module from the parse state
    codex = std::make_unique<Codex>(std::move(pstate.codex));

    // Initialize state with the new codex (wipes prior state)
    init_state();
    return ParseResult::with(std::move(pstate.errors));
    dbg_out(">>> codex_path() = " << codex->codex_path());
  } catch (const pegtl::parse_error &e) {
    dbg_out("Codex parse error: " << e.what());
    return ParseResult::fail(e.what());
  } catch (const std::exception &e) {
    dbg_out("Codex error: " << e.what());
    return ParseResult::fail(e.what());
  }
}

ParseResult Engine::load(std::string path) {
  try {
    // Resolve project paths against the codex root: "alice.ska" with codex
    // ~/bob/a.codex -> ~/bob/alice.ska. Without a codex, use the path as-is.
    std::string file_path = codex ? codex->resolve_path(path) : path;
    std::optional<std::string> source =
        reader_ ? reader_(file_path) : default_source_reader(file_path);
    if (!source) {
      return ParseResult::fail("File not found: " + file_path);
    }
    pegtl::memory_input in(*source, file_path);
    dbg_out("Loaded file: " << file_path);

    /// PARSING ///

    ParseState pstate(path, codex.get());

    if (pegtl::parse<grammar, action>(in, pstate)) {
      dbg_out("Parse successful!");
      pstate.do_dbg_desc();
    } else {
      dbg_out("Parse failed!");
      return ParseResult::fail("Module parse failed!");
    }

    // Grab the finished module from the parse state
    current = std::make_unique<Module>(std::move(pstate.module));
    return ParseResult::with(pstate.errors);
  } catch (const pegtl::parse_error &e) {
    dbg_out("Parse error: " << e.what());
    return ParseResult::fail(e.what());
  } catch (const std::exception &e) {
    dbg_out("Error: " << e.what());
    return ParseResult::fail(e.what());
  }
}

void Engine::trace(std::string path) {
  pegtl::file_input in(path);
  dbg_out("Loaded file: " << path << "\n");

  pegtl::standard_trace<grammar>(in);
}

std::optional<std::string> Engine::get_project_root() {
  if (codex) {
    return codex->path;
  }
  return std::nullopt;
}
std::optional<std::string> Engine::get_codex_name() {
  if (codex) {
    return codex->filename;
  }
  return std::nullopt;
}

/// EXTERNAL STATE ACCESS ///

/** Sets global state; errors if global doesn't exist or type mismatch. */
std::optional<Error> Engine::set(std::string key, SimpleRValue val) {
  auto it = global_state.find(key);
  if (it == global_state.end()) {
    return Error(ERROR_VAR_UNDEFINED,
                 "Tried to set undefined global var " + key + ".", 0);
  }

  if (srval_get_type(it->second) != srval_get_type(val)) {
    return Error(ERROR_TYPE_MISMATCH,
                 "Tried to set global var " + key + " to " +
                     rval_to_string(val) + ", but the type does not match.",
                 0);
  }

  it->second = val;
  return std::nullopt;
}

/** Returns state; errors if not set. */
std::variant<Error, SimpleRValue> Engine::get(std::string key) {
  auto it = global_state.find(key);
  if (it == global_state.end()) {
    return Error(ERROR_VAR_UNDEFINED,
                 "Tried to get undefined global var " + key + ".", 0);
  }
  return it->second;
}

} // namespace Skald
