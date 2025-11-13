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

/* EXTERNAL API */

void Skald::load(std::string path) {
  try {
    pegtl::file_input in(path);
    std::cout << "Loaded file: " << path << "\n";

    ParseState state(path);

    if (pegtl::parse<grammar, action>(in, state)) {
      std::cout << "Parse successful!\n";
    } else {
      std::cout << "Parse failed!\n";
    }

    std::cout << ">>> Parse results:\n\n";

    std::cout << "DECLARATIONS:\n";
    for (const auto &dec : state.module.declarations) {
      std::cout << " - " << (dec.is_imported ? "<IMPORT>" : "<NEW>") << " "
                << dec.var.name << " (" << rval_to_string(dec.initial_value)
                << ")\n";
    }

    std::cout << "TESTBEDS:\n";
    for (const auto &testbed : state.module.testbeds) {
      std::cout << testbed.dbg_desc() << "\n";
    }

    std::cout << "\nSTRUCTURE:\n";
    // Print details about each block
    for (const auto &[tag, block] : state.module.blocks) {
      std::cout << "\n- Block '" << tag << "': " << block.beats.size()
                << " beats" << std::endl;
      for (const auto &beat : block.beats) {
        std::cout << "  - Beat: " << beat.dbg_desc() << "\n";
      }
      for (const auto &choice : block.choices) {
        std::cout << "    - Choice: " << choice.dbg_desc() << "\n";
        std::cout << dbg_desc_ops(choice.operations);
      }
    }

  } catch (const pegtl::parse_error &e) {
    std::cout << "Parse error: " << e.what() << "\n";
  } catch (const std::exception &e) {
    std::cout << "Error: " << e.what() << "\n";
  }
}

void Skald::trace(std::string path) {
  pegtl::file_input in(path);
  std::cout << "Loaded file: " << path << "\n";

  pegtl::standard_trace<grammar>(in);
}

} // namespace Skald
