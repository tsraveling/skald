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

/* DEBUG OUTPUT STUFF TO DELETE LATER */

struct OpDebugProcessor {
  void operator()(const Move &move) {
    dbg_out("        * MOVE TO: " << move.target_tag);
  }
  void operator()(const MethodCall &method_call) {
    dbg_out("        * CALL: " << method_call.dbg_desc());
  }
  void operator()(const Mutation &mutation) {
    dbg_out("        * MUTATE: " << mutation.dbg_desc());
  }
};

void dbg_desc_ops(const std::vector<Operation> &ops) {
  for (auto &op : ops) {
    std::visit(OpDebugProcessor{}, op);
  }
}

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

    std::cout << ">>> Parse results:";
    // Print details about each block
    for (const auto &[tag, block] : state.module.blocks) {
      std::cout << "   - Block '" << tag << "': " << block.beats.size()
                << " beats" << std::endl;
      for (const auto &beat : block.beats) {
        std::cout << "     - Beat: " << beat.attribution << ": "
                  << beat.content.dbg_desc() << "\n";
      }
      for (const auto &choice : block.choices) {
        std::cout << "      - Choice: " << choice.content.dbg_desc() << "\n";
        dbg_desc_ops(choice.operations);
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
