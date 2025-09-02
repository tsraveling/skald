#pragma once

#include <map>
#include <string>
#include <vector>

namespace Skald {

struct TextInsertion {
  std::string variable_name;
};

using TextPart = std::variant<std::string, TextInsertion>;

struct TextContent {
  std::vector<TextPart> parts;
  std::string dbg_desc() const {
    std::string ret = "";
    for (auto &part : parts) {
      if (std::holds_alternative<std::string>(part)) {
        ret += std::get<std::string>(part);
      } else {
        auto ins = std::get<TextInsertion>(part);
        ret += "{" + ins.variable_name + "}";
      }
    }
    return ret;
  }
};

struct Choice {
  TextContent content;
  // STUB: Operations will go here
};

struct Beat {
  std::string attribution;
  TextContent content;
};

// TODO: Support the "auto-continue" block
// (that is untagged and follows an inline choice block)
struct Block {
  std::string tag;
  std::vector<Beat> beats{};
  std::vector<Choice> choices;
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
