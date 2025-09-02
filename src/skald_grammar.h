#pragma once

#include <tao/pegtl.hpp>

using namespace tao::pegtl;

namespace Skald {

// Line comment using the same comment marker style
struct line_comment : seq<string<'-', '-'>, until<eol>> {};

struct blank_line : seq<star<blank>, eol> {};

struct ws : star<space> {};

// SECTION: PREFIXES

struct choice_prefix : seq<star<blank>, one<'>'>> {};

// SECTION: TEXT

/** Matches {-- some comment} */
struct inline_comment : seq<string<'{', '-', '-'>, until<string<'}'>>> {};

/** Matches anything up to { or EOL */
struct inline_text_segment : plus<seq<not_at<string<'{'>>, not_at<eol>, any>> {
};

/** Any valid part of a text sequence */
struct text_content_part : sor<inline_comment, inline_text_segment> {};

/** A piece of text conent (an array of parts) */
struct text_content : plus<text_content_part> {};

// SECTION: BEATS

/** The `some_tag: ...` part of a beat */
struct beat_attribution
    : seq<star<blank>, plus<identifier_other>, one<':'>, star<blank>> {};

/** A line with an optional attribution that is not indented or blank */
struct beat_line : seq<not_at<seq<star<blank>, eol>>, not_at<choice_prefix>,
                       opt<beat_attribution>, text_content, eol> {};

// SECTION: CHOICES

// STUB: put optional one-line choice ops here
/** The initial line e.g. `> Some choice` */
struct choice_line : seq<choice_prefix, star<blank>, text_content> {};
// STUB: put optional indented "operations" here later
struct choice_clause : seq<choice_line, eol> {};
struct choice_block : plus<choice_clause> {};

// SECTION: EXCLUDED

struct ignored : sor<line_comment, blank_line> {};

// SECTION: BLOCKS

struct block_tag_name : plus<identifier_other> {};
struct block_tag_line : seq<one<'#'>, block_tag_name, eol> {};
struct block
    : seq<block_tag_line, star<sor<ignored, beat_line, choice_block>>> {};

// SECTION: FULL GRAMMAR

struct grammar : seq<star<ignored>, // Skip initial comments/blanks
                     plus<block>,   // One or more blocks
                     star<ignored>, // Skip trailing comments/blanks
                     opt<eof>       // Optional EOF (more forgiving)
                     > {};

} // namespace Skald
