#include "server.h"
#include "analyzer.h"
#include "transport.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace SkaldLsp {

// A .codex file is the project's mother codex, not a .ska script. It must be
// parsed by the codex grammar (via CodexCache), never the .ska Document parser
// — otherwise every @methods/@globals/declaration line trips the .ska grammar's
// malformed-line recovery and floods the file with bogus diagnostics.
static bool is_codex_uri(const std::string &uri) {
    return uri.size() >= 6 && uri.substr(uri.size() - 6) == ".codex";
}

void Server::handle_message(const json &msg) {
    std::string method;
    if (msg.contains("method")) {
        method = msg["method"].get<std::string>();
    }

    json id = msg.contains("id") ? msg["id"] : json(nullptr);
    json params = msg.contains("params") ? msg["params"] : json::object();

    // Requests (have id)
    if (!id.is_null()) {
        if (method == "initialize") {
            Transport::write_message(handle_initialize(id, params));
        } else if (method == "shutdown") {
            Transport::write_message(handle_shutdown(id));
        } else if (method == "textDocument/definition") {
            Transport::write_message(handle_definition(id, params));
        } else if (method == "textDocument/references") {
            Transport::write_message(handle_references(id, params));
        } else if (method == "textDocument/completion") {
            Transport::write_message(handle_completion(id, params));
        } else if (method == "textDocument/hover") {
            Transport::write_message(handle_hover(id, params));
        } else if (method == "textDocument/documentSymbol") {
            Transport::write_message(handle_document_symbol(id, params));
        } else {
            // Unknown method
            Transport::write_message(
                Transport::make_error(id, -32601, "Method not found: " + method));
        }
        return;
    }

    // Notifications (no id)
    if (method == "initialized") {
        // Client acknowledged initialization — nothing to do
    } else if (method == "exit") {
        handle_exit();
    } else if (method == "textDocument/didOpen") {
        handle_did_open(params);
    } else if (method == "textDocument/didChange") {
        handle_did_change(params);
    } else if (method == "textDocument/didClose") {
        handle_did_close(params);
    } else if (method == "workspace/didChangeWatchedFiles") {
        handle_did_change_watched_files(params);
    }
}

json Server::handle_initialize(const json &id, const json &params) {
    initialized_ = true;

    // Extract workspace root
    if (params.contains("rootUri") && !params["rootUri"].is_null()) {
        workspace_.set_root(params["rootUri"].get<std::string>());
    } else if (params.contains("rootPath") && !params["rootPath"].is_null()) {
        workspace_.set_root(
            Workspace::path_to_uri(params["rootPath"].get<std::string>()));
    }

    json capabilities = {
        {"textDocumentSync",
         {{"openClose", true}, {"change", 1} /* Full sync */}},
        {"completionProvider", {{"triggerCharacters", {">", ":", "~", " "}}}},
        {"definitionProvider", true},
        {"referencesProvider", true},
        {"hoverProvider", true},
        {"documentSymbolProvider", true}};

    json result = {{"capabilities", capabilities},
                   {"serverInfo", {{"name", "skald-lsp"}, {"version", "0.1.0"}}}};

    return Transport::make_response(id, result);
}

json Server::handle_shutdown(const json &id) {
    shutdown_ = true;
    return Transport::make_response(id, json(nullptr));
}

void Server::handle_exit() { exit_ = true; }

const Skald::Codex *Server::codex_for_uri(const std::string &uri) {
    std::string path = Workspace::uri_to_path(uri);
    return codex_cache_.codex_for(path);
}

void Server::ensure_project_index(const Skald::Codex *codex, bool force) {
    if (!codex)
        return;
    std::string full = (fs::path(codex->path) / codex->filename).string();
    if (!force && full == indexed_codex_path_)
        return;
    project_index_.build(codex);
    indexed_codex_path_ = full;
}

std::string Server::rel_path_for(const std::string &uri,
                                 const Skald::Codex *codex) {
    if (!codex)
        return "";
    std::error_code ec;
    auto rel = fs::relative(Workspace::uri_to_path(uri), codex->path, ec);
    return ec ? "" : rel.generic_string();
}

void Server::publish_codex_diagnostics(const std::string &ska_uri) {
    std::string codex_path =
        codex_cache_.codex_path_for(Workspace::uri_to_path(ska_uri));
    if (codex_path.empty())
        return;
    json diags = json::array();
    for (auto &d : codex_cache_.diagnostics_for_codex(codex_path))
        diags.push_back(d);
    Transport::write_message(Transport::make_notification(
        "textDocument/publishDiagnostics",
        {{"uri", Workspace::path_to_uri(codex_path)}, {"diagnostics", diags}}));
}

