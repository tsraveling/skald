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

// SECTION: GAMEPLAY

Response Engine::start_at(std::string tag) {
  auto *start_block = current->get_block(tag);
  if (start_block == nullptr) {
    throw std::runtime_error("Block not found: " + tag);
  }

  // FIXME: Replace this with beat text
  return Response{.text = {Chunk{"One"}, Chunk{"Two"}, Chunk{"Three"}}};
}

Response Engine::start() {
  dbg_out("engine start");
  if (current->blocks.size() < 1) {
    throw std::runtime_error("There are no blocks in the current module");
  }
  dbg_out("start blocks size: " << current->blocks.size());
  auto *start_block = &current->blocks.front();

  // FIXME: Replace this with beat text
  return Response{.text = {Chunk{"One"}, Chunk{"Two"}, Chunk{"Three"}}};
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
