#include "parse_state.h"
#include "debug.h"
#include "logger.h"
#include "skald.h"
#include <utility>

namespace Skald {

ParseState::ParseState(const std::string &filename, const Codex *c) {
  module.filename = filename;
  codex = c;
}

// SECTION: MODULE LEVEL

// SECTION: ERROR HANDLING

void ParseState::err(const tao::pegtl::position pos, std::string msg) {
  errors.push_back(ParseError{.pos = from_pos(pos),
                              .msg = msg,
                              .severity = ParseError::Severity::ERROR});

  dbg_out("XXX ERR: " << msg);
}
void ParseState::warn(const tao::pegtl::position pos, std::string msg) {
  errors.push_back(ParseError{.pos = from_pos(pos),
                              .msg = msg,
                              .severity = ParseError::Severity::WARNING});
}

// SECTION: TOP MATTER

// SECTION: BLOCKS

void ParseState::start_block(const std::string &tag) {
  Log::verbose("Starting new block:", tag);

  Block new_block;
  // STUB: Use stored tag level
  new_block.tag = tag;
  module.blocks.push_back(new_block);
  dbg_out(">>> [] block_lookup[" << tag << "] = " << module.blocks.size() - 1);
  module.block_lookup[tag] = module.blocks.size() - 1;

  current_block = &module.blocks.back();
}
void ParseState::add_choice_member(Member mem) {
  assert(choice_stack.size() > 0);
  choice_stack.back().members.push_back(std::move(mem));
}

/** Adds a member either to the main thread, or the open conditional block */
void ParseState::add_member(BlockMember mem) {
  if (open_chain != nullptr) {
    assert(open_chain->cond_blocks.size() > 0); // must have members
    open_chain->cond_blocks.back().members.push_back(std::move(mem));
    dbg_out("   ... added member to open conditional block.\n");
  } else {
    current_block->members.push_back(MainBlockMember{std::move(mem)});
    dbg_out("   ... added member to base stack.\n");
  }
}

// SECTION: BEATS

void ParseState::add_beat(int line_number) {
  if (!current_block) {
    Log::err("Found beat but there is no current block!");
  }
  Log::verbose(" - Adding beat.");

  Beat beat;

  // Grab the text content
  beat.content.parts = std::move(text_content_queue);

  // Consume the attribution tag if there is one
  beat.attribution = current_attrib_tag;
  current_attrib_tag = "";
  beat.line_number = line_number;
  member_body_buffer = beat;
}

// SECTION: CHOICES

void ParseState::add_choice_group(int line_number) {
  ChoiceGroup grp;
  grp.choices = std::move(choice_stack);
  grp.line_number = line_number;
  add_member(grp);
}

// SECTION: TEXT

void ParseState::add_text_string(std::string str) {
  Log::verbose("Beat queue +=", str);
  if (text_content_queue.empty() ||
      !std::holds_alternative<std::string>(text_content_queue.back())) {
    text_content_queue.push_back(str);
  } else {
    std::string &last_str = std::get<std::string>(text_content_queue.back());
    // Eliminate double spaces for comment joins
    last_str +=
        (last_str.back() == ' ' && str.front() == ' ') ? str.substr(1) : str;
  }
}

RValue ParseState::injectable_buffer_pop() {
  return *std::exchange(injectable_buffer, std::nullopt);
}

// SECTION: CONDITIONALS

void ParseState::conditional_step_in() {
  conditional_stack.push_back(Conditional{});
}

void ParseState::conditional_step_out() {
  if (conditional_stack.size() > 1) {
    // If this isn't the first item, close it into a conditional item and add
    // it to the next list up
    auto last =
        std::make_shared<Conditional>(std::move(conditional_stack.back()));
    conditional_stack.pop_back();
    conditional_stack.back().items.push_back(last);
  } else if (conditional_stack.size() > 0) {
    // If it's the only item, move it into the conditional buffer
    conditional_buffer = std::move(
        conditional_stack.back()); // Moves the struct's moveable members
    conditional_stack.pop_back();
  } else {
    Log::err("Tried to step out of a conditional but the stack is empty!");
  }
}

std::optional<Conditional> ParseState::conditional_buffer_pop() {
  return std::exchange(conditional_buffer, std::nullopt);
}

void ParseState::add_conditional_atom(const ConditionalAtom &atom) {
  dbg_out(
      "-++ Adding a conditional atom to checkable_queue: " << atom.dbg_desc());
  if (conditional_stack.size() < 1) {
    Log::err("Tried to add a conditional atom, but no conditional was open!");
    return;
  }
  auto &current = conditional_stack.back();
  current.items.push_back(atom);
}

// SECTION: METHODS
void ParseState::validate_method(const MethodCall &m,
                                 const tao::pegtl::position pos) {
  // 1. There must be a codex
  if (!codex) {
    err(pos, "Method calls require a Codex!");
    return;
  }

  // 2. Is method in codex?
  const MethodDef *def = nullptr;
  for (size_t i = 0; i < codex->method_defs.size(); i++) {
    if (codex->method_defs[i].name == m.method) {
      def = &codex->method_defs[i];
      break;
    }
  }
  if (!def) {
    err(pos, "No method by that name is in the Codex.");
    return;
  }

  // 3. Arg count match
  if (m.args.size() != def->args.size()) {
    err(pos, "Method in codex has " + std::to_string(def->args.size()) +
                 " arguments; this one has " + std::to_string(m.args.size()) +
                 ".");
    return;
  }

  // 4. Arg validation
  for (size_t i = 0; i < m.args.size(); i++) {

    // We check explicit arg values at parse time; methods and vars are checked
    // at runtime.
    if (auto srval = cast_rval_to_simple(m.args[i])) {
      auto t = srval_get_type(*srval);
      if (t != def->args[i].type) {
        err(pos, "Type value mismatch; " + val_type_to_str(def->args[i].type) +
                     " was expected, " + val_type_to_str(t) + " was found.");
      }
    }

    // Must be normal RValue (not method call)
    if (rval_get_call(m.args[i])) {
      err(pos, "Methods are not yet supported as arguments of other methods");
    }
  }
}

// SECTION: GO

// SECTION: RVALUES

RValue ParseState::rval_buffer_pop() {
  auto back = rval_buffer.back();
  rval_buffer.pop_back();
  return back;
}

SimpleRValue ParseState::simple_rval_buffer_pop(tao::pegtl::position pos) {
  auto back = rval_buffer.back();
  rval_buffer.pop_back();

  return std::visit(
      [&](auto &&val) -> SimpleRValue {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::string> ||
                      std::is_same_v<T, bool> || std::is_same_v<T, int> ||
                      std::is_same_v<T, float>) {
          return val;
        } else {
          err(pos, "Tried to pop a simple value (int, bool, string, float) but "
                   "got a complex value (variable, method) instead!");
          return false;
        }
      },
      back);
}

