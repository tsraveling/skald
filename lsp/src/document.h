#pragma once

#include "lsp_parse_state.h"
#include "lsp_types.h"
#include "skald.h"
#include <optional>
#include <string>
#include <vector>

namespace SkaldLsp {

class ProjectIndex; // cross-file model (project_index.h)

class Document {
public:
    // codex may be null (no mother codex found for this file). project may be
    // null (no project model available yet). Neither is owned.
    Document(const std::string &uri, const std::string &text,
             const Skald::Codex *codex = nullptr,
             const ProjectIndex *project = nullptr);

    void update(const std::string &text);

    const std::string &uri() const { return uri_; }
    const std::string &text() const { return text_; }
    const Skald::Module &module() const { return module_; }
    const Skald::Codex *codex() const { return codex_; }
    const ProjectIndex *project() const { return project_; }
    const std::vector<SymbolOccurrence> &symbols() const { return symbols_; }
    const std::vector<LspTypes::Diagnostic> &diagnostics() const {
        return diagnostics_;
    }
    bool parse_succeeded() const { return parse_ok_; }

    // Find symbol at a given position (0-based line and character)
    std::optional<SymbolOccurrence> symbol_at(int line, int character) const;

    // Find definition of a symbol by name and kind (same file only)
    std::optional<SymbolOccurrence> find_definition(const std::string &name,
                                                     SymbolKind kind) const;

    // Find all references of a symbol by name and kind
    std::vector<SymbolOccurrence> find_references(const std::string &name,
                                                   SymbolKind kind) const;

    // Topmost assignment (`~ name = …`) of a variable in this file. Used as the
    // "definition" of a local/ad-hoc variable that has no @let or global decl.
    std::optional<SymbolOccurrence>
    find_first_assignment(const std::string &name) const;

    // Get the line of text at 0-based line index
    std::string get_line(int line) const;

    // Get all block tag names defined in this document (dotted form)
    std::vector<std::string> block_tags() const;

    // Get all variable names declared in this document
    std::vector<std::string> variable_names() const;

    // Get all method names used in this document
    std::vector<std::string> method_names() const;

    // Open transitions: -> targets referenced but with no defining block yet.
    // Returns leaf tag names (deduped, excluding already-defined tags).
    std::vector<std::string> open_transitions() const;

private:
    void parse();
    void run_semantic_checks();

    std::string uri_;
    std::string text_;
    const Skald::Codex *codex_ = nullptr;
    const ProjectIndex *project_ = nullptr;
    Skald::Module module_;
    std::vector<SymbolOccurrence> symbols_;
    std::vector<LspTypes::Diagnostic> diagnostics_;
    bool parse_ok_ = false;
};

} // namespace SkaldLsp
