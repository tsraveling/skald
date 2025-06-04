#pragma once
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
  std::vector<BeatPart> beat_queue;

  /** Construct with filename */
  ParseState(const std::string &filename) { module.filename = filename; }

  /** Will either append to the last string if also a simple string, or add it
   * to the stack if not. */
  void add_beat_string(std::string str) {
    Log::verbose("Beat queue +=", str);
    if (beat_queue.empty() ||
        !std::holds_alternative<std::string>(beat_queue.back())) {
      beat_queue.push_back(str);
    } else {
      std::string &last_str = std::get<std::string>(beat_queue.back());
      last_str += str;
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

  /** Creates a beat and adds it to the current block */
  void add_beat() {
    if (current_block) {
      Log::verbose("Adding beat to block.");

      Beat beat;
      beat.parts = std::move(beat_queue);
      current_block->beats.push_back(beat);
    } else {
      Log::err("Found beat but there is no current block!");
    }
  }
};

} // namespace Skald
