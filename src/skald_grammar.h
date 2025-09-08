#pragma once

#include <tao/pegtl.hpp>

using namespace tao::pegtl;

namespace Skald {

// Line comment using the same comment marker style
struct line_comment : seq<string<'-', '-'>, until<eol>> {};
struct blank_line : seq<star<blank>, eol> {};
struct ws : star<space> {};

// SECTION: ABSTRACT STRUCTURE

// Parentheses
template <typename ParenContent>
struct paren : seq<one<'('>, ParenContent, one<')'>> {};

// Indentation
using two_or_more_spaces = seq<space, space, star<space>>;
using one_or_more_tabs = plus<one<'\t'>>;
using indent = sor<two_or_more_spaces, one_or_more_tabs>;

// SECTION: PREFIXES

struct choice_prefix : seq<star<blank>, one<'>'>> {};

// SECTION: BASIC VALUE TYPES

struct escaped_char : seq<one<'\\'>, one<'"', '\\', 'n'>> {};
struct string_content : star<not_one<'"'>> {};
struct val_string : seq<one<'"'>, string_content, one<'"'>> {};
struct val_bool_true : string<'t', 'r', 'u', 'e'> {};
struct val_bool_false : string<'f', 'a', 'l', 's', 'e'> {};
struct val_bool : sor<val_bool_true, val_bool_false> {};
struct identifier : plus<identifier_other> {};
struct move_marker : seq<star<blank>, string<'-', '>'>> {};
struct sign_indicator : opt<one<'+', '-'>> {};
struct signed_float
    : seq<opt<sign_indicator>, plus<digit>, one<'.'>, plus<digit>> {};
struct signed_int : seq<sign_indicator, plus<digit>> {};
struct val_int : signed_int {};
struct val_float : signed_float {};

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

/** A variable name used as an rvalue */
struct r_variable : variable_name {};
struct rvalue : sor<val_bool, val_string, val_float, val_int, r_variable> {};
struct arg_separator : seq<star<blank>, one<','>, star<blank>> {};
struct argument : rvalue {};
struct arg_list : list<argument, arg_separator> {};

// SECTION: OPERATIONS

struct op_move : seq<move_marker, star<blank>, identifier, star<blank>> {};
struct op_method : seq<one<':'>, identifier, paren<opt<arg_list>>> {};
struct operation : sor<op_move, op_method> {};
struct op_line : seq<indent, operation, eol> {};

// SECTION: BEATS

/** The `some_tag: ...` part of a beat */
struct beat_attribution : seq<star<blank>, identifier, one<':'>, star<blank>> {
};

/** A line with an optional attribution that is not indented or blank */
struct beat_line : seq<not_at<seq<star<blank>, eol>>, not_at<choice_prefix>,
                       opt<beat_attribution>, text_content, eol> {};

// SECTION: CHOICES

struct inline_choice_move : op_move {};

/** The initial line e.g. `> Some choice` */
struct choice_line : seq<choice_prefix, star<blank>, text_content,
                         opt<inline_choice_move>, eol> {};

/** The choice line with optional indented operation lines */
struct choice_clause : seq<choice_line, star<op_line>> {};
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
