#include "move_resolver.h"

namespace SkaldLsp {

std::string BlockHierarchy::leaf_of(const std::string &dotted_tag) {
    auto pos = dotted_tag.rfind('.');
    return pos == std::string::npos ? dotted_tag : dotted_tag.substr(pos + 1);
}

BlockHierarchy::BlockHierarchy(const Skald::Module &module) {
    for (const auto &block : module.blocks) {
        tags_.insert(block.tag);
        leaves_.insert(leaf_of(block.tag));
    }
}

bool BlockHierarchy::is_defined(const std::string &dotted_tag) const {
    return tags_.find(dotted_tag) != tags_.end();
}

bool BlockHierarchy::leaf_defined(const std::string &leaf) const {
    return leaves_.find(leaf) != leaves_.end();
}

std::vector<std::string>
open_transitions(const Skald::Module &module,
                 const std::vector<SymbolOccurrence> &symbols) {
    BlockHierarchy hierarchy(module);
    std::vector<std::string> open;
    std::set<std::string> seen;
    for (const auto &sym : symbols) {
        if (sym.kind != SymbolKind::BlockTag || sym.is_definition)
            continue;
        if (hierarchy.is_defined(sym.name))
            continue; // resolves to a real block
        auto leaf = BlockHierarchy::leaf_of(sym.name);
        if (hierarchy.leaf_defined(leaf))
            continue; // a block with this leaf already exists
        if (seen.insert(leaf).second)
            open.push_back(leaf);
    }
    return open;
}

} // namespace SkaldLsp
