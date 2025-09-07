#pragma once

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/json.hpp>

using namespace tao::pegtl;

namespace Skald {

// Line comment using the same comment marker style
struct line_comment : seq<string<'-', '-'>, until<eol>> {};
struct blank_line : seq<star<blank>, eol> {};
struct ws : star<space> {};

// SECTION: ABSTRACT STRUCTURE
//
template <typename ParenContent>
struct paren : seq<one<'('>, ParenContent, one<')'>> {};

// SECTION: PREFIXES

struct choice_prefix : seq<star<blank>, one<'>'>> {};

// SECTION: BASIC VALUE TYPES

struct val_string : json::string {};
struct val_bool_true : string<'t', 'r', 'u', 'e'> {};
struct val_bool_false : string<'f', 'a', 'l', 's', 'e'> {};
struct val_bool : sor<val_bool_true, val_bool_false> {};
struct identifier : plus<identifier_other> {};
struct move_marker : seq<star<blank>, string<'-', '>'>> {};
struct sign_indicator : one<'+', '-'> {};
struct signed_float
    : seq<opt<sign_indicator>, plus<digit>, one<'.'>, plus<digit>> {};
struct signed_int : seq<sign_indicator, plus<digit>> {};
struct val_number : sor<signed_float, signed_int> {};

// SECTION: TEXT

/** Matches {-- some comment} */
struct inline_comment : seq<string<'{', '-', '-'>, until<string<'}'>>> {};

/** Matches anything up to { or EOL */
struct inline_text_segment
    : plus<seq<not_at<string<'{'>>, not_at<eol>, not_at<move_marker>, any>> {};

/** Any valid part of a text sequence */
struct text_content_part : sor<inline_comment, inline_text_segment> {};

/** A piece of text conent (an array of parts) */
struct text_content : plus<text_content_part> {};

// SECTION: LOGIC FUNDAMENTALS

struct variable_name : identifier {};
struct rvalue : sor<val_bool, val_string, val_number, variable_name> {};
struct arg_separator : seq<star<blank>, one<','>, star<blank>> {};
struct argument : rvalue {};
struct arg_list : list<argument, arg_separator> {};

// SECTION: OPERATIONS

// struct argument: identifier
struct op_move : seq<move_marker, star<blank>, identifier, star<blank>> {};
struct op_method : seq<one<':'>, identifier, paren<opt<arg_list>>> {};

// SECTION: BEATS

/** The `some_tag: ...` part of a beat */
struct beat_attribution : seq<star<blank>, identifier, one<':'>, star<blank>> {
};

/** A line with an optional attribution that is not indented or blank */
struct beat_line : seq<not_at<seq<star<blank>, eol>>, not_at<choice_prefix>,
                       opt<beat_attribution>, text_content, eol> {};

// SECTION: CHOICES

// STUB: put optional one-line choice ops here
/** The initial line e.g. `> Some choice` */
struct inline_choice_move : op_move {};
struct choice_line : seq<choice_prefix, star<blank>, text_content,
                         opt<inline_choice_move>, eol> {};
// STUB: put optional indented "operations" here later
struct choice_clause : seq<choice_line> {};
struct choice_block : plus<choice_clause> {};

// SECTION: EXCLUDED

struct ignored : sor<line_comment, blank_line> {};

// SECTION: BLOCKS

struct block_tag_name : identifier {};
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
