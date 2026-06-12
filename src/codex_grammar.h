#pragma once

#include "shared_grammar.h"
#include <tao/pegtl.hpp>

using namespace tao::pegtl;

namespace Skald {

// SECTION: KEYWORDS

struct keyword_methods : keyword<'@', 'm', 'e', 't', 'h', 'o', 'd', 's'> {};
struct keyword_globals : keyword<'@', 'g', 'l', 'o', 'b', 'a', 'l', 's'> {};

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
    : seq<methods_open, star<sor<ignored, method_def>>, methods_close> {};

// SECTION: GLOBALS

struct globals_open : seq<keyword_globals, functional_eol> {};
struct globals_close : seq<keyword_end, functional_eol> {};
struct globals
    : seq<globals_open, star<sor<ignored, declaration>>, globals_close> {};

// SECTION: FINAL GRAMMAR

struct codex_grammar
    : seq<star<sor<methods, globals, ignored>>, // Skip initial comments/blanks
          opt<eof>> {};

} // namespace Skald
