#include "parse_state.h"
#include "debug.h"
#include "logger.h"
#include "skald.h"
#include <utility>

namespace Skald {

ParseState::ParseState(const std::string &filename) {
  module.filename = filename;
}

// SECTION: MODULE LEVEL

// SECTION: ERROR HANDLING

void ParseState::err(const tao::pegtl::position pos, std::string msg) {
  errors.push_back(ParseError{
      .pos = pos, .msg = msg, .severity = ParseError::Severity::ERROR});

  dbg_out("XXX ERR: " << msg);
}
void ParseState::warn(const tao::pegtl::position pos, std::string msg) {
  errors.push_back(ParseError{
      .pos = pos, .msg = msg, .severity = ParseError::Severity::WARNING});
}
void ParseState::fail(const tao::pegtl::position pos, std::string msg) {
  errors.push_back(ParseError{
      .pos = pos, .msg = msg, .severity = ParseError::Severity::FATAL});
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

/** Adds a member either to the main thread, or the open conditional block */
void ParseState::add_member(BlockMember mem) {
  if (open_chain != nullptr) {
    assert(open_chain->cond_blocks.size() > 0); // must have members
    open_chain->cond_blocks.back().members.push_back(std::move(mem));
  } else {
    current_block->members.push_back(MainBlockMember{std::move(mem)});
  }
}

// SECTION: BEATS

void ParseState::add_beat(int line_number) {
  if (!current_block) {
    Log::err("Found beat but there is no current block!");
  }
  Log::verbose(" - Adding beat.");

  Beat beat;

  // Will consume a conditional if one is present
  beat.condition.condition = conditional_buffer_pop();

  // Grab the text content
  beat.content.parts = std::move(text_content_queue);

  // Consume the attribution tag if there is one
  beat.attribution = current_attrib_tag;
  current_attrib_tag = "";
  beat.line_number = line_number;
  add_member(beat);
}

// SECTION: CHOICES

Choice *ParseState::add_choice() {
  if (!current_block) {
    Log::err("Found choice but there is no current block!");
    return nullptr;
  }
  Log::verbose(" - Adding choice.");
  Choice choice;
  choice.content.parts = std::move(text_content_queue);
  choice.operations = std::move(operation_queue);
  choice.condition.condition = conditional_buffer_pop();
  choice_stack.push_back(choice);
  return &choice_stack.back();
}

void ParseState::add_choice_group(int line_number) {
  ChoiceGroup grp;
  grp.choices = std::move(choice_stack);
  grp.line_number = line_number;
  add_member(grp);
}

// SECTION: OPERATIONS

Operation ParseState::operation_queue_pop() {
  dbg_out(">>> operation_queue_pop: " << operation_queue.size() << " -1 ");
  auto back = std::move(operation_queue.back());
  operation_queue.pop_back();
  return back;
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

// SECTION: GO

// SECTION: RVALUES

RValue ParseState::rval_buffer_pop() {
  // dbg_out(">>> rval_buffer_pop: " << rval_buffer.size() << " -1 ");
  auto back = rval_buffer.back();
  rval_buffer.pop_back();
  return back;
}

// FIXME: handle errors correctly
SimpleRValue ParseState::simple_rval_buffer_pop() {
  // dbg_out(">>> simple_rval_buffer_pop: " << rval_buffer.size() << " -1 ");
  auto back = rval_buffer.back();
  rval_buffer.pop_back();

  return std::visit(
      [](auto &&val) -> SimpleRValue {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::string> ||
                      std::is_same_v<T, bool> || std::is_same_v<T, int> ||
                      std::is_same_v<T, float>) {
          return val;
        } else {
          throw std::runtime_error("Expected simple RValue, got complex type");
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

} // namespace Skald