// SECTION: ATOMS

std::optional<std::string> ParseState::pop_id_cond() {
  if (last_identifier.length() < 1) {
    return std::nullopt;
  }
  return pop_id();
}

std::string ParseState::pop_id() {
  auto r = last_identifier;
  last_identifier = "";
  return r;
}

void ParseState::do_dbg_desc() {
  dbg_out("MODULE VARS:");
  for (const auto &dec : module.module_vars) {
    dbg_out(" - " << dec.var.dbg_desc() << " = "
                  << rval_to_string(dec.initial_value));
  }

  dbg_out("TESTBEDS:");
  for (const auto &testbed : module.testbeds) {
    dbg_out(testbed.dbg_desc());
  }

  dbg_out("\nSTRUCTURE:");
  // Print details about each block
  for (const auto &block : module.blocks) {
    dbg_out("\n- Block '" << block.tag << "': " << block.members.size()
                          << " members");
    auto print_bm = [](const BlockMember &mem, std::string prefix = "") {
      std::visit(
          [&](const auto &member) {
            using T = std::decay_t<decltype(member)>;
            if constexpr (std::is_same_v<T, Member>) {
              dbg_out(prefix << "  - Member: " << member.dbg_desc());
            } else if constexpr (std::is_same_v<T, ChoiceGroup>) {
              dbg_out(prefix << "  - ChoiceGroup: ");
              for (const auto &choice : member.choices) {
                dbg_out(prefix << "    > Choice: " << choice.dbg_desc());
                for (const auto &cm : choice.members) {
                  dbg_out(prefix << "    >> Choice Member: " << cm.dbg_desc());
                }
              }
            }
          },
          mem);
    };

    for (const auto &mem : block.members) {
      std::visit(
          [&](const auto &m) {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, BlockMember>) {
              print_bm(m);
            } else if constexpr (std::is_same_v<T, ConditionalChain>) {
              dbg_out("  ?? ConditionalChain: " << m.cond_blocks.size()
                                                << " blocks");
              for (const auto &cb : m.cond_blocks) {
                dbg_out("  ?? CondBlock: " << cb.cond.dbg_desc());
                for (const auto &inner : cb.members) {
                  print_bm(inner, "    ?| ");
                }
              }
            }
          },
          mem);
    }
  }
}

} // namespace Skald
