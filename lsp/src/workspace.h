#pragma once

#include <string>
#include <vector>

namespace SkaldLsp {

class Workspace {
public:
    void set_root(const std::string &root_uri);
    void scan();

    // Get all .ska file paths relative to workspace root
    const std::vector<std::string> &ska_files() const { return ska_files_; }

    // Re-scan on file change notifications
    void refresh();

    // Convert between file URI and filesystem path
    static std::string uri_to_path(const std::string &uri);
    static std::string path_to_uri(const std::string &path);

    const std::string &root_path() const { return root_path_; }

private:
    std::string root_uri_;
    std::string root_path_;
    std::vector<std::string> ska_files_; // Relative paths
};

} // namespace SkaldLsp
