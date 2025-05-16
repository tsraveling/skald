#include "../include/skald.h"
#include <tao/pegtl.hpp>

namespace pegtl = tao::pegtl;

namespace Skald {

/* EXTERNAL API */

void Skald::load(std::string path) {
  pegtl::file_input in(path);
  // STUB: Build out parser
}

void Skald::trace() {
  // STUB: Integrate pegtl's trace system to output some debug stuff here
}

} // namespace Skald