void Server::handle_did_open(const json &params) {
    auto td = params["textDocument"];
    std::string uri = td["uri"].get<std::string>();
    std::string text = td["text"].get<std::string>();

    // Opening the codex itself: parse it as a codex, not a .ska document.
    // codex_for_uri() loads it into the cache; then publish its own diagnostics.
    if (is_codex_uri(uri)) {
        codex_for_uri(uri);
        publish_codex_diagnostics(uri);
        return;
    }

    const Skald::Codex *codex = codex_for_uri(uri);
    ensure_project_index(codex, false);
    documents_[uri] =
        std::make_unique<Document>(uri, text, codex, &project_index_);
    publish_diagnostics(uri);
    publish_codex_diagnostics(uri);
}

void Server::handle_did_change(const json &params) {
    auto td = params["textDocument"];
    std::string uri = td["uri"].get<std::string>();

    auto &changes = params["contentChanges"];
    if (!changes.empty()) {
        // Editing the codex itself: re-parse via the codex grammar (from disk),
        // never the .ska Document parser. Drop the stale cache so codex_for_uri
        // reloads, then republish the codex's own diagnostics.
        if (is_codex_uri(uri)) {
            codex_cache_.invalidate(Workspace::uri_to_path(uri));
            codex_for_uri(uri);
            publish_codex_diagnostics(uri);
            return;
        }

        // Full sync mode: take the last full content
        std::string text = changes.back()["text"].get<std::string>();
        const Skald::Codex *codex = codex_for_uri(uri);
        ensure_project_index(codex, false);
        // Re-parse in place on the common path (text edits against the same
        // mother codex); only rebuild the Document if its codex changed, since
        // update() keeps the codex/project pointers it was constructed with.
        auto it = documents_.find(uri);
        if (it != documents_.end() && it->second->codex() == codex &&
            it->second->project() == &project_index_) {
            it->second->update(text);
        } else {
            documents_[uri] =
                std::make_unique<Document>(uri, text, codex, &project_index_);
        }
        publish_diagnostics(uri);
    }
}

void Server::handle_did_close(const json &params) {
    auto td = params["textDocument"];
    std::string uri = td["uri"].get<std::string>();
    documents_.erase(uri);

    // Clear diagnostics for closed document
    Transport::write_message(Transport::make_notification(
        "textDocument/publishDiagnostics",
        {{"uri", uri}, {"diagnostics", json::array()}}));
}

json Server::handle_definition(const json &id, const json &params) {
    auto tdp = params.get<LspTypes::TextDocumentPositionParams>();
    auto it = documents_.find(tdp.textDocument.uri);
    if (it == documents_.end()) {
        return Transport::make_response(id, json(nullptr));
    }

    auto &doc = *it->second;
    auto sym = doc.symbol_at(tdp.position.line, tdp.position.character);
    if (!sym) {
        return Transport::make_response(id, json(nullptr));
    }

    // Handle file references (GO path). GO paths resolve relative to the codex
    // root, not the workspace (git) root; fall back to workspace root if there
    // is no codex.
    if (sym->kind == SymbolKind::FileRef) {
        const Skald::Codex *codex = doc.codex();
        std::string base = codex ? codex->path : workspace_.root_path();
        std::string file_path = (fs::path(base) / sym->name).string();
        std::string file_uri = Workspace::path_to_uri(file_path);
        LspTypes::Location loc;
        loc.uri = file_uri;
        loc.range = {{0, 0}, {0, 0}};
        return Transport::make_response(id, loc);
    }

    // Same-file definition (fast path).
    auto def = doc.find_definition(sym->name, sym->kind);
    if (def) {
        LspTypes::Location loc;
        loc.uri = tdp.textDocument.uri;
        loc.range = {{def->range.line, def->range.col},
                     {def->range.line, def->range.end_col}};
        return Transport::make_response(id, loc);
    }

    // Methods are defined in the codex.
    if (sym->kind == SymbolKind::Method) {
        if (auto md = project_index_.resolve_method(sym->name)) {
            LspTypes::Location loc;
            loc.uri = md->uri;
            loc.range = {{md->line, md->col}, {md->line, md->end_col}};
            return Transport::make_response(id, loc);
        }
    }

    if (sym->kind == SymbolKind::Variable) {
        // Cross-file: codex global or thread var from a predecessor module.
        std::string rel = rel_path_for(tdp.textDocument.uri, doc.codex());
        if (auto vd = project_index_.resolve_external_var(sym->name, rel)) {
            LspTypes::Location loc;
            loc.uri = vd->uri;
            loc.range = {{vd->line, vd->col}, {vd->line, vd->end_col}};
            return Transport::make_response(id, loc);
        }
        // Local/ad-hoc variable: jump to the topmost place it's set.
        if (auto a = doc.find_first_assignment(sym->name)) {
            LspTypes::Location loc;
            loc.uri = tdp.textDocument.uri;
            loc.range = {{a->range.line, a->range.col},
                         {a->range.line, a->range.end_col}};
            return Transport::make_response(id, loc);
        }
    }

    return Transport::make_response(id, json(nullptr));
}

