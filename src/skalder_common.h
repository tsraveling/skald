/** Shared helpers for skalder modes: diagnostics printing and codex-aware
 *  engine loading. */
#pragma once

#include "skald.h"
#include "skalder_fs.h"
#include <iostream>
#include <string>
#include <vector>

namespace skalder {

/** gcc-style `path:line:col: severity: message` lines. */
inline void print_diagnostics(const std::vector<Skald::ParseError> &errs,
                              std::ostream &os = std::cerr) {
  for (auto &err : errs) {
    if (!err.pos.source.empty()) {
      os << err.pos.source << ":" << err.pos.line << ":" << err.pos.column
         << ": ";
    }
    os << (err.severity == Skald::ParseError::ERROR ? "error: " : "warning: ")
       << err.msg << "\n";
  }
}

struct EngineLoad {
  /** 0 ok, 1 parse error, 2 file error. */
  int exit_code = 0;
  /** Codex-relative module path handed to Engine::load. */
  std::string module_path;
  /** Informational lines (codex found, orphan mode). */
  std::vector<std::string> notes;
};

/** Finds the codex above `path` (if any), sets up the engine with it, and
 *  loads the module. Diagnostics go to `err`. */
inline EngineLoad load_engine(Skald::Engine &engine, const std::string &path,
                              std::ostream &err = std::cerr) {
  EngineLoad out;
  FileManager files;
  out.module_path = path;

  auto root = files.find_project_root(path);
  if (auto *codex_path = std::get_if<std::string>(&root)) {
    out.notes.push_back("CODEX: " + *codex_path);
    auto res = engine.setup(*codex_path);
    print_diagnostics(res.exceptions, err);
    if (!res.ok) {
      out.exit_code = 1;
      return out;
    }
    auto project_root = engine.get_project_root();
    out.module_path = files.loc_to_proj(*project_root, path);
    if (out.module_path.empty()) {
      err << "error: " << path << " is not inside the codex project\n";
      out.exit_code = 2;
      return out;
    }
  } else if (auto *fe = std::get_if<FileManager::FileError>(&root)) {
    err << "error: " << fe->msg << "\n";
    out.exit_code = 2;
    return out;
  } else {
    out.notes.push_back("Not in a Skald codex; loading as orphan (no globals "
                        "or methods available).");
  }

  auto res = engine.load(out.module_path);
  print_diagnostics(res.exceptions, err);
  if (!res.ok) {
    out.exit_code = 1;
  }
  return out;
}

/** Applies --testbed after load. Returns false (and prints) on failure. */
inline bool apply_testbed_arg(Skald::Engine &engine,
                              const std::optional<std::string> &name,
                              std::ostream &err = std::cerr) {
  if (!name) {
    return true;
  }
  if (auto e = engine.apply_testbed(*name)) {
    err << "error: " << e->message << "\n";
    return false;
  }
  return true;
}

/** start() or start_at(--start). */
inline Skald::Response start_engine(Skald::Engine &engine,
                                    const std::optional<std::string> &tag) {
  return tag ? engine.start_at(*tag) : engine.start();
}

} // namespace skalder
