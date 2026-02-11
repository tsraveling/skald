#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace Transport {

using json = nlohmann::json;

// Read a JSON-RPC message from stdin (Content-Length framed)
std::optional<json> read_message();

// Write a JSON-RPC message to stdout (Content-Length framed)
void write_message(const json &msg);

// Build a JSON-RPC response
json make_response(const json &id, const json &result);

// Build a JSON-RPC error response
json make_error(const json &id, int code, const std::string &message);

// Build a JSON-RPC notification (no id)
json make_notification(const std::string &method, const json &params);

} // namespace Transport
