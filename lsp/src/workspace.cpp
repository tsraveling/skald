#include "workspace.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace SkaldLsp {

void Workspace::set_root(const std::string &root_uri) {
    root_uri_ = root_uri;
    root_path_ = uri_to_path(root_uri);
    scan();
}

void Workspace::scan() {
    ska_files_.clear();

    if (root_path_.empty())
        return;

    try {
        for (auto &entry : fs::recursive_directory_iterator(root_path_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".ska") {
                auto rel = fs::relative(entry.path(), root_path_).string();
                ska_files_.push_back(rel);
            }
        }
        std::sort(ska_files_.begin(), ska_files_.end());
    } catch (...) {
        // Ignore filesystem errors during scan
    }
}

void Workspace::refresh() { scan(); }

std::string Workspace::uri_to_path(const std::string &uri) {
    const std::string prefix = "file://";
    if (uri.substr(0, prefix.size()) == prefix) {
        std::string path = uri.substr(prefix.size());
        // Decode percent-encoded characters (basic: space)
        std::string decoded;
        for (size_t i = 0; i < path.size(); ++i) {
            if (path[i] == '%' && i + 2 < path.size()) {
                int hex = std::stoi(path.substr(i + 1, 2), nullptr, 16);
                decoded += static_cast<char>(hex);
                i += 2;
            } else {
                decoded += path[i];
            }
        }
        return decoded;
    }
    return uri;
}

std::string Workspace::path_to_uri(const std::string &path) {
    return "file://" + path;
}

} // namespace SkaldLsp