json Server::handle_references(const json &id, const json &params) {
    auto tdp = params.get<LspTypes::TextDocumentPositionParams>();
    auto it = documents_.find(tdp.textDocument.uri);
    if (it == documents_.end()) {
        return Transport::make_response(id, json::array());
    }

    auto &doc = *it->second;
    auto sym = doc.symbol_at(tdp.position.line, tdp.position.character);
    if (!sym) {
        return Transport::make_response(id, json::array());
    }

    auto refs = doc.find_references(sym->name, sym->kind);
    json locations = json::array();
    for (auto &ref : refs) {
        LspTypes::Location loc;
        loc.uri = tdp.textDocument.uri;
        loc.range = {{ref.range.line, ref.range.col},
                     {ref.range.line, ref.range.end_col}};
        locations.push_back(loc);
    }

    return Transport::make_response(id, locations);
}

json Server::handle_completion(const json &id, const json &params) {
    auto tdp = params.get<LspTypes::TextDocumentPositionParams>();
    auto it = documents_.find(tdp.textDocument.uri);
    if (it == documents_.end()) {
        return Transport::make_response(id, json::array());
    }

    auto &doc = *it->second;

    // GO paths resolve relative to the codex root, not the workspace (git)
    // root, so completion paths must be codex-relative too. Re-base the
    // workspace's .ska list onto the document's codex root, dropping any file
    // outside it (unreachable by a relative GO). No codex => fall back to the
    // workspace-relative list.
    std::vector<std::string> ska_files;
    const Skald::Codex *codex = doc.codex();
    if (codex) {
        for (auto &rel : workspace_.ska_files()) {
            fs::path abs = fs::path(workspace_.root_path()) / rel;
            std::error_code ec;
            auto codex_rel = fs::relative(abs, codex->path, ec);
            if (ec)
                continue;
            std::string s = codex_rel.generic_string();
            if (s.rfind("..", 0) == 0) // outside codex root
                continue;
            ska_files.push_back(s);
        }
    } else {
        ska_files = workspace_.ska_files();
    }

    auto items = get_completions(doc, tdp.position.line,
                                  tdp.position.character, ska_files);

    json result = json::array();
    for (auto &item : items) {
        result.push_back(item);
    }

    return Transport::make_response(id, result);
}

json Server::handle_hover(const json &id, const json &params) {
    auto tdp = params.get<LspTypes::TextDocumentPositionParams>();
    auto it = documents_.find(tdp.textDocument.uri);
    if (it == documents_.end()) {
        return Transport::make_response(id, json(nullptr));
    }

    auto &doc = *it->second;
    auto hover_text =
        get_hover(doc, tdp.position.line, tdp.position.character);

    if (!hover_text) {
        return Transport::make_response(id, json(nullptr));
    }

    json result = {{"contents", {{"kind", "markdown"}, {"value", *hover_text}}}};
    return Transport::make_response(id, result);
}

json Server::handle_document_symbol(const json &id, const json &params) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();
    auto it = documents_.find(uri);
    if (it == documents_.end()) {
        return Transport::make_response(id, json::array());
    }

    auto &doc = *it->second;
    auto symbols = get_document_symbols(doc);

    json result = json::array();
    for (auto &sym : symbols) {
        result.push_back(sym);
    }

    return Transport::make_response(id, result);
}

void Server::handle_did_change_watched_files(const json &params) {
    workspace_.refresh();

    // Invalidate any changed .codex files so they re-parse.
    bool codex_changed = false;
    if (params.contains("changes")) {
        for (auto &ch : params["changes"]) {
            std::string uri = ch.value("uri", std::string{});
            if (uri.size() >= 6 && uri.substr(uri.size() - 6) == ".codex") {
                codex_cache_.invalidate(Workspace::uri_to_path(uri));
                codex_changed = true;
            }
        }
    }

    // Re-parse open documents against fresh codex/project state. Their
    // method/global checks depend on the codex, so this must cascade. Clearing
    // the indexed path forces one rebuild per distinct codex below.
    indexed_codex_path_.clear();
    for (auto &[uri, doc] : documents_) {
        const Skald::Codex *codex = codex_for_uri(uri);
        ensure_project_index(codex, /*force=*/false);
        // A watched-file change can swap the mother codex, so rebuild against
        // the fresh codex/project. The new Document copies the old text before
        // the assignment frees the old doc, so no separate text copy is needed.
        doc = std::make_unique<Document>(uri, doc->text(), codex,
                                         &project_index_);
    }
    for (auto &[uri, doc] : documents_) {
        publish_diagnostics(uri);
        if (codex_changed)
            publish_codex_diagnostics(uri);
    }
}

void Server::publish_diagnostics(const std::string &uri) {
    auto it = documents_.find(uri);
    if (it == documents_.end())
        return;

    auto &doc = *it->second;
    json diags = json::array();
    for (auto &d : doc.diagnostics()) {
        diags.push_back(d);
    }

    Transport::write_message(Transport::make_notification(
        "textDocument/publishDiagnostics",
        {{"uri", uri}, {"diagnostics", diags}}));
}

} // namespace SkaldLsp
