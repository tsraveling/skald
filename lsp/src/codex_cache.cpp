#include "codex_cache.h"
#include "codex_actions.h"
#include "codex_grammar.h"
#include "codex_parse_state.h"
#include "skalder_fs.h"
#include <tao/pegtl.hpp>

namespace SkaldLsp {

const std::vector<LspTypes::Diagnostic> &
CodexCache::diagnostics_for_codex(const std::string &codex_fs_path) const {
    auto it = cache_.find(codex_fs_path);
    if (it == cache_.end())
        return empty_;
    return it->second.diagnostics;
}

void CodexCache::invalidate(const std::string &codex_fs_path) {
    cache_.erase(codex_fs_path);
}

std::string CodexCache::codex_path_for(const std::string &ska_fs_path) {
    FileManager fm;
    auto result = fm.find_project_root(ska_fs_path);
    if (auto *path = std::get_if<std::string>(&result))
        return *path;
    return "";
}

CodexCache::Entry &CodexCache::load(const std::string &codex_fs_path) {
    auto it = cache_.find(codex_fs_path);
    if (it != cache_.end())
        return it->second;

    Entry entry;
    Skald::CodexParseState pstate(codex_fs_path);
    // CodexParseState leaves these declaration flags uninitialized; zero them
    // to avoid spurious type-mismatch errors on the first global.
    pstate.declaration_was_typed = false;
    pstate.declaration_was_valued = false;
    pstate.last_type = Skald::ValueType::STRING;
    try {
        tao::pegtl::file_input input(codex_fs_path);
        tao::pegtl::parse<Skald::codex_grammar, Skald::codex_action>(input,
                                                                     pstate);
    } catch (const tao::pegtl::parse_error &e) {
        const auto &p = e.position_object();
        LspTypes::Diagnostic diag;
        diag.range.start.line = static_cast<int>(p.line) - 1;
        diag.range.start.character = static_cast<int>(p.column) - 1;
        diag.range.end.line = static_cast<int>(p.line) - 1;
        diag.range.end.character = static_cast<int>(p.column);
        diag.severity = LspTypes::DiagnosticSeverity::Error;
        diag.message = std::string(e.message());
        entry.diagnostics.push_back(diag);
    } catch (const std::exception &e) {
        LspTypes::Diagnostic diag;
        diag.severity = LspTypes::DiagnosticSeverity::Error;
        diag.message = std::string("Codex error: ") + e.what();
        entry.diagnostics.push_back(diag);
    }

    // Surface structured codex parse errors (1-based -> 0-based).
    for (const auto &err : pstate.errors) {
        LspTypes::Diagnostic diag;
        diag.range.start.line = static_cast<int>(err.pos.line) - 1;
        diag.range.start.character = static_cast<int>(err.pos.column) - 1;
        diag.range.end.line = static_cast<int>(err.pos.line) - 1;
        diag.range.end.character = static_cast<int>(err.pos.column);
        diag.severity = err.severity == Skald::ParseError::WARNING
                            ? LspTypes::DiagnosticSeverity::Warning
                            : LspTypes::DiagnosticSeverity::Error;
        diag.message = err.msg;
        entry.diagnostics.push_back(diag);
    }

    entry.codex = std::make_unique<Skald::Codex>(std::move(pstate.codex));
    auto [inserted, _] = cache_.emplace(codex_fs_path, std::move(entry));
    return inserted->second;
}

const Skald::Codex *CodexCache::codex_for(const std::string &ska_fs_path) {
    std::string codex_path = codex_path_for(ska_fs_path);
    if (codex_path.empty())
        return nullptr;
    return load(codex_path).codex.get();
}

} // namespace SkaldLsp
