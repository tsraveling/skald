#pragma once
#include "debug.h"
#include "logger.h"
#include "skald.h"
#include <vector>

namespace Skald {

struct ParseState {
  /** The module attached to the parsed file */
  Module module;

  /** The block currently under construction */
  Block *current_block = nullptr;

  /** The current beat stack */
  std::vector<TextPart> text_content_queue;

  /** Tag holder, might be used by multiple entities */
  std::string current_tag;

  /** Construct with filename */
  ParseState(const std::string &filename) { module.filename = filename; }

  /** Will either append to the last string if also a simple string, or add it
   * to the stack if not. */
  void add_text_string(std::string str) {
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

  /** Creates a block with the given tag and sets it as current in the parse
   * state */
  void start_block(const std::string &tag) {
    Log::verbose("Starting new block:", tag);

    Block new_block;
    new_block.tag = tag;
    module.blocks[tag] = new_block;

    current_block = &module.blocks[tag];
  }

  /** Called whenever a text queue is concluded */
  void conclude_text() {
    dbg_out(">>> conclude_text()");
    add_beat();
  }

  /** Creates a beat and adds it to the current block */
  void add_beat() {
    dbg_out(">>> add_beat()");
    if (current_block) {
      Log::verbose(
          "Adding beat to block with", text_content_queue.size(), "parts",
          current_tag.length() > 0 ? "(tag: " + current_tag + ")" : "(no tag)");

      Beat beat;
      beat.content.parts = std::move(text_content_queue);
      beat.attribution = current_tag;
      current_block->beats.push_back(beat);
      current_tag = "";
    } else {
      Log::err("Found beat but there is no current block!");
    }
  }
};

} // namespace Skald
