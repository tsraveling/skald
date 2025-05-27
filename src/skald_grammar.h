#pragma once

#include <tao/pegtl.hpp>

using namespace tao::pegtl;

namespace Skald {
namespace grammar {

struct comment : seq<string<'-', '-'>, until<eolf>> {};

struct blank_line : seq<star<blank>, eolf> {};

struct ws : star<space> {};

// Block tag
struct tag_name : plus<identifier_other> {};
struct block_tag : seq<one<'#'>, tag_name, eolf> {};

// Beat
struct beat_content : plus<not_one<'\n'>> {};
struct beat : seq<beat_content, eolf> {};

// Skip these
struct ignored : sor<comment, blank_line> {};

// Full grammar
struct grammar
    : must<star<ignored>,
           plus<seq<block_tag, star<ignored>, plus<seq<beat, star<ignored>>>>>,
           eof> {};

} // namespace grammar
} // namespace Skald
