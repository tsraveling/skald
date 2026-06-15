#pragma once

#include "lsp_parse_state.h"
#include "skald.h"
#include <set>
#include <string>
#include <vector>

namespace SkaldLsp {

// Index of a module's block hierarchy.
//
// The 0.3 grammar allows absolute dotted move targets (`a.b.c`) and relative
// ones (`.child`, `-sib`, `^`, and combinations). The library resolves every
// move expression into an absolute dotted tag *during parsing* (relative to
// the block the transition appears in). This class is the single LSP-side
// place that reasons about those resolved tags: it answers "does this target
// exist?" and computes the "open transitions" used for block-definition
// completion. Adjusting how the LSP treats relative targets happens here.
class BlockHierarchy {
public:
    explicit BlockHierarchy(const Skald::Module &module);

    // Is there a defining block for this absolute dotted tag?
    bool is_defined(const std::string &dotted_tag) const;

    // Is `leaf` the leaf (last dotted component) of any defined block tag?
    bool leaf_defined(const std::string &leaf) const;

    // Leaf name (last dotted component) of a tag.
    static std::string leaf_of(const std::string &dotted_tag);

private:
    std::set<std::string> tags_;   // full dotted tags
    std::set<std::string> leaves_; // leaf names of all defined tags
};

// Open transitions = move references whose resolved target has no defining
// block. Returns the leaf names of those targets, deduped, excluding any name
// that is already a defined block tag (redefining it would be wrong).
std::vector<std::string>
open_transitions(const Skald::Module &module,
                 const std::vector<SymbolOccurrence> &symbols);

} // namespace SkaldLsp
