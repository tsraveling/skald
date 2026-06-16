#pragma once

#include "lsp_types.h"
#include "skald.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace SkaldLsp {

// Finds and caches the "mother codex" for any .ska file: the nearest ancestor
// directory holding exactly one .codex (same rule as the engine's
// FileManager::find_project_root). Parses it once, caches the resulting
// Skald::Codex keyed by codex path, and re-parses when invalidated.
class CodexCache {
public:
    // Returns the codex governing `ska_fs_path` (a filesystem path), or nullptr
    // when there is no mother codex (or it could not be resolved). The returned
    // pointer is stable until the codex is invalidated.
    const Skald::Codex *codex_for(const std::string &ska_fs_path);

    // Filesystem path of the .codex file governing `ska_fs_path`, or "".
    std::string codex_path_for(const std::string &ska_fs_path);

    // Diagnostics produced while parsing a given codex file (parse errors in
    // the .codex itself), keyed by codex filesystem path.
    const std::vector<LspTypes::Diagnostic> &
    diagnostics_for_codex(const std::string &codex_fs_path) const;

    // Drop a cached codex (e.g. the .codex file changed) so it re-parses.
    void invalidate(const std::string &codex_fs_path);

private:
    struct Entry {
        std::unique_ptr<Skald::Codex> codex;
        std::vector<LspTypes::Diagnostic> diagnostics;
    };
    Entry &load(const std::string &codex_fs_path);

    std::unordered_map<std::string, Entry> cache_; // by codex fs path
    std::vector<LspTypes::Diagnostic> empty_;
};

} // namespace SkaldLsp
