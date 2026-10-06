// LLM Usage Level: Assistant

#include "../include/skald_emit.h"
#include <cmath>
#include <cstdio>

namespace Skald::Emit {

namespace {

void indent(std::string &out, int depth) {
  out.append(static_cast<size_t>(depth) * 2, ' ');
}

void write_string(std::string &out, const std::string &s) {
  out += '"';
  for (unsigned char c : s) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    case '\b':
      out += "\\b";
      break;
    case '\f':
      out += "\\f";
      break;
    default:
      if (c < 0x20) {
        char buf[8];
        std::snprintf(buf, sizeof buf, "\\u%04x", c);
        out += buf;
      } else {
        out += static_cast<char>(c);
      }
    }
  }
  out += '"';
}

void write_float(std::string &out, double d) {
  if (!std::isfinite(d)) {
    out += "null";
    return;
  }
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.9g", d);
  std::string s = buf;
  if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) {
    s += ".0";
  }
  out += s;
}

void write(std::string &out, const Value &value, int depth) {
  std::visit(
      [&](const auto &x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          out += "null";
        } else if constexpr (std::is_same_v<T, bool>) {
          out += x ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int64_t>) {
          out += std::to_string(x);
        } else if constexpr (std::is_same_v<T, double>) {
          write_float(out, x);
        } else if constexpr (std::is_same_v<T, std::string>) {
          write_string(out, x);
        } else if constexpr (std::is_same_v<T, Array>) {
          if (x.empty()) {
            out += "[]";
            return;
          }
          out += "[\n";
          for (size_t i = 0; i < x.size(); i++) {
            indent(out, depth + 1);
            write(out, x[i], depth + 1);
            out += i + 1 < x.size() ? ",\n" : "\n";
          }
          indent(out, depth);
          out += ']';
        } else {
          if (x.empty()) {
            out += "{}";
            return;
          }
          out += "{\n";
          for (size_t i = 0; i < x.size(); i++) {
            indent(out, depth + 1);
            write_string(out, x[i].first);
            out += ": ";
            write(out, x[i].second, depth + 1);
            out += i + 1 < x.size() ? ",\n" : "\n";
          }
          indent(out, depth);
          out += '}';
        }
      },
      value.v);
}

} // namespace

std::string print_json(const Value &value) {
  std::string out;
  write(out, value, 0);
  out += '\n';
  return out;
}

} // namespace Skald::Emit
