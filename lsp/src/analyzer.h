#pragma once

#include "document.h"
#include "lsp_types.h"
#include <optional>
#include <string>
#include <vector>

namespace SkaldLsp {

// Determine completion context from text before cursor
enum class CompletionContext { None, MoveTarget, Variable, Method, GoPath };

CompletionContext detect_completion_context(const std::string &line_text,
                                            int character);

// Build completion items based on context
std::vector<LspTypes::CompletionItem>
get_completions(const Document &doc, int line, int character,
                const std::vector<std::string> &workspace_files);

// Build document symbols for outline
std::vector<LspTypes::DocumentSymbol> get_document_symbols(const Document &doc);

// Get hover info for symbol at position
std::optional<std::string> get_hover(const Document &doc, int line,
                                      int character);

} // namespace SkaldLsp
