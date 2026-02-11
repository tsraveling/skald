#include "transport.h"
#include <iostream>
#include <sstream>

namespace Transport {

std::optional<json> read_message() {
    // Read headers until empty line
    std::string line;
    int content_length = -1;

    while (std::getline(std::cin, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Empty line signals end of headers
        if (line.empty()) {
            break;
        }

        // Parse Content-Length header
        const std::string prefix = "Content-Length: ";
        if (line.substr(0, prefix.size()) == prefix) {
            content_length = std::stoi(line.substr(prefix.size()));
        }
    }

    if (content_length < 0) {
        return std::nullopt;
    }

    // Read exactly content_length bytes
    std::string body(content_length, '\0');
    std::cin.read(&body[0], content_length);

    if (std::cin.fail()) {
        return std::nullopt;
    }

    try {
        return json::parse(body);
    } catch (...) {
        return std::nullopt;
    }
}

void write_message(const json &msg) {
    std::string body = msg.dump();
    std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    std::cout.flush();
}

json make_response(const json &id, const json &result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

json make_error(const json &id, int code, const std::string &message) {
    return {{"jsonrpc", "2.0"},
            {"id", id},
            {"error", {{"code", code}, {"message", message}}}};
}

json make_notification(const std::string &method, const json &params) {
    return {{"jsonrpc", "2.0"}, {"method", method}, {"params", params}};
}

} // namespace Transport
