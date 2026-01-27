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

std::pair<Block &, Beat &> Engine::getCurrentBlockAndBeat() {
  Block &block = current->blocks[cursor.current_block_index];
  Beat &beat = block.beats[cursor.current_beat_index];
  return {block, beat};
}

// SECTION: STATE

void Engine::build_state(const Module &module) {
  state.clear();
  for (auto &var : module.declarations) {
    // We don't overwrite variables that are already in state
    if (!state.count(var.var.name))
      state[var.var.name] = var.initial_value;
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

std::vector<Query> queries_for_operations(const std::vector<Operation> &ops) {
  std::vector<Query> ret;
  for (auto &op : ops) {
    auto *call = op_get_call(op);
    if (call)
      ret.push_back(Query{.call = *call,
                          .expects_response = false,
                          .line_number = call->line_number});
  }
  return ret;
}

/** This is called in the Conditional beat phase, to check if the beat should be
 * processed at all */
std::vector<Query> queries_for_beat_conditional(const Beat &beat) {
  std::vector<Query> ret;
  if (beat.condition) {
    auto cond = queries_for_conditional(*beat.condition);
    ret.insert(ret.end(), cond.begin(), cond.end());
  }
  return ret;
}

/** Sets up queries for a beat's operations, choice conditionals,
 *  and choice ops. Will be called only if its conditionals have previously been
 * queried and resolved to true. */
std::vector<Query> queries_for_beat(const Beat &beat) {
  std::vector<Query> ret;
  if (beat.operations.size() > 0) {
    auto op = queries_for_operations(beat.operations);
    ret.insert(ret.end(), op.begin(), op.end());
  }
  if (beat.choices.size() > 0) {
    for (auto &choice : beat.choices) {
      if (choice.condition) {
        auto cond = queries_for_conditional(*choice.condition);
        ret.insert(ret.end(), cond.begin(), cond.end());
      }
      // We only process ops if this choice is selected.
    }
  }

  return ret;
}

std::vector<Query> queries_for_choice_exec(const Choice &choice) {
  std::vector<Query> ret;
  if (choice.operations.size() > 0) {
    auto op = queries_for_operations(choice.operations);
    ret.insert(ret.end(), op.begin(), op.end());
  }
  return ret;
}

// SECTION: RESOLVERS

/** Resolves an rvalue (potentialy including method calls or variables) down to
 *  a simple value based on the current state and query cache. If no key exists,
 *  boolean false will be returned. */
SimpleRValue Engine::resolve_rval_to_simple(const RValue &rval) {
  return std::visit(
      [this](const auto &value) -> SimpleRValue {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::shared_ptr<MethodCall>>) {
          auto key = key_for_call(*value);
          auto it = query_cache.find(key);
          auto val = it != query_cache.end() ? it->second : SimpleRValue{false};
          dbg_out(">>> lookup for " << key << ": val=" << rval_to_string(val));
          return val;
        } else if constexpr (std::is_same_v<T, Variable>) {
          auto it = state.find(value.name);
          return it != state.end() ? it->second : SimpleRValue{false};
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
          dbg_out("   (alraedy called)");
          // This has already been done in the resolution phase; do nothing
        } else if constexpr (std::is_same_v<T, Mutation>) {
          auto val = o.rvalue ? resolve_rval_to_simple(*o.rvalue) : false;
          switch (o.type) {
          case Mutation::Type::EQUATE:
            state[o.lvalue] = val;
            break;
          case Mutation::Type::ADD:
            dbg_out(">>> TODO: Implement adding");
            break;
          case Mutation::Type::SUBTRACT:
            dbg_out(">>> TODO: Implement subtraction");
            break;
          case Mutation::Type::SWITCH:
            if (state.count(o.lvalue)) {
              if (auto *bval = std::get_if<bool>(&val)) {
                *bval = !*bval;
              } else {
                ret = Error(ERROR_TYPE_MISMATCH,
                            "Tried to switch " + o.lvalue +
                                ", which is not a boolean",
                            o.line_number);
              }
            } else {
              state[o.lvalue] = true;
            }
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

// SECTION: 1 - BEAT CONDITIONAL

/** This sets the phase to first and queues the beat's conditional for
 *  processing. */
void Engine::setup_beat() {
  auto [block, beat] = getCurrentBlockAndBeat();
  cursor.resolution_stack = queries_for_beat_conditional(beat);
  dbg_out("     (" << block.tag << ":" << cursor.current_beat_index
                   << ") -> [COND: " << cursor.resolution_stack.size()
                   << " queries queued ]");
  cursor.current_phase = ProcessPhase::Conditional;
}

// SECTION: 2 - BEAT RESOLUTION

/** This sets the phase to second and queues the beat's ops and choices for
 *  processing. */
void Engine::process_beat() {
  auto [block, beat] = getCurrentBlockAndBeat();

  // STUB: Index checking and error throwing system

  // Queue up any queries that need to happen
  cursor.resolution_stack = queries_for_beat(beat);
  cursor.current_phase = ProcessPhase::Resolution;
  dbg_out("      [PROCESS: there are " << cursor.resolution_stack.size()
                                       << " queries queued ]");
}

// SECTION: 3 - PRESENTATION

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

// SECTION: 4 - EXECUTION

void Engine::setup_choice() {
  auto [block, beat] = getCurrentBlockAndBeat();
  auto &choice = beat.choices[cursor.choice_selection];
  cursor.resolution_stack = queries_for_choice_exec(choice);
  dbg_out("     [CHOICE EXEC: there are " << cursor.resolution_stack.size()
                                          << " queries queued ]");
  // STUB: NEXT: tie this back into next()
  cursor.current_phase = ProcessPhase::Execution;
}

std::optional<Error> Engine::advance_cursor(int from_line_number) {
  // consuming the queued transition tag as we do so.
  if (cursor.queued_transition.length() > 0) {
    dbg_out("    >>> transitioning to " << cursor.queued_transition);
    auto new_index = current->get_block_index(cursor.queued_transition);
    if (new_index == -1)
      return Error(ERROR_MODULE_TAG_NOT_FOUND,
                   "Module tag not found: " + cursor.queued_transition,
                   from_line_number);

    cursor.current_block_index = new_index;
    dbg_out("    >>> now on block index " << new_index);

    // This allows the next clause to check for empty block
    cursor.current_beat_index = -1;
    cursor.queued_transition = "";
  }

  Block &block = current->blocks[cursor.current_block_index];
  cursor.current_beat_index++;
  dbg_out("    >>> now on beat index " << cursor.current_beat_index << ", of "
                                       << block.beats.size());
  if (cursor.current_beat_index >= block.beats.size()) {
    cursor.current_block_index++;
    dbg_out(
        "    >>> passed end of block, jumping in at first beat of next block: "
        << cursor.current_block_index);
    cursor.current_beat_index = 0;
    if (cursor.current_block_index >= current->blocks.size()) {
      return Error(ERROR_EOF, "Unexpectedly reached the end of the file",
                   from_line_number);
    }
  }
  dbg_out("CURSOR --> " << cursor.current_block_index << ", "
                        << cursor.current_beat_index);
  setup_beat();
  return std::nullopt;
}

// SECTION: CORE ITERATOR

/** This steps forward to whatever the next thing is that needs to happen, and
 *  as soon as any response is pending, returns it. */
Response Engine::next() {
  // Processor loop
  int debug_blocker = 0;
  while (true) {
    debug_blocker++;
    if (debug_blocker > 50) {
      dbg_out(">>> Engine::next infinite loop exception! breaking.");
      return End{};
    }

    // Handle exits and module transitions
    if (cursor.queued_exit) {
      return *cursor.queued_exit;
    }
    if (cursor.queued_go) {
      return *cursor.queued_go;
    }

    // If there's a pending query, do that.
    if (cursor.resolution_stack.size() > 0) {
      return cursor.resolution_stack.back();
    }

    // If we get here, it means we're ready to do logic

    auto [block, beat] = getCurrentBlockAndBeat();
    switch (cursor.current_phase) {
    case ProcessPhase::Conditional: {
      if (beat.is_else) {
        if (cursor.did_last_condition_pass) {
          // An else beat will always fail if the previos beat passed.
          auto err = advance_cursor();
          if (err)
            return *err;
        } else {
          // An else beat will succeed if the last beat did not!
          process_beat();
          cursor.did_last_condition_pass = true;
          break;
        }
      }
      if (resolve_condition(beat.condition)) {
        process_beat(); // This advances cursor as well
        cursor.did_last_condition_pass = true;
      } else {
        auto err = advance_cursor();
        cursor.did_last_condition_pass = false;
        if (err)
          return *err;
      }
      break;
    }
    case ProcessPhase::Resolution: {
      // Do the operations that are connected to the beat itself
      for (auto &op : beat.operations) {
        auto err = do_operation(op);
        if (err)
          return *err;
      }
      // Then present the text
      cursor.current_phase = ProcessPhase::Presentation;
      break;
    }
    case ProcessPhase::Presentation: {

      // Logic beats just progress
      if (beat.is_logic_block) {
        auto err = advance_cursor();
        if (err)
          return *err;
        break;
      }

      // Assemble the content to return. After this, the cursor will advance via
      // act. Choice effects will also be applied in act(), so next() hangs here
      // until user input occurs.
      auto content = Content{};
      content.text = resolve_text(beat.content);
      for (auto &choice : beat.choices) {
        auto opt = Option{};
        opt.text = resolve_text(choice.content);
        opt.is_available = resolve_condition(choice.condition);
        content.options.push_back(opt);
      }
      return content;
    }
    case ProcessPhase::Execution: {
      // Now that e have queried any method calls, do all the operations
      auto &choice = beat.choices[cursor.choice_selection];
      for (auto &op : choice.operations) {
        auto err = do_operation(op);
        if (err)
          return *err;
      }

      // And proceed
      advance_cursor(choice.line_number);
      break;
    }
    }
  }
}

// SECTION: PLAYER INPUT

Response Engine::act(int choice_index) {
  auto [block, beat] = getCurrentBlockAndBeat();
  if (beat.choices.size() > 0) {

    // Make sure the selection is in bounds
    if (choice_index >= beat.choices.size()) {
      return Error(ERROR_CHOICE_OUT_OF_BOUNDS,
                   "You picked choice " + std::to_string(choice_index) +
                       ", but there are only " +
                       std::to_string(beat.choices.size()) +
                       " choices available!",
                   beat.line_number);
    }

    auto &choice = beat.choices[choice_index];

    // Make sure the selection is valid
    if (!resolve_condition(choice.condition)) {
      return Error(ERROR_CHOICE_UNAVAILABLE,
                   "You picked choice " + std::to_string(choice_index) +
                       ", but it is unavailable.",
                   choice.line_number);
    }

    // Process any queries that are needed
    cursor.choice_selection = choice_index;
    setup_choice(); // This also sets us to execution phase
  } else {

    // If there are no choices, just proceed to next
    auto err = advance_cursor(beat.line_number);
    if (err)
      return *err;
  }
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
  return enter(1, 0);
}

Response Engine::enter(int block, int beat) {
  cursor.reset();
  cursor.current_block_index = 0;
  cursor.current_beat_index = 0;
  setup_beat();
  return next();
}

// SECTION: FILE LOADING AND PARSING

void Engine::load(std::string path) {
  try {
    pegtl::file_input in(path);
    std::cout << "Loaded file: " << path << "\n";

    ParseState pstate(path);

    if (pegtl::parse<grammar, action>(in, pstate)) {
      std::cout << "Parse successful!\n";
    } else {
      std::cout << "Parse failed!\n";
    }

    std::cout << ">>> Parse results:\n\n";

    std::cout << "DECLARATIONS:\n";
    for (const auto &dec : pstate.module.declarations) {
      std::cout << " - " << (dec.is_imported ? "<IMPORT>" : "<NEW>") << " "
                << dec.var.name << " (" << rval_to_string(dec.initial_value)
                << ")\n";
    }

    std::cout << "TESTBEDS:\n";
    for (const auto &testbed : pstate.module.testbeds) {
      std::cout << testbed.dbg_desc() << "\n";
    }

    std::cout << "\nSTRUCTURE:\n";
    // Print details about each block
    for (const auto &block : pstate.module.blocks) {
      std::cout << "\n- Block '" << block.tag << "': " << block.beats.size()
                << " beats" << std::endl;
      for (const auto &beat : block.beats) {
        std::cout << "  - Beat: " << beat.dbg_desc() << "\n";
        for (const auto &choice : beat.choices) {
          std::cout << "    > Choice: " << choice.dbg_desc() << "\n";
          std::cout << dbg_desc_ops(choice.operations);
        }
      }
    }

    // Grab the finished module from the parse state
    current = std::make_unique<Module>(std::move(pstate.module));

  } catch (const pegtl::parse_error &e) {
    std::cout << "Parse error: " << e.what() << "\n";
  } catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << "\n";
  }
}

void Engine::trace(std::string path) {
  pegtl::file_input in(path);
  std::cout << "Loaded file: " << path << "\n";

  pegtl::standard_trace<grammar>(in);
}

} // namespace Skald
