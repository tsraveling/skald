#include "skalder_compile.h"
#include "skald.h"
#include "skald_emit.h"
#include "skalder_common.h"
#include "skalder_fs.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>

namespace fs = std::filesystem;
using namespace Skald;

namespace {

std::string slashes(fs::path p) {
  std::string s = p.lexically_normal().generic_string();
  return s;
}

bool write_file(const fs::path &path, const std::string &text) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    return false;
  }
  f << text;
  return static_cast<bool>(f);
}

struct ParsedFile {
  std::string rel;
  Module module;
};

/** Visits every GoModule in a module, in source order. */
template <typename F> void each_go(const Module &m, F &&f) {
  auto visit_member = [&](const Member &mem) {
    if (auto *go = mem.get_go_module()) {
      f(*go);
    }
  };
  auto visit_bm = [&](const BlockMember &bm) {
    if (auto *mem = std::get_if<Member>(&bm)) {
      visit_member(*mem);
    } else {
      for (auto &c : std::get<ChoiceGroup>(bm).choices) {
        for (auto &mem : c.members) {
          visit_member(mem);
        }
      }
    }
  };
  for (auto &b : m.blocks) {
    for (auto &mbm : b.members) {
      if (auto *bm = std::get_if<BlockMember>(&mbm)) {
        visit_bm(*bm);
      } else {
        for (auto &cb : std::get<ConditionalChain>(mbm).cond_blocks) {
          for (auto &bm : cb.members) {
            visit_bm(bm);
          }
        }
      }
    }
  }
}

std::string ext_for(const std::string &mode) {
  return mode == "lua" ? ".lua" : ".json";
}

std::string render(const Emit::Value &tree, const std::string &mode,
                   const std::string &header_path) {
  return mode == "lua" ? Emit::print_lua(tree, header_path)
                       : Emit::print_json(tree);
}

