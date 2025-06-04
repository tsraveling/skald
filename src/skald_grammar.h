#pragma once

#include <tao/pegtl.hpp>

using namespace tao::pegtl;

namespace Skald {

struct inline_comment : seq<string<'{', '-', '-'>, until<string<'}'>>> {};

// Line comment using the same comment marker style
struct line_comment : seq<string<'-', '-'>, until<eol>> {};

struct blank_line : seq<star<blank>, eol> {};

struct ws : star<space> {};

/* Block tag */
struct block_tag_name : plus<identifier_other> {};
struct block_tag_line : seq<one<'#'>, block_tag_name, eol> {};

/* Beat */
struct beat_attribution
    : seq<star<blank>, plus<identifier_other>, one<':'>, star<blank>> {};
struct beat_text : plus<seq<not_at<string<'{'>>, not_at<eol>, any>> {};
struct beat_content_part : sor<inline_comment, beat_text> {};
struct beat_content : plus<beat_content_part> {};

// Rules out blank lines
struct beat_line : seq<not_at<seq<star<blank>, eol>>, opt<beat_attribution>,
                       beat_content, eol> {};

/* Choices */
struct choice_text : until<eol> {};
struct choice_line : seq<star<blank>, one<'>'>, star<blank>, choice_text> {};
// STUB: put optional indented "operations" here later
struct choice_clause : seq<choice_line> {};
struct choice_block : plus<choice_clause> {};

// Skip these
struct ignored : sor<line_comment, blank_line> {};

struct block : seq<block_tag_line, star<sor<ignored, beat_line>>> {};

struct grammar : seq<star<ignored>, // Skip initial comments/blanks
                     plus<block>,   // One or more blocks
                     star<ignored>, // Skip trailing comments/blanks
                     opt<eof>       // Optional EOF (more forgiving)
                     > {};

} // namespace Skald
