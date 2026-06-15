#pragma once

#include "skald.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SkaldLsp {

// A cross-file definition site for a variable.
struct VarDef {
    std::string uri;
    int line = 0;
    int col = 0;
    int end_col = 0;
    Skald::ValueType type = Skald::ValueType::STRING;
};

// Whole-project model: every .ska under the codex root, its @let module vars,
// and the directed GO graph between modules. A module B "imports" (as thread
// variables) the module vars of every module that can reach B via GO. Used for
// cross-file go-to-definition, hover, and testbed-target validation.
class ProjectIndex {
public:
    // Scan + parse every .ska under the codex root (codex-aware) and the codex
    // globals. Safe to call repeatedly to refresh.
    void build(const Skald::Codex *codex);

    // Does any predecessor module (one that reaches `rel_path` via GO) declare
    // a module var named `name`?
    bool has_thread_var(const std::string &name,
                        const std::string &rel_path) const;

    // Resolve a variable used in module `rel_path` to a cross-file definition:
    // a codex global, or a thread var from a predecessor module. Returns
    // nullopt if neither (callers resolve same-file definitions first).
    std::optional<VarDef> resolve_external_var(const std::string &name,
                                               const std::string &rel_path) const;

private:
    struct ModuleEntry {
        std::string uri;
        std::vector<std::string> go_targets;          // normalized rel paths
        std::unordered_map<std::string, VarDef> vars; // module vars by name
    };

    // All modules that can reach `rel_path` via GO edges, nearest first.
    std::vector<std::string> predecessors(const std::string &rel_path) const;

    std::unordered_map<std::string, ModuleEntry> modules_; // by rel path
    std::unordered_map<std::string, VarDef> globals_;      // codex globals
};

} // namespace SkaldLsp