int compile_project(const SkalderArgs &args) {
  fs::path project_dir =
      args.path.empty() ? fs::current_path() : fs::path(args.path);
  std::error_code ec;
  if (!fs::is_directory(project_dir, ec)) {
    std::cerr << "error: " << project_dir.string() << " is not a directory\n";
    return 2;
  }

  // Exactly one .codex directly in project_dir.
  std::vector<fs::path> codices;
  for (auto &entry : fs::directory_iterator(project_dir, ec)) {
    if (entry.is_regular_file(ec) && entry.path().extension() == ".codex") {
      codices.push_back(entry.path());
    }
  }
  if (codices.size() != 1) {
    std::cerr << "error: expected exactly one .codex in "
              << project_dir.string() << ", found " << codices.size() << "\n";
    return 2;
  }
  const fs::path codex_path = codices.front();

  auto codex_text = default_source_reader(codex_path.string());
  if (!codex_text) {
    std::cerr << "error: cannot read " << codex_path.string() << "\n";
    return 2;
  }
  auto parsed_codex = parse_codex(*codex_text, codex_path.string());
  std::vector<ParseError> diagnostics = parsed_codex.result.exceptions;
  bool failed = !parsed_codex.result.ok;

  FileManager files;
  std::vector<std::string> module_paths = files.find_modules(project_dir);
  for (auto &p : module_paths) {
    p = slashes(p);
  }

  std::vector<ParsedFile> parsed;
  for (auto &rel : module_paths) {
    auto text = default_source_reader((project_dir / rel).string());
    if (!text) {
      std::cerr << "error: cannot read " << rel << "\n";
      return 2;
    }
    auto pm = parse_module(*text, rel, &parsed_codex.codex);
    for (auto &d : pm.result.exceptions) {
      if (d.pos.source.empty()) {
        d.pos.source = rel;
      }
      diagnostics.push_back(d);
    }
    failed = failed || !pm.result.ok;
    parsed.push_back(ParsedFile{rel, std::move(pm.module)});
  }

  // Cross-module: every GO target exists, and its start tag if given.
  std::unordered_map<std::string, const Module *> by_path;
  for (auto &pf : parsed) {
    by_path[pf.rel] = &pf.module;
  }
  for (auto &pf : parsed) {
    each_go(pf.module, [&](const GoModule &go) {
      std::string target = slashes(go.module_path);
      auto it = by_path.find(target);
      auto at = [&](std::string msg) {
        diagnostics.push_back(
            ParseError{.pos = ParsePosition{go.line_number, 1, pf.rel},
                       .msg = std::move(msg),
                       .severity = ParseError::ERROR});
        failed = true;
      };
      if (it == by_path.end()) {
        at("GO target not found in project: " + go.module_path);
        return;
      }
      if (!go.start_in_tag.empty() &&
          it->second->block_lookup.find(go.start_in_tag) ==
              it->second->block_lookup.end()) {
        at("GO start tag '" + go.start_in_tag + "' not found in " + target);
      }
    });
  }

  skalder::print_diagnostics(diagnostics);
  if (failed) {
    return 1;
  }

  const std::string mode = *args.compile;
  fs::path outdir = args.out ? fs::path(*args.out) : project_dir / mode;

  // Never wipe the sources: outdir must not contain the codex.
  fs::path out_abs = fs::weakly_canonical(fs::absolute(outdir, ec), ec);
  fs::path codex_abs = fs::weakly_canonical(fs::absolute(codex_path, ec), ec);
  if (fs::relative(codex_abs, out_abs, ec).string().rfind("..", 0) != 0) {
    std::cerr << "error: output dir " << outdir.string()
              << " contains the project sources\n";
    return 2;
  }
  if (args.clean) {
    fs::remove_all(outdir, ec);
    if (ec) {
      std::cerr << "error: could not clean " << outdir.string() << ": "
                << ec.message() << "\n";
      return 2;
    }
  }

  auto shown = [&](const fs::path &p) {
    auto rel = fs::relative(p, project_dir, ec);
    return (ec || rel.empty() || rel.string().rfind("..", 0) == 0) ? slashes(p)
                                                                    : slashes(rel);
  };

  auto codex_tree = Emit::build_codex_tree(parsed_codex.codex, module_paths);
  fs::path codex_out = outdir / ("_codex" + ext_for(mode));
  if (!write_file(codex_out,
                  render(codex_tree, mode, parsed_codex.codex.filename))) {
    std::cerr << "error: cannot write " << codex_out.string() << "\n";
    return 2;
  }
  std::cout << shown(codex_path) << " -> " << shown(codex_out) << "\n";

  for (auto &pf : parsed) {
    auto tree = Emit::build_module_tree(pf.module, pf.rel);
    fs::path out = outdir / fs::path(pf.rel).replace_extension(ext_for(mode));
    if (!write_file(out, render(tree, mode, pf.rel))) {
      std::cerr << "error: cannot write " << out.string() << "\n";
      return 2;
    }
    std::cout << pf.rel << " -> " << shown(out) << " ("
              << pf.module.blocks.size() << " blocks)\n";
  }
  return 0;
}

int compile_single(const SkalderArgs &args) {
  auto text = default_source_reader(args.path);
  if (!text) {
    std::cerr << "error: cannot read " << args.path << "\n";
    return 2;
  }
  auto pm = parse_module(*text, args.path, nullptr);
  for (auto &d : pm.result.exceptions) {
    if (d.pos.source.empty()) {
      d.pos.source = args.path;
    }
  }
  skalder::print_diagnostics(pm.result.exceptions);
  if (!pm.result.ok) {
    return 1;
  }
  const std::string mode = *args.compile;
  fs::path out = args.out ? fs::path(*args.out)
                          : fs::path(fs::path(args.path).stem().string() +
                                     ext_for(mode));
  auto tree = Emit::build_module_tree(pm.module, args.path);
  if (!write_file(out, render(tree, mode, args.path))) {
    std::cerr << "error: cannot write " << out.string() << "\n";
    return 2;
  }
  std::cout << args.path << " -> " << slashes(out) << " ("
            << pm.module.blocks.size() << " blocks)\n";
  return 0;
}

} // namespace

int run_compile(const SkalderArgs &args) {
  bool single = fs::path(args.path).extension() == ".ska";
  return single ? compile_single(args) : compile_project(args);
}
