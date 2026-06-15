#pragma once

#include "shared_grammar.h"
#include <tao/pegtl.hpp>

using namespace tao::pegtl;

namespace Skald {

// SECTION: KEYWORDS

struct keyword_methods : keyword<'@', 'm', 'e', 't', 'h', 'o', 'd', 's'> {};
struct keyword_globals : keyword<'@', 'g', 'l', 'o', 'b', 'a', 'l', 's'> {};

// SECTION: ERROR RECOVERY

/** A non-empty line that no real codex rule could consume. Tried only after the
 *  real rules fail, and never swallows an `@end` (so an open @methods/@globals
 *  block can still close). An action emits a ParseError and parsing continues
 *  instead of silently abandoning the rest of the codex. */
struct codex_malformed_line
    : seq<not_at<keyword_end>, not_at<eolf>, until<eolf>> {};

// SECTION: METHODS

struct type_action : keyword<'a', 'c', 't', 'i', 'o', 'n'> {};
struct method_signature : sor<value_type, type_action> {};
struct methods_open : seq<keyword_methods, functional_eol> {};
struct methods_close : seq<keyword_end, functional_eol> {};
struct arg_def : seq<identifier, sp, value_type> {};
struct arg_def_list : list<arg_def, arg_separator> {};
struct method_id : identifier {};
struct method_def : seq<indent, method_id, paren<opt<arg_def_list>>, sp,
                        method_signature, functional_eol> {};
struct methods
    : seq<methods_open, star<sor<ignored, method_def, codex_malformed_line>>,
          methods_close> {};

// SECTION: GLOBALS

struct globals_open : seq<keyword_globals, functional_eol> {};
struct globals_close : seq<keyword_end, functional_eol> {};
struct globals
    : seq<globals_open, star<sor<ignored, declaration, codex_malformed_line>>,
          globals_close> {};

// SECTION: FINAL GRAMMAR

struct codex_grammar
    : seq<star<sor<methods, globals, ignored, codex_malformed_line>>,
          opt<eof>> {};

} // namespace Skald
