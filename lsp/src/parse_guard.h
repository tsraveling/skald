#pragma once

#include <sstream>
#include <string>

namespace SkaldLsp {

// The library's `top_matter` rule is `star<sor<…, not_at<block_prefix>>>`.
// `not_at<block_prefix>` matches the empty string, so the moment top matter
// contains a line the grammar cannot consume — a stray/old-syntax line, an
// unterminated @let/@testbed, or a file with no complete `# ` block yet — the
// star loops forever and the parse never returns. Any code that hands a `.ska`
// to PEGTL (open documents AND the project-wide scan) must gate on this first,
// or a single bad file freezes the whole LSP.
//
// Returns true when parsing is safe: the top-matter region (everything before
// the first top-level `# ` block) is only blank lines, `---` comments, and
// balanced @let/@testbed/@receive blocks. On an unsafe result, `has_stray`
// indicates a concrete offending line and (line,col,len) locate it (0-based).
inline bool top_matter_safe(const std::string &text, bool &has_stray,
                            int &stray_line, int &stray_col, int &stray_len) {
    std::istringstream stream(text);
    std::string line;
    int idx = 0;
    enum { NONE, LET, TESTBED } section = NONE;
    bool saw_block = false;
    has_stray = false;

    auto set_stray = [&](int li, int col, int len) {
        has_stray = true;
        stray_line = li;
        stray_col = col;
        stray_len = len;
    };

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // A top-level block tag line (`#`, `##`, `###` + space) ends top matter.
        if (!line.empty() && line[0] == '#') {
            size_t h = 0;
            while (h < line.size() && line[h] == '#')
                ++h;
            if (h >= 1 && h <= 3 && h < line.size() && line[h] == ' ') {
                saw_block = true;
                break;
            }
            set_stray(idx, 0, static_cast<int>(line.size())); // `#tag` etc.
            return false;
        }

        size_t s = 0;
        while (s < line.size() && (line[s] == ' ' || line[s] == '\t'))
            ++s;
        std::string t = line.substr(s);

        if (t.empty() || t.rfind("---", 0) == 0) {
            ++idx;
            continue;
        }

        if (section == NONE) {
            if (t.rfind("@let", 0) == 0) {
                section = LET;
            } else if (t.rfind("@testbed", 0) == 0) {
                section = TESTBED;
            } else if (t.rfind("@receive", 0) == 0) {
                // single line, stays at NONE
            } else {
                set_stray(idx, static_cast<int>(s),
                          static_cast<int>(line.size() - s));
                return false;
            }
        } else {
            if (t.rfind("@end", 0) == 0)
                section = NONE;
            // otherwise a declaration / testbed-set line inside the block: ok
        }
        ++idx;
    }

    // No block at all, or an unterminated @let/@testbed reaching the first
    // block: both loop in the grammar. Unsafe, but no concrete stray line.
    if (!saw_block || section != NONE)
        return false;
    return true;
}

} // namespace SkaldLsp
