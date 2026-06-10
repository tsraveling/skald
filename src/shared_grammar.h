#pragma once

#include <tao/pegtl.hpp>

using namespace tao::pegtl;

namespace Skald {

// Parentheses
template <typename ParenContent>
struct paren : seq<one<'('>, ParenContent, one<')'>> {};

// Indentation
using two_or_more_spaces = seq<space, space, star<space>>;
using one_or_more_tabs = plus<one<'\t'>>;
using indent = sor<two_or_more_spaces, one_or_more_tabs>;

// End of line / file
using eolf = sor<eol, eof>;

// Line comment using the same comment marker style
struct line_comment : seq<string<'-', '-', '-'>, until<eolf>> {};
struct end_line_comment
    : seq<string<'-', '-', '-'>, star<not_one<'\r', '\n'>>> {};

/** Whitespace means 0-n whitespace characters */
using ws = star<blank>;
using sp = plus<blank>;
struct blank_line : seq<ws, eol> {};
struct ignored : sor<line_comment, blank_line> {};

/** This can be used to cap any single functional line that allows an end
 * comment */
struct functional_eol : seq<ws, opt<end_line_comment>, eolf> {};

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

// SECTION: SHARED KEYWORDS

struct keyword_end : keyword<'@', 'e', 'n', 'd'> {};

// SECTION: LOGIC FUNDAMENTALS

struct variable_name : identifier {};
struct type_int : keyword<'i', 'n', 't'> {};
struct type_float : keyword<'f', 'l', 'o', 'a', 't'> {};
struct type_string : keyword<'s', 't', 'r', 'i', 'n', 'g'> {};
struct type_bool : keyword<'b', 'o', 'o', 'l'> {};

/** A variable name used as an rvalue */
struct arg_list;
struct r_method : seq<one<':'>, identifier, paren<opt<arg_list>>> {};
struct r_variable : variable_name {};
struct rvalue
    : sor<val_bool, val_string, val_float, val_int, r_variable, r_method> {};
struct rvalue_simple : sor<val_bool, val_string, val_float, val_int> {};
struct arg_separator : seq<ws, one<','>, ws> {};

/** Used to define a value type for methods or variables */
using value_type = sor<type_int, type_float, type_string, type_bool>;

/// Declarations, used for module sets and globals in codex files. ///
/// bob string = "bob" ///
struct declaration_type : seq<sp, value_type> {};
struct declaration_default : seq<ws, one<'='>, ws, rvalue_simple> {};
struct declaration : seq<indent, identifier, opt<declaration_type>,
                         opt<declaration_default>, functional_eol> {};

} // namespace Skald
