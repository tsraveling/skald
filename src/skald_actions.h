#pragma once

#include "skald_grammar.h"
#include <iostream>

namespace Skald {

template <typename Rule> struct action {};

// This should fire when the # character is matched
template <> struct action<one<'#'>> {
  template <typename ActionInput>
  static void apply(const ActionInput &in, std::string &v) {
    std::cout << "FOUND HASH: " << in.string() << "\n";
  }
};

// This should fire for the tag name part
template <> struct action<tag_name> {
  template <typename ActionInput>
  static void apply(const ActionInput &in, std::string &v) {
    std::cout << "FOUND TAG NAME: " << in.string() << "\n";
  }
};

} // namespace Skald
