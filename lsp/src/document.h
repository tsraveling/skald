#pragma once

#include "lsp_parse_state.h"
#include "lsp_types.h"
#include "skald.h"
#include <optional>
#include <string>
#include <vector>

namespace SkaldLsp {

class Document {
public:
    Document(const std::string &uri, const std::string &text);

    void update(const std::string &text);

    const std::string &uri() const { return uri_; }
    const std::string &text() const { return text_; }
    const Skald::Module &module() const { return module_; }
    const std::vector<SymbolOccurrence> &symbols() const { return symbols_; }
    const std::vector<LspTypes::Diagnostic> &diagnostics() const {
        return diagnostics_;
    }
    bool parse_succeeded() const { return parse_ok_; }

    // Find symbol at a given position (0-based line and character)
    std::optional<SymbolOccurrence> symbol_at(int line, int character) const;

    // Find definition of a symbol by name and kind
    std::optional<SymbolOccurrence> find_definition(const std::string &name,
                                                     SymbolKind kind) const;

    // Find all references of a symbol by name and kind
    std::vector<SymbolOccurrence> find_references(const std::string &name,
                                                   SymbolKind kind) const;

    // Get the line of text at 0-based line index
    std::string get_line(int line) const;

    // Get all block tag names defined in this document
    std::vector<std::string> block_tags() const;

    // Get all variable names declared in this document
    std::vector<std::string> variable_names() const;

    // Get all method names used in this document
    std::vector<std::string> method_names() const;

private:
    void parse();
    void run_semantic_checks();

    std::string uri_;
    std::string text_;
    Skald::Module module_;
    std::vector<SymbolOccurrence> symbols_;
    std::vector<LspTypes::Diagnostic> diagnostics_;
    bool parse_ok_ = false;
};

} // namespace SkaldLsp
