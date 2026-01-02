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

// SECTION: GAMEPLAY

// STUB: Implementation approach:
// 1. Look ahead at the whole next beat.
// 2. Stack anything that needs to be resolved (methods for now)
// 3. Run the processing stack until it is empty
// 4. Store returned values in a temporary storage keyed by call + args (e.g.
// `pop_alert(14, juicy_var)` will be keyed as `pop_alert|14|juicy_var`
// 5. Text can then be assembled thusly

Response Engine::start_at(std::string tag) {
  auto *start_block = current->get_block(tag);
  if (start_block == nullptr) {
    throw std::runtime_error("Block not found: " + tag);
  }

  // FIXME: Replace this with beat text
  return Content{.text = {Chunk{"One"}, Chunk{"Two"}, Chunk{"Three"}}};
}

Response Engine::start() {
  dbg_out("engine start");
  if (current->blocks.size() < 1) {
    throw std::runtime_error("There are no blocks in the current module");
  }
  dbg_out("start blocks size: " << current->blocks.size());
  auto *start_block = &current->blocks.front();

  // FIXME: Replace this with beat text
  return Content{.text = {Chunk{"One"}, Chunk{"Two"}, Chunk{"Three"}}};
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
