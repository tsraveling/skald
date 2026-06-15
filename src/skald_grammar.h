#pragma once

#include "shared_grammar.h"
#include "tao/pegtl/rules.hpp"
#include <tao/pegtl.hpp>

using namespace tao::pegtl;

namespace Skald {

// SECTION: PREFIXES

struct choice_prefix : seq<ws, one<'>'>> {};

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

/** Defines a module path, relative to codex. Ends at EOL, move marker, or
 * comment. */
struct module_path
    : plus<
          seq<not_at<move_marker>, not_at<line_comment>, not_one<'\r', '\n'>>> {
};

using block1_prefix = string<'#'>;
using block2_prefix = string<'#', '#'>;
using block3_prefix = string<'#', '#', '#'>;
using block_prefix = sor<block3_prefix, block2_prefix, block1_prefix>;

// SECTION: KEYWORDS

struct keyword_testbed : keyword<'@', 't', 'e', 's', 't', 'b', 'e', 'd'> {};
struct keyword_let : keyword<'@', 'l', 'e', 't'> {};
struct keyword_if : keyword<'@', 'i', 'f'> {};
struct keyword_elseif : keyword<'@', 'e', 'l', 's', 'e', 'i', 'f'> {};
struct keyword_else : keyword<'@', 'e', 'l', 's', 'e'> {};
struct keyword_endif : keyword<'@', 'e', 'n', 'd', 'i', 'f'> {};
struct keyword_receive : keyword<'@', 'r', 'e', 'c', 'e', 'i', 'v', 'e'> {};

// SECTION: TOP MATTER

/** Error-recovery rule (defined after block_tag_line). Forward-declared so top
 *  matter can use it too. */
struct malformed_line;

/** Simple `value = 3` kind of set for testbeds. */
struct testbed_set
    : seq<indent, identifier, sp, one<'='>, ws, rvalue_simple, functional_eol> {
};

// Testbeds
struct testbed_open
    : seq<keyword_testbed, plus<blank>, identifier, functional_eol> {};
struct testbed_closed : seq<keyword_end, functional_eol> {};
struct testbed
    : seq<testbed_open, star<sor<ignored, testbed_set>>, testbed_closed> {};

// Let clause
struct let_open : seq<keyword_let, functional_eol> {};
struct let_close : seq<keyword_end, functional_eol> {};
struct let : seq<let_open, star<sor<ignored, declaration>>, let_close> {};

// Receive
struct receive : seq<keyword_receive, ws, module_path, functional_eol> {};

/** The whole top matter section */
struct top_matter
    : star<sor<testbed, let, receive, ignored, malformed_line>> {};

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
struct inline_comment : seq<string<'{', '-', '-', '-'>, until<string<'}'>>> {};

/** Matches anything up to { or EOL */
struct inline_text_segment
    : plus<seq<not_at<string<'{'>>, not_at<eolf>, not_at<move_marker>, any>> {};

/** Any valid part of a text sequence */
struct text_content_part
    : sor<inline_comment, text_injection, inline_text_segment> {};

/** A piece of text content (an array of parts) */
struct text_content : plus<text_content_part> {};

// SECTION: OPERATIONS

struct op_mutate_start : seq<one<'~'>, ws, identifier, ws> {};
struct op_mutate_equate : seq<op_mutate_start, operator_equals, ws, rvalue> {};
struct op_mutate_switch : seq<op_mutate_start, operator_equals_switch> {};
struct math_rvalue : sor<r_variable, val_int, val_float, r_method> {};
struct op_mutate_add
    : seq<op_mutate_start, operator_plus_equals, ws, math_rvalue> {};
struct op_mutate_subtract
    : seq<op_mutate_start, operator_minus_equals, ws, math_rvalue> {};

/** A variable mutation of any kind, including equates */
struct op_mutation : sor<op_mutate_equate, op_mutate_switch, op_mutate_add,
                         op_mutate_subtract> {};

/** You can use basically any string for your module path; we'll check validity
 * later in the LSP */
struct keyword_go : keyword<'G', 'O'> {};
struct keyword_exit : keyword<'E', 'X', 'I', 'T'> {};
struct op_exit : seq<keyword_exit, opt<sp, rvalue>> {};

struct move_child : seq<one<'.'>, identifier> {};
struct move_sib : seq<one<'-'>, identifier> {};
struct move_parent : one<'^'> {};
struct move_identifier_short : plus<sor<move_child, move_sib, move_parent>> {};
struct move_identifier_full
    : seq<identifier,
          opt<seq<one<'.'>, identifier, opt<seq<one<'.'>, identifier>>>>> {};
struct op_go_start_tag : seq<sp, move_marker, ws, move_identifier_full, ws> {};
struct op_go : seq<keyword_go, plus<space>, module_path, opt<op_go_start_tag>> {
};
struct op_move : seq<move_marker, ws,
                     sor<move_identifier_full, move_identifier_short>, ws> {};
struct op_method : seq<one<':'>, identifier, paren<opt<arg_list>>> {};
struct operation : sor<op_move, op_method, op_mutation, op_go, op_exit> {};

// SECTION: BEATS

/** The tag part of a block tag */
struct block_tag_name : identifier {};

/** Block tags, e.g.:
 *
 *  - # top_level
 *  - ## child_tag
 *  - ### grandchild_tag
 */
struct block_tag_line
    : seq<block_prefix, one<' '>, block_tag_name, functional_eol> {};

/** The `some_tag: ...` part of a beat. */
struct beat_attribution : seq<ws, identifier, one<':'>, ws> {};

/** A line with an optional attribution that is not indented or blank
 *
 *  - alice: Hey there!
 */
struct beat : seq<opt<beat_attribution>,  // Optional attribution
                  text_content, eolf> {}; // The text content

/** Operation + optional line comment, to EOL|F */
struct op_end : seq<operation, functional_eol> {};

/** A member; of a block, or of a choice. */
struct member : seq<not_at<seq<ws, eolf>>,  // Not at end of line or whitespace
                    not_at<choice_prefix>,  // Not a choice
                    not_at<block_tag_line>, // Not at a new block
                    opt<seq<conditional, ws>>, // Optional conditional
                    sor<op_end, beat>> {};

/** A non-indented member belongs to the block level. */
struct base_member : seq<not_at<indent>, member> {};

/** A choice member is like a normal member, but indented */
struct choice_member : seq<indent, member> {};

// SECTION: CHOICES

struct inline_choice_move : op_move {};

/** The initial line e.g. `> Some choice` */
struct choice_line : seq<choice_prefix, ws, opt<conditional>, ws, text_content,
                         opt<inline_choice_move>, eolf> {};

/** The choice line with optional indented member lines, including child beats
 *
 *  - > Go left
 *  -   :do_operation()
 */
struct choice_clause : seq<choice_line, star<choice_member>> {};

/** A group of choices, corresponding to ChoiceGroup
 *
 *  - > First choice
 *  - > Second choice
 */
struct choice_block : plus<choice_clause> {};

// SECTION: CONDITIONAL CHAINS

using block_member =
    seq<not_at<one<'@'>>, sor<ignored, choice_block, base_member>>;
using block_members = star<block_member>;

struct cond_chain_if
    : seq<keyword_if, sp, checkable_clause, ws, functional_eol> {};
struct cond_chain_if_block : seq<cond_chain_if, block_members> {};
struct cond_chain_elseif
    : seq<keyword_elseif, sp, checkable_clause, ws, functional_eol> {};
struct cond_chain_elseif_block : seq<cond_chain_elseif, block_members> {};
struct cond_chain_else : seq<keyword_else, ws, functional_eol> {};
struct cond_chain_else_block : seq<cond_chain_else, block_members> {};
struct cond_chain_endif : seq<keyword_endif, ws, functional_eol> {};
struct cond_chain : seq<cond_chain_if_block, star<cond_chain_elseif_block>,
                        opt<cond_chain_else_block>, cond_chain_endif> {};

// SECTION: BLOCKS

/** Error recovery: a non-empty line that no real member rule could consume
 *  (e.g. an old `--` comment after an operation, or a stray indented line).
 *  Used in both top matter and blocks; tried only after every real rule fails,
 *  and never swallows a valid block tag line. An action emits a ParseError so
 *  callers (skalder, LSP) can surface it, and parsing continues instead of
 *  silently abandoning the rest of the file. */
struct malformed_line : seq<not_at<block_tag_line>, not_at<eolf>, until<eolf>> {
};

/** A `block` starts with a tag line, then has beats, comments/blank,
 * operations, choice blocks until the next block starts. */
struct block
    : seq<block_tag_line,
          star<sor<cond_chain, block_member, malformed_line>>> {};

// SECTION: FULL GRAMMAR

struct grammar : seq<star<ignored>, // Skip initial comments/blanks
                     top_matter,    // testbed, module vars, etc
                     plus<block>,   // One or more blocks
                     opt<eof>       // Optional EOF (more forgiving)
                     > {};

} // namespace Skald
