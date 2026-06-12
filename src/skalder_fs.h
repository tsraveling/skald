/** Filesystem helpers for Skalder (codex discovery, module listing). Only
 *  used by the skalder binary -- never consumed in library mode. */
#pragma once

#include "debug.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include <sys/stat.h>
#include <variant>
#include <vector>

class FileManager {
public:
  struct NoOp {};
  struct FileError {
    std::string msg;
  };

  /** Converts a contextual (cwd-relative) path like "../test.ska" into a
   *  project path relative to the project root, e.g. "texts/test.ska" --
   *  the same form GO paths and find_modules() results use. Returns "" if
   *  either path can't be resolved or the file lives outside the project. */
  std::string loc_to_proj(const std::string &project_path,
                          const std::string &loc_path) {
    namespace fs = std::filesystem;
    std::error_code ec;

    fs::path abs = fs::weakly_canonical(fs::absolute(loc_path, ec), ec);
    if (ec) {
      return "";
    }
    fs::path root = fs::weakly_canonical(fs::absolute(project_path, ec), ec);
    if (ec) {
      return "";
    }

    fs::path rel = fs::relative(abs, root, ec);
    if (ec || rel.empty()) {
      return "";
    }
    // A path that escapes the root ("../outside.ska") isn't in the project.
    if (rel.begin()->string() == "..") {
      return "";
    }
    return rel.string();
  }

  /** Walks the filetree and finds all .ska files in or below the given
   *  project root. Returned paths are relative to that root, sorted. Hidden
   *  directories (".git" etc.) are skipped. */
  std::vector<std::string> find_modules(const std::string &project_path) {
    namespace fs = std::filesystem;
    std::vector<std::string> modules;
    fs::path root = project_path;

    std::error_code ec;
    auto it = fs::recursive_directory_iterator(
        root, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
      return modules;
    }
    for (auto end = fs::end(it); it != end; it.increment(ec)) {
      if (ec) {
        break;
      }
      const auto &entry = *it;
      std::string name = entry.path().filename().string();
      if (entry.is_directory(ec) && !name.empty() && name[0] == '.') {
        it.disable_recursion_pending();
        continue;
      }
      if (entry.is_regular_file(ec) && entry.path().extension() == ".ska") {
        modules.push_back(fs::relative(entry.path(), root, ec).string());
      }
    }
    std::sort(modules.begin(), modules.end());
    return modules;
  }

  /** Checks start_path's directory, then every parent, until it finds one
   *  with a .codex file, and returns the full path to that codex file.
   *  Returns FileError if a directory contains more than one .codex file,
   *  and NoOp if we reach the filesystem root (or cross a mount boundary)
   *  without finding one. */
  std::variant<std::string, FileError, NoOp>
  find_project_root(std::string start_path) {
    namespace fs = std::filesystem;
    std::error_code ec;

    fs::path dir = fs::canonical(start_path, ec);
    if (ec) {
      return FileError{"Could not resolve path: " + start_path};
    }
    if (!fs::is_directory(dir)) {
      dir = dir.parent_path();
    }

    // Mount boundary guard: don't walk up across filesystems (same rule git
    // uses -- a codex above a mount point is almost never the intended one).
    auto device_of = [](const fs::path &p) -> dev_t {
      struct stat st;
      return ::stat(p.c_str(), &st) == 0 ? st.st_dev : 0;
    };
    const dev_t dev = device_of(dir);

    while (true) {
      std::string found;
      int count = 0;
      for (fs::directory_iterator
               dit(dir, fs::directory_options::skip_permission_denied, ec),
           dend;
           !ec && dit != dend; dit.increment(ec)) {
        if (dit->is_regular_file(ec) && dit->path().extension() == ".codex") {
          found = dit->path().string();
          count++;
        }
      }
      if (count > 1) {
        return FileError{"Multiple .codex files found in " + dir.string()};
      }
      if (count == 1) {
        return found;
      }

      fs::path parent = dir.parent_path();
      if (parent == dir || device_of(parent) != dev) {
        return NoOp{};
      }
      dir = parent;
    }
  }
};
