#pragma once

#include <tao/pegtl.hpp>

using namespace tao::pegtl;

namespace Skald {

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

} // namespace Skald
