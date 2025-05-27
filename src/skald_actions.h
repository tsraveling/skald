#pragma once

#include "skald_grammar.h"
#include <iostream>

namespace Skald {
template <typename Rule> struct action {};

template <> struct action<block_tag> {
  template <typename ActionInput>
  static void apply(const ActionInput &in, std::string &v) {
    v = in.string();
    std::cout << "block tag: " << v << "\n";
  }
};

} // namespace Skald
