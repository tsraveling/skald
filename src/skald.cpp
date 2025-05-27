#include "../include/skald.h"
#include "skald_actions.h"
#include "skald_grammar.h"
#include "tao/pegtl/parse.hpp"
#include <tao/pegtl.hpp>

namespace pegtl = tao::pegtl;

namespace Skald {

/* EXTERNAL API */

void Skald::load(std::string path) {
  pegtl::file_input in(path);
  std::cout << "Loaded file: " << path << "\n";
  pegtl::parse<grammar, action>(in);
}

void Skald::trace() {
  // STUB: Integrate pegtl's trace system to output some debug stuff here
}

} // namespace Skald
