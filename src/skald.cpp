#include "../include/skald.h"
#include "debug.h"
#include "parse_state.h"
#include "skald_actions.h"
#include "skald_grammar.h"
#include "tao/pegtl/parse.hpp"
#include <cstdio>
#include <iostream>
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
    state[var.var.name] = var.initial_value;
  }
}

std::vector<Query> queries_for_conditional(const Conditional &cond) {

  std::vector<Query> result;

  for (const auto &item : cond.items) {
    if (auto *atom = std::get_if<ConditionalAtom>(&item)) {
      const MethodCall *a = rval_get_call(atom->a);
      const MethodCall *b = atom->b ? rval_get_call(*atom->b) : nullptr;
      if (a)
        result.push_back(Query{.call = *a});
      if (b)
        result.push_back(Query{.call = *b});

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
      ret.push_back(Query{.call = *call});
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

bool Engine::resolve_condition(const std::optional<Conditional> &cond) {
  if (cond)
    return resolve_condition(*cond);
  return true;
}
bool Engine::resolve_condition(const Conditional &cond) {
  // STUB: Actually process conditional here
  return true;
}

std::string Engine::resolve_simple(const SimpleInsertion &ins) {
  // STUB: Implement resolution queue here
  return rval_to_string(ins.rvalue);
}
std::string Engine::resolve_tern(const TernaryInsertion &tern) {
  // STUB: Implement ternary resolution here
  return tern.dbg_desc();
}
void Engine::do_operation(Operation &op) {
  // STUB: Implement op resolution here
  dbg_out("    x-x " << dbg_dsc_op(op));
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
    auto new_index = current->get_block_index(cursor.queued_transition);
    if (new_index == -1)
      return Error(ERROR_EOF, "Unexpectedly reached the end of the file",
                   from_line_number);

    cursor.current_block_index = new_index;

    // This allows the next clause to check for empty block
    cursor.current_beat_index = -1;
    cursor.queued_transition = "";
  }

  Block &block = current->blocks[cursor.current_block_index];
  cursor.current_beat_index++;
  if (cursor.current_beat_index >= block.beats.size()) {
    cursor.current_block_index++;
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
    // If there's a pending query, do that.
    if (cursor.resolution_stack.size() > 0) {
      return cursor.resolution_stack.back();
    }

    // If we get here, it means we're ready to do logic

    auto [block, beat] = getCurrentBlockAndBeat();
    switch (cursor.current_phase) {
    case ProcessPhase::Conditional: {
      if (resolve_condition(beat.condition)) {
        process_beat(); // This advances cursor as well
      } else {
        auto err = advance_cursor();
        if (err)
          return *err;
      }
      break;
    }
    case ProcessPhase::Resolution: {
      // Do the operations that are connected to the beat itself
      for (auto &op : beat.operations) {
        do_operation(op);
      }
      // Then present the text
      cursor.current_phase = ProcessPhase::Presentation;
      break;
    }
    case ProcessPhase::Presentation: {
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
        do_operation(op);
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

Response Engine::answer(QueryAnswer answer) {
  // STUB: Error handling for empty queue
  auto &answering = cursor.resolution_stack.back();
  auto key = answering.get_key();
  if (answer.val) {
    query_cache.insert_or_assign(key, *answer.val);
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
  return enter(1, 0);
}

/** This zeroes the state and drops us in at this beat index, e.g. from an
 *  external entry point */
Response Engine::enter(int block, int beat) {
  cursor.current_block_index = 0;
  cursor.current_beat_index = 0;
  cursor.resolution_stack.clear();
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

    // Builds initial gamestate
    build_state(pstate.module);

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
