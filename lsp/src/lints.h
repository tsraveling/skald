#pragma once

#include "lsp_types.h"
#include <sstream>
#include <string>
#include <vector>

namespace SkaldLsp {

// Returns the leading-whitespace width of a line.
inline size_t indent_width(const std::string &line) {
  size_t i = 0;
  while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
    ++i;
  return i;
}

// Does `body` (a line with leading whitespace already stripped) begin with an
// operation — optionally behind an inline `(? … )` conditional? On success,
// `op_at` is the offset of the operation token within `body`.
inline bool looks_like_operation(const std::string &body, size_t &op_at) {
  size_t k = 0;
  if (body.rfind("(?", 0) == 0) {
    int depth = 0;
    size_t i = 0;
    for (; i < body.size(); ++i) {
      if (body[i] == '(')
        ++depth;
      else if (body[i] == ')') {
        if (--depth == 0) {
          ++i;
          break;
        }
      }
    }
    k = i;
    while (k < body.size() && (body[k] == ' ' || body[k] == '\t'))
      ++k;
  }
  op_at = k;
  std::string op = body.substr(k);
  return op.rfind("->", 0) == 0 || op.rfind("~", 0) == 0 ||
         op.rfind(":", 0) == 0 || op.rfind("GO ", 0) == 0 ||
         op.rfind("GO\t", 0) == 0 || op == "GO" || op.rfind("EXIT", 0) == 0;
}

// Find a two-hyphen `--` comment marker in `line` starting at `from`, skipping
// double-quoted string contents and ignoring valid `---` markers. Returns the
// column of the `--`, or -1 if none. Only counts `--` preceded by whitespace
// (i.e. shaped like a comment, not part of `->`).
inline int find_two_hyphen_comment(const std::string &line, size_t from) {
  bool in_string = false;
  for (size_t i = from; i + 1 < line.size(); ++i) {
    char c = line[i];
    if (in_string) {
      if (c == '\\') {
        ++i;
        continue;
      }
      if (c == '"')
        in_string = false;
      continue;
    }
    if (c == '"') {
      in_string = true;
      continue;
    }
    if (c == '-' && line[i + 1] == '-') {
      if (i + 2 < line.size() && line[i + 2] == '-') {
        i += 2; // a valid `---`, skip it
        continue;
      }
      if (i > 0 && (line[i - 1] == ' ' || line[i - 1] == '\t'))
        return static_cast<int>(i);
    }
  }
  return -1;
}

// Lint for indented content that is not part of a choice. In 0.3, an indented
// line (beat OR operation) is only valid as a choice member — indented under a
// `>` choice. Indentation anywhere else (e.g. tucked under a plain beat, or
// inside an @if) cannot be consumed by the grammar and halts parsing of
// everything below it.
//
// Excluded from the check: `---` comments (legal at any indent), and the
// indented declaration / set lines inside top-matter @let / @testbed blocks.
//
// We track an open-choice state: a `>` line opens it; a non-indented line that
// isn't a choice (block-level content or a new block tag) closes it. Indented
// and blank lines leave it unchanged (a choice's members may span blank gaps).
inline std::vector<LspTypes::Diagnostic>
lint_indentation(const std::string &text) {
  std::vector<LspTypes::Diagnostic> out;
  std::istringstream stream(text);
  std::string line;
  int idx = 0;
  bool choice_open = false;
  bool in_block = false;
  enum { TOP_NONE, TOP_LET, TOP_TESTBED } top = TOP_NONE;

  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    size_t ind = indent_width(line);
    std::string body = line.substr(ind);

    if (body.empty()) { // blank: neutral
      ++idx;
      continue;
    }

    // Inside a top-matter @let / @testbed block, indented declaration and
    // set lines are expected; skip until @end.
    if (top != TOP_NONE) {
      if (body.rfind("@end", 0) == 0)
        top = TOP_NONE;
      ++idx;
      continue;
    }

    if (ind == 0) {
      if (body.rfind("@let", 0) == 0) {
        top = TOP_LET;
      } else if (body.rfind("@testbed", 0) == 0) {
        top = TOP_TESTBED;
      } else if (body[0] == '#') {
        in_block = true;
        choice_open = false;
      } else {
        // Block-level content: a choice keeps the group open, anything
        // else closes it.
        choice_open = (body[0] == '>');
      }
    } else { // indented line
      if (body.rfind("---", 0) == 0) {
        ++idx; // comment, legal anywhere
        continue;
      }
      if (body[0] == '>') {
        choice_open = true; // an (indented) choice line
      } else if (in_block && !choice_open) {
        out.push_back(
            {{{idx, static_cast<int>(ind)},
              {idx, static_cast<int>(line.size())}},
             LspTypes::DiagnosticSeverity::Warning,
             "Indented line is not under a '>' choice. Indented beats "
             "and operations are only valid as choice members; this "
             "halts parsing here.",
             "skald"});
      }
    }
    ++idx;
  }
  return out;
}

// Lint for deprecated two-hyphen `--` comments (0.3 uses `---`). These don't
// just read wrong — on an operation line they break `functional_eol` and halt
// parsing; as a standalone line in a block they get parsed as beat text. We
// only inspect standalone comment lines and operation lines, so prose em-dashes
// in beats/choices and `--` inside string args are never flagged.
inline std::vector<LspTypes::Diagnostic>
lint_deprecated_comments(const std::string &text) {
  std::vector<LspTypes::Diagnostic> out;
  std::istringstream stream(text);
  std::string line;
  int idx = 0;

  auto add = [&](int li, int col, int end, const std::string &msg) {
    LspTypes::Diagnostic diag;
    diag.range.start = {li, col};
    diag.range.end = {li, end};
    diag.severity = LspTypes::DiagnosticSeverity::Warning;
    diag.message = msg;
    out.push_back(diag);
  };

  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    size_t ind = indent_width(line);
    std::string body = line.substr(ind);

    // Valid `---` comment line: fine.
    if (body.rfind("---", 0) == 0) {
      ++idx;
      continue;
    }
    // Standalone old `--` comment line.
    if (body.size() >= 2 && body[0] == '-' && body[1] == '-') {
      add(idx, static_cast<int>(ind), static_cast<int>(line.size()),
          "Comment designation requires three hyphens.");
      ++idx;
      continue;
    }
    // Trailing `--` comment on an operation line.
    size_t op_at = 0;
    if (looks_like_operation(body, op_at)) {
      int c = find_two_hyphen_comment(line, ind);
      if (c >= 0)
        add(idx, c, static_cast<int>(line.size()),
            "Comment designation requires three hyphens.");
    }
    ++idx;
  }
  return out;
}

} // namespace SkaldLsp
