#pragma once

#include <tao/pegtl.hpp>

using namespace tao::pegtl;

namespace Skald {

// Line comment using the same comment marker style
struct line_comment : seq<string<'-', '-'>, until<eol>> {};
struct end_line_comment : seq<string<'-', '-'>, star<not_one<'\r', '\n'>>> {};
struct ws : star<blank> {};
struct blank_line : seq<ws, eol> {};
struct ignored : sor<line_comment, blank_line> {};

/** This can be used to cap any single functional line that allows an end
 * comment */
struct functional_eol : seq<ws, opt<end_line_comment>, eol> {};

// SECTION: ABSTRACT STRUCTURE

// Parentheses
template <typename ParenContent>
struct paren : seq<one<'('>, ParenContent, one<')'>> {};

// Indentation
using two_or_more_spaces = seq<space, space, star<space>>;
using one_or_more_tabs = plus<one<'\t'>>;
using indent = sor<two_or_more_spaces, one_or_more_tabs>;

// SECTION: PREFIXES

struct choice_prefix : seq<ws, one<'>'>> {};

// SECTION: BASIC VALUE TYPES

struct escaped_char : seq<one<'\\'>, one<'"', '\\', 'n'>> {};
struct string_content : star<not_one<'"'>> {};
struct val_string : seq<one<'"'>, string_content, one<'"'>> {};
struct val_bool_true : keyword<'t', 'r', 'u', 'e'> {};
struct val_bool_false : keyword<'f', 'a', 'l', 's', 'e'> {};
struct val_bool : sor<val_bool_true, val_bool_false> {};
struct identifier : seq<identifier_first, star<identifier_other>> {};
struct move_marker : seq<ws, string<'-', '>'>> {};
struct sign_indicator : opt<one<'+', '-'>> {};
struct signed_float
    : seq<opt<sign_indicator>, plus<digit>, one<'.'>, plus<digit>> {};
struct signed_int : seq<sign_indicator, plus<digit>> {};
struct val_int : signed_int {};
struct val_float : signed_float {};

// SECTION: LOGIC FUNDAMENTALS

struct variable_name : identifier {};

/** A variable name used as an rvalue */
struct arg_list;
struct r_method : seq<one<':'>, identifier, paren<opt<arg_list>>> {};
struct r_variable : variable_name {};
struct rvalue
    : sor<val_bool, val_string, val_float, val_int, r_variable, r_method> {};
struct rvalue_simple : sor<val_bool, val_string, val_float, val_int> {};
struct arg_separator : seq<ws, one<','>, ws> {};
struct argument : rvalue {};
struct arg_list : list<argument, arg_separator> {};

// These are used for inline computation
struct operator_plus_equals : string<'+', '='> {};
struct operator_minus_equals : string<'-', '='> {};
struct operator_not_equals : string<'!', '='> {};
struct operator_equals : one<'='> {};
struct operator_equals_switch : string<'=', '!'> {};
struct operator_more : one<'>'> {};
struct operator_less : one<'<'> {};
struct operator_more_equal : string<'>', '='> {};
struct operator_less_equal : string<'<', '='> {};
struct mut_operator : sor<operator_plus_equals, operator_minus_equals,
                          operator_equals_switch, operator_equals> {};

// SECTION: TESTBEDS

struct rvalue_testbed : sor<val_bool, val_string, val_float, val_int> {};
struct testbed_open : seq<keyword<'@', 't', 'e', 's', 't', 'b', 'e', 'd'>,
                          plus<blank>, identifier, functional_eol> {};
struct keyword_end : keyword<'@', 'e', 'n', 'd'> {};
struct testbed_closed : seq<keyword_end, functional_eol> {};
struct testbed_declaration
    : seq<ws, identifier, ws, one<'='>, ws, rvalue_testbed, functional_eol> {};
struct testbed : seq<testbed_open, star<sor<ignored, testbed_declaration>>,
                     testbed_closed> {};

// SECTION: CONDITIONALS

/// CHECKABLE SYNTAX ///

struct checkable_not_truthy : seq<one<'!'>, rvalue> {};
struct checkable_2f_operator
    : sor<operator_equals, operator_not_equals, operator_more_equal,
          operator_less_equal, operator_more, operator_less> {};
struct checkable_right_tail : seq<ws, checkable_2f_operator, ws, rvalue> {};
struct checkable_base
    : sor<checkable_not_truthy, seq<rvalue, opt<checkable_right_tail>>> {};

/// SPECIFIC CONSTRUCTIONS ///
struct subclause_opener : one<'('> {};
struct subclause_closer : one<')'> {};
struct checkable_subclause;
struct checkable_atom : sor<checkable_base, checkable_subclause> {};

struct checkable_and : keyword<'a', 'n', 'd'> {};
struct checkable_and_tail
    : plus<seq<plus<blank>, checkable_and, plus<blank>, checkable_atom>> {};
struct checkable_or : keyword<'o', 'r'> {};
struct checkable_or_tail
    : plus<seq<plus<blank>, checkable_or, plus<blank>, checkable_atom>> {};

struct checkable_clause
    : seq<checkable_atom, opt<sor<checkable_or_tail, checkable_and_tail>>> {};
struct checkable_subclause
    : seq<subclause_opener, ws, checkable_clause, ws, subclause_closer> {};

/// PUTTING IT TOGETHER ///
struct conditional_opener : seq<one<'('>, ws, one<'?'>> {};
struct conditional_closer : seq<one<')'>> {};
struct conditional
    : seq<conditional_opener, ws, checkable_clause, ws, conditional_closer> {};

// SECTION: INJECTABLES

struct injectable_rvalue : rvalue {};
struct ternary_tail : seq<ws, one<'?'>, ws, rvalue, ws, one<':'>, ws, rvalue> {
};
struct switch_default : one<'_'> {};
struct switch_option
    : seq<sor<switch_default, rvalue>, ws, one<':'>, ws, rvalue> {};
// STUB: NEXT: This switch tail isn't working (regular ternary is)
struct switch_tail : seq<ws, one<'?'>, ws, one<'['>, ws,
                         list<switch_option, seq<ws, one<','>, ws>, space>, ws,
                         must<one<']'>>> {};
struct injectable
    : seq<injectable_rvalue, opt<sor<ternary_tail, switch_tail>>, ws> {};
struct text_injection : seq<one<'{'>, ws, injectable, ws, one<'}'>> {};

// SECTION: TEXT

/** Matches {-- some comment} */
struct inline_comment : seq<string<'{', '-', '-'>, until<string<'}'>>> {};

/** Matches anything up to { or EOL */
struct inline_text_segment
    : plus<seq<not_at<string<'{'>>, not_at<eol>, not_at<move_marker>, any>> {};

/** Any valid part of a text sequence */
struct text_content_part
    : sor<inline_comment, text_injection, inline_text_segment> {};

/** A piece of text conent (an array of parts) */
struct text_content : plus<text_content_part> {};

// SECTION: OPERATIONS

struct op_mutate_start : seq<ws, one<'~'>, ws, identifier, ws> {};
struct op_mutate_equate
    : seq<op_mutate_start, ws, operator_equals, ws, rvalue> {};
struct op_mutate_switch : seq<op_mutate_start, ws, operator_equals_switch> {};
struct math_rvalue : sor<r_variable, val_int, val_float, r_method> {};
struct op_mutate_add
    : seq<op_mutate_start, ws, operator_plus_equals, ws, math_rvalue> {};
struct op_mutate_subtract
    : seq<op_mutate_start, ws, operator_minus_equals, ws, math_rvalue> {};

struct op_mutation : sor<op_mutate_equate, op_mutate_switch, op_mutate_add,
                         op_mutate_subtract> {};

/** You can use basically any string for your module path; we'll check validity
 * later in the LSP */
struct module_path : plus<seq<not_at<move_marker>, not_one<'\r', '\n'>>> {};
struct keyword_go : keyword<'G', 'O'> {};
struct keyword_exit : keyword<'E', 'X', 'I', 'T'> {};
struct op_exit : seq<keyword_exit, opt<seq<plus<blank>, rvalue>>> {};
struct op_go_start_tag : seq<plus<blank>, move_marker, ws, identifier, ws> {};
struct op_go : seq<keyword_go, plus<space>, module_path, opt<op_go_start_tag>> {
};
struct op_move : seq<move_marker, ws, identifier, ws> {};
struct op_method : seq<one<':'>, identifier, paren<opt<arg_list>>> {};
struct operation : sor<op_move, op_method, op_mutation, op_go, op_exit> {};
struct op_line : seq<indent, operation, ws, opt<end_line_comment>, eol> {};

// SECTION: VARIABLE DECLARATION

struct declaration_initial : one<'~'> {};
struct declaration_import : one<'<'> {};
struct declaration_line
    : seq<ws, sor<declaration_initial, declaration_import>, ws, identifier, ws,
          one<'='>, ws, rvalue_simple, ws, opt<end_line_comment>, eol> {};

// SECTION: CHOICES

struct inline_choice_move : op_move {};

/** The initial line e.g. `> Some choice` */
struct choice_line : seq<choice_prefix, ws, opt<conditional>, ws, text_content,
                         opt<inline_choice_move>, eol> {};

/** The choice line with optional indented operation lines */
struct choice_clause : seq<choice_line, star<op_line>> {};
struct choice_block : plus<choice_clause> {};

// SECTION: BEATS
//
struct block_tag_name : identifier {};
struct block_tag_line : seq<one<'#'>, block_tag_name, ws, opt<end_line_comment>, eol> {};

struct logic_beat_else : paren<keyword<'e', 'l', 's', 'e'>> {};
struct logic_beat_conditional : sor<conditional, logic_beat_else> {};
struct logic_beat_clause
    : seq<ws, one<'*'>, ws, opt<logic_beat_conditional>, ws,
          opt<end_line_comment>, ws, eol, plus<op_line>> {};
struct logic_beat_single
    : seq<ws, one<'*'>, ws, opt<logic_beat_conditional>, ws, operation, ws,
          opt<end_line_comment>, ws, eol> {};

/** The `some_tag: ...` part of a beat */
struct beat_attribution : seq<ws, identifier, one<':'>, ws> {};

/** A line with an optional attribution that is not indented or blank */
struct beat_line
    : seq<not_at<seq<ws, eol>>, not_at<choice_prefix>, not_at<block_tag_line>,
          opt<conditional>, ws, opt<beat_attribution>, text_content, eol> {};

/** A text beat, optionally followed by some operations. */
struct beat_clause
    : seq<beat_line, star<op_line>, opt<seq<star<blank_line>, choice_block>>> {
};

// SECTION: BLOCKS

// Error recovery: skip any line that doesn't match known block content.
// This prevents a single bad line from killing the parse for the rest of the file.
struct skip_line : seq<not_at<block_tag_line>, not_at<seq<ws, eol>>, until<eol>> {};

struct block : seq<block_tag_line, star<sor<ignored, logic_beat_single,
                                            logic_beat_clause, beat_clause, skip_line>>> {
};

// SECTION: FULL GRAMMAR

struct grammar
    : seq<star<ignored>,                        // Skip initial comments/blanks
          star<sor<ignored, declaration_line>>, // Variable declarations
          star<sor<testbed, ignored>>,          // Testbeds
          star<ignored>,                        // Whitespace etc
          plus<block>,                          // One or more blocks
          star<ignored>,                        // Skip trailing comments/blanks
          opt<eof>                              // Optional EOF (more forgiving)
          > {};

} // namespace Skald
