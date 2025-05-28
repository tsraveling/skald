#include "../include/skald.h"
#include "skald_actions.h"
#include "skald_grammar.h"
#include "tao/pegtl/parse.hpp"
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/trace.hpp>

namespace pegtl = tao::pegtl;

namespace Skald {

/* EXTERNAL API */

void Skald::load(std::string path) {
  try {
    pegtl::file_input in(path);
    std::cout << "Loaded file: " << path << "\n";

    std::string state;
    if (pegtl::parse<grammar, action>(in, state)) {
      std::cout << "Parse successful!\n";
    } else {
      std::cout << "Parse failed!\n";
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
