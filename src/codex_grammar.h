#pragma once

#include "shared_grammar.h"
#include <tao/pegtl.hpp>

using namespace tao::pegtl;

namespace Skald {

// SECTION: FULL GRAMMAR

struct codex_grammar : seq<star<ignored>, // Skip initial comments/blanks
                           opt<eof>> {};

} // namespace Skald
