#include "project_index.h"
#include "lsp_actions.h"
#include "lsp_parse_state.h"
#include "skald_grammar.h"
#include "skalder_fs.h"
#include <deque>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <tao/pegtl.hpp>

namespace fs = std::filesystem;

namespace SkaldLsp {

static std::string read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::string normalize_rel(const std::string &p) {
    return fs::path(p).lexically_normal().generic_string();
}

// Recursively collect GO target paths from a block's members.
static void collect_go_from_member(const Skald::Member &m,
                                   std::vector<std::string> &out) {
    if (const auto *go = m.get_go_module())
        out.push_back(normalize_rel(go->module_path));
}

static void collect_go_from_block_member(const Skald::BlockMember &bm,
                                         std::vector<std::string> &out) {
    if (const auto *mem = std::get_if<Skald::Member>(&bm)) {
        collect_go_from_member(*mem, out);
    } else if (const auto *cg = std::get_if<Skald::ChoiceGroup>(&bm)) {
        for (const auto &choice : cg->choices)
            for (const auto &cm : choice.members)
                collect_go_from_member(cm, out);
    }
}

static void collect_go(const Skald::Block &block,
                       std::vector<std::string> &out) {
    for (const auto &mbm : block.members) {
        if (const auto *bm = std::get_if<Skald::BlockMember>(&mbm)) {
            collect_go_from_block_member(*bm, out);
        } else if (const auto *chain =
                       std::get_if<Skald::ConditionalChain>(&mbm)) {
            for (const auto &cb : chain->cond_blocks)
                for (const auto &inner : cb.members)
                    collect_go_from_block_member(inner, out);
        }
    }
}

// Find the 0-based line/col of a declaration named `name` within [from,to)
// lines of `text` (a leading-identifier match). Returns false if not found.
static bool find_decl_position(const std::string &text, const std::string &name,
                               int &line_out, int &col_out, int &end_out) {
    std::istringstream stream(text);
    std::string line;
    int idx = 0;
    while (std::getline(stream, line)) {
        size_t i = 0;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
            ++i;
        size_t start = i;
        while (i < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[i])) ||
                line[i] == '_'))
            ++i;
        if (line.substr(start, i - start) == name) {
            line_out = idx;
            col_out = static_cast<int>(start);
            end_out = static_cast<int>(i);
            return true;
        }
        ++idx;
    }
    return false;
}

void ProjectIndex::build(const Skald::Codex *codex) {
    modules_.clear();
    globals_.clear();
    if (!codex)
        return;

    const std::string root = codex->path;

    // Codex globals: locate each in the codex file text for jump targets.
    std::string codex_full = (fs::path(root) / codex->filename).string();
    std::string codex_uri = "file://" + codex_full;
    std::string codex_text = read_file(codex_full);
    for (const auto &g : codex->global_vars) {
        VarDef def;
        def.uri = codex_uri;
        def.type = g.var.type;
        find_decl_position(codex_text, g.var.name, def.line, def.col,
                           def.end_col);
        globals_[g.var.name] = def;
    }

    FileManager fm;
    auto rels = fm.find_modules(root);
    for (const auto &rel : rels) {
        std::string full = (fs::path(root) / rel).string();
        std::string text = read_file(full);
        if (text.empty())
            continue;
        if (text.back() != '\n')
            text += '\n';

        LspParseState state(rel, codex);
        try {
            tao::pegtl::memory_input input(text, rel);
            tao::pegtl::parse<Skald::grammar, lsp_action>(input, state);
        } catch (...) {
            // keep partial results
        }

        ModuleEntry entry;
        entry.uri = "file://" + full;

        // Module vars: names + types from the parsed module, ranges from the
        // recorded definition symbols.
        for (const auto &mv : state.module.module_vars) {
            VarDef def;
            def.uri = entry.uri;
            def.type = mv.var.type;
            for (const auto &sym : state.symbols) {
                if (sym.kind == SymbolKind::Variable && sym.is_definition &&
                    sym.name == mv.var.name) {
                    def.line = sym.range.line;
                    def.col = sym.range.col;
                    def.end_col = sym.range.end_col;
                    break;
                }
            }
            entry.vars[mv.var.name] = def;
        }

        for (const auto &block : state.module.blocks)
            collect_go(block, entry.go_targets);

        modules_[normalize_rel(rel)] = std::move(entry);
    }
}

std::vector<std::string>
ProjectIndex::predecessors(const std::string &rel_path) const {
    std::string target = normalize_rel(rel_path);
    std::vector<std::string> order;
    std::set<std::string> seen{target};
    std::deque<std::string> frontier{target};
    while (!frontier.empty()) {
        std::string cur = frontier.front();
        frontier.pop_front();
        // Any module whose GO targets include `cur` is a direct predecessor.
        for (const auto &[rel, entry] : modules_) {
            if (seen.count(rel))
                continue;
            for (const auto &t : entry.go_targets) {
                if (t == cur) {
                    seen.insert(rel);
                    order.push_back(rel);
                    frontier.push_back(rel);
                    break;
                }
            }
        }
    }
    return order;
}

bool ProjectIndex::has_thread_var(const std::string &name,
                                  const std::string &rel_path) const {
    for (const auto &pred : predecessors(rel_path)) {
        auto it = modules_.find(pred);
        if (it != modules_.end() && it->second.vars.count(name))
            return true;
    }
    return false;
}

std::optional<VarDef>
ProjectIndex::resolve_external_var(const std::string &name,
                                   const std::string &rel_path) const {
    // 1.8 resolution order: global first, then thread (predecessor) vars.
    auto g = globals_.find(name);
    if (g != globals_.end())
        return g->second;
    for (const auto &pred : predecessors(rel_path)) {
        auto it = modules_.find(pred);
        if (it == modules_.end())
            continue;
        auto v = it->second.vars.find(name);
        if (v != it->second.vars.end())
            return v->second;
    }
    return std::nullopt;
}

} // namespace SkaldLsp
