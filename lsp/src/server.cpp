#include "server.h"
#include "analyzer.h"
#include "transport.h"
#include <iostream>

namespace SkaldLsp {

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

void Server::handle_did_open(const json &params) {
    auto td = params["textDocument"];
    std::string uri = td["uri"].get<std::string>();
    std::string text = td["text"].get<std::string>();

    documents_[uri] = std::make_unique<Document>(uri, text);
    publish_diagnostics(uri);
}

void Server::handle_did_change(const json &params) {
    auto td = params["textDocument"];
    std::string uri = td["uri"].get<std::string>();

    auto &changes = params["contentChanges"];
    if (!changes.empty()) {
        // Full sync mode: take the last full content
        std::string text = changes.back()["text"].get<std::string>();
        auto it = documents_.find(uri);
        if (it != documents_.end()) {
            it->second->update(text);
        } else {
            documents_[uri] = std::make_unique<Document>(uri, text);
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

    // Handle file references (GO path)
    if (sym->kind == SymbolKind::FileRef) {
        // Resolve path relative to workspace root
        std::string file_path =
            workspace_.root_path() + "/" + sym->name;
        std::string file_uri = Workspace::path_to_uri(file_path);
        LspTypes::Location loc;
        loc.uri = file_uri;
        loc.range = {{0, 0}, {0, 0}};
        return Transport::make_response(id, loc);
    }

    // Find definition in current document
    auto def = doc.find_definition(sym->name, sym->kind);
    if (!def) {
        return Transport::make_response(id, json(nullptr));
    }

    LspTypes::Location loc;
    loc.uri = tdp.textDocument.uri;
    loc.range = {{def->range.line, def->range.col},
                 {def->range.line, def->range.end_col}};

    return Transport::make_response(id, loc);
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
    auto items = get_completions(doc, tdp.position.line,
                                  tdp.position.character,
                                  workspace_.ska_files());

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
