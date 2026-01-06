#include "../include/skald.h"
#include "debug.h"
#include "parse_state.h"
#include "skald_actions.h"
#include "skald_grammar.h"
#include "tao/pegtl/parse.hpp"
#include <iostream>
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/trace.hpp>
#include <vector>

namespace pegtl = tao::pegtl;

namespace Skald {

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

      // STUB: is .a a method? if so add to query
      // STUB: is .b a method? if so add to query
      // result.push_back(Query{ /* fill */ });
    } else if (auto *nested =
                   std::get_if<std::shared_ptr<Conditional>>(&item)) {
      auto queries = queries_for_conditional(**nested);
      result.insert(result.end(), queries.begin(), queries.end());
    }
  }

  return result;
}

std::vector<Query> queries_for_operations(const std::vector<Operation> &ops) {}

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

Option Engine::resolve_option(const Choice &choice) {
  Option ret;
  ret.text = resolve_text(choice.content);
  if (choice.condition.has_value()) {
    ret.is_available = resolve_condition(choice.condition.value());
  } else {
    ret.is_available = true;
  }
  return ret;
}

void Engine::process_cursor() {
  Block &block = current->blocks[cursor.current_block_index];
  Beat &beat = block.beats[cursor.current_beat_index];

  // STUB: 1. beat conditional
  // STUB: 2. beat operations

  // STUB: Method to detect if it's choice time
  // STUB: Resolve queries on options

  // If at end of block, include choices
  // if (cursor.current_beat_index == block.beats.size() - 1) {
  //   for (auto &choice : block.choices) {
  //     content.options.push_back(resolve_option(choice));
  //   }
  // }
}

Response Engine::next() {
  if (cursor.resolution_stack.size() > 0) {
    // STUB: Do queries here
  }
  Block &block = current->blocks[cursor.current_block_index];
  Beat &beat = block.beats[cursor.current_beat_index];

  // STUB: Flush resolver cache here

  Content content;
  content.text = resolve_text(beat.content);

  // STUB: If queries are resolved, check if this beat is valid and if it isn't,
  // skip forward

  // If at end of block, include choices
  // STUB: Handle edge case where final beat is optional and you don't get it.
  // still show choices!
  if (cursor.current_beat_index == block.beats.size() - 1) {
    for (auto &choice : block.choices) {
      content.options.push_back(resolve_option(choice));
    }
  }
  return content;
}

// SECTION: GAMEPLAY

// STUB: Implementation approach:
// 1. Look ahead at the whole next beat.
// 2. Stack anything that needs to be resolved (methods for now)
// 3. Run the processing stack until it is empty
// 4. Store returned values in a temporary storage keyed by call + args (e.g.
// `pop_alert(14, juicy_var)` will be keyed as `pop_alert|14|juicy_var`
// 5. Text can then be assembled thusly

Response Engine::start_at(std::string tag) {
  auto start_index = current->get_block_index(tag);
  if (start_index < 0) {
    throw std::runtime_error("Block not found: " + tag);
  }
  cursor.current_block_index = start_index;
  cursor.current_beat_index = 0;
  cursor.resolution_stack.clear();

  process_cursor();

  return next();
}

Response Engine::start() {
  dbg_out("engine start");
  if (current->blocks.size() < 1) {
    throw std::runtime_error("There are no blocks in the current module");
  }
  cursor.current_block_index = 0;
  cursor.current_beat_index = 0;
  cursor.resolution_stack.clear();

  process_cursor();

  return next();
}

Response Engine::act(int choice_index) { return End{}; }
Response Engine::answer(QueryAnswer answer) { return End{}; }

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
      }
      for (const auto &choice : block.choices) {
        std::cout << "    - Choice: " << choice.dbg_desc() << "\n";
        std::cout << dbg_desc_ops(choice.operations);
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
