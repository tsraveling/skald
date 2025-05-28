#pragma once
#include "logger.h"
#include "skald.h"

namespace Skald {

struct ParseState {
  /** The module attached to the parsed file */
  Module module;

  /** The block currently under construction */
  Block *current_block = nullptr;

  /** Construct with filename */
  ParseState(const std::string &filename) { module.filename = filename; }

  /** Creates a block with the given tag and sets it as current in the parse
   * state */
  void start_block(const std::string &tag) {
    Log::verbose("Starting new block:", tag);

    Block new_block;
    new_block.tag = tag;
    module.blocks[tag] = new_block;

    current_block = &module.blocks[tag];
  }

  /** Creates a beat and adds it to the current block */
  void add_beat(const std::string &text) {
    if (current_block) {
      Log::verbose("Adding beat to block", current_block->tag, " => ", text);

      Beat beat;
      beat.text = text;
      current_block->beats.push_back(beat);
    } else {
      Log::err("Found beat but there is no current block!");
    }
  }
};

} // namespace Skald
