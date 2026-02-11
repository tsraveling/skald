#pragma once

#include "document.h"
#include "workspace.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace SkaldLsp {

using json = nlohmann::json;

class Server {
public:
    // Process a JSON-RPC message and return optional response
    void handle_message(const json &msg);

    bool should_exit() const { return exit_; }

private:
    // Lifecycle
    json handle_initialize(const json &id, const json &params);
    json handle_shutdown(const json &id);
    void handle_exit();

    // Document sync
    void handle_did_open(const json &params);
    void handle_did_change(const json &params);
    void handle_did_close(const json &params);

    // Navigation
    json handle_definition(const json &id, const json &params);
    json handle_references(const json &id, const json &params);

    // Completions
    json handle_completion(const json &id, const json &params);

    // Workspace
    void handle_did_change_watched_files(const json &params);

    // Hover
    json handle_hover(const json &id, const json &params);

    // Document symbols
    json handle_document_symbol(const json &id, const json &params);

    // Publish diagnostics for a document
    void publish_diagnostics(const std::string &uri);

    // State
    std::unordered_map<std::string, std::unique_ptr<Document>> documents_;
    Workspace workspace_;
    bool initialized_ = false;
    bool shutdown_ = false;
    bool exit_ = false;
};

} // namespace SkaldLsp
