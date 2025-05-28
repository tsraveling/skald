#pragma once

#include <tao/pegtl.hpp>

using namespace tao::pegtl;

namespace Skald {

struct comment : seq<string<'-', '-'>, until<eol>> {};

struct blank_line : seq<star<blank>, eol> {};

struct ws : star<space> {};

// Block tag
struct block_tag_name : plus<identifier_other> {};
struct block_tag_line : seq<one<'#'>, block_tag_name, eol> {};

// Beat
struct beat_content : plus<not_one<'\n'>> {};
struct beat_line : seq<beat_content, eol> {};

// Skip these
struct ignored : sor<comment, blank_line> {};

struct block : seq<block_tag_line, star<sor<ignored, beat_line>>> {};

struct grammar : seq<star<ignored>, // Skip initial comments/blanks
                     plus<block>,   // One or more blocks
                     star<ignored>, // Skip trailing comments/blanks
                     opt<eof>       // Optional EOF (more forgiving)
                     > {};

} // namespace Skald
