#pragma once

#include <map>
#include <string>
#include <vector>

namespace Skald {

struct BeatInsertion {
  std::string variable_name;
};

using BeatPart = std::variant<std::string, BeatInsertion>;

struct Beat {
  std::vector<BeatPart> parts;
};

struct Block {
  std::string tag;
  std::vector<Beat> beats{};
};

class Module {
public:
  std::string filename;
  std::map<std::string, Block> blocks;

  Block *get_block(std::string tag) {
    auto it = blocks.find(tag);
    return (it != blocks.end()) ? &it->second : nullptr;
  }
};

class Skald {
private:
  std::unique_ptr<Module> current;

public:
  void load(std::string path);
  void trace(std::string path);
};

} // namespace Skald
