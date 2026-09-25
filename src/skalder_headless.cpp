#include "skalder_headless.h"
#include "skald.h"
#include "skald_emit.h"
#include "skalder_common.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace Skald;

namespace {

struct Token {
  enum Kind { ACT, QUERY } kind;
  int index = 0;
  std::optional<SimpleRValue> value;
};

/** Parses a Skald simple literal: true, false, int, float, "string". */
std::optional<SimpleRValue> parse_literal(const std::string &s) {
  if (s == "true")
    return SimpleRValue{true};
  if (s == "false")
    return SimpleRValue{false};
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
    std::string out;
    for (size_t i = 1; i + 1 < s.size(); i++) {
      if (s[i] == '\\' && i + 2 < s.size()) {
        char n = s[++i];
        out += n == 'n' ? '\n' : n;
      } else {
        out += s[i];
      }
    }
    return SimpleRValue{out};
  }
  char *end = nullptr;
  long l = std::strtol(s.c_str(), &end, 10);
  if (end != s.c_str() && *end == '\0') {
    return SimpleRValue{static_cast<int>(l)};
  }
  float f = std::strtof(s.c_str(), &end);
  if (end != s.c_str() && *end == '\0') {
    return SimpleRValue{f};
  }
  return std::nullopt;
}

std::optional<std::vector<Token>> read_script(const std::string &path,
                                              std::string &error) {
  std::ifstream f(path);
  if (!f) {
    error = "cannot read script " + path;
    return std::nullopt;
  }
  std::vector<Token> tokens;
  std::string line;
  size_t n = 0;
  while (std::getline(f, line)) {
    n++;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
      line.pop_back();
    if (line.empty() || line.rfind("---", 0) == 0)
      continue;
    if (line[0] == 'a') {
      Token t{Token::ACT};
      if (line.size() > 1) {
        char *end = nullptr;
        t.index = static_cast<int>(std::strtol(line.c_str() + 1, &end, 10));
        if (*end != '\0') {
          error = path + ":" + std::to_string(n) + ": bad token " + line;
          return std::nullopt;
        }
      }
      tokens.push_back(t);
    } else if (line[0] == 'q') {
      Token t{Token::QUERY};
      if (line.size() > 1) {
        t.value = parse_literal(line.substr(1));
        if (!t.value) {
          error = path + ":" + std::to_string(n) + ": bad literal " + line;
          return std::nullopt;
        }
      }
      tokens.push_back(t);
    } else {
      error = path + ":" + std::to_string(n) + ": bad token " + line;
      return std::nullopt;
    }
  }
  return tokens;
}

std::string stitch(const std::vector<Chunk> &chunks) {
  std::string out;
  for (auto &c : chunks)
    out += c.text;
  return out;
}

std::string args_of(const MethodCall &call) {
  std::string out;
  for (size_t i = 0; i < call.args.size(); i++) {
    out += (i ? "," : "") + Emit::lua_literal(call.args[i]);
  }
  return out;
}

const char *op_name(Mutation::Type t) {
  switch (t) {
  case Mutation::EQUATE:
    return "set";
  case Mutation::ADD:
    return "add";
  case Mutation::SUBTRACT:
    return "sub";
  case Mutation::SWITCH:
    return "toggle";
  }
  return "set";
}

void print_response(const Response &r) {
  std::visit(
      [](const auto &x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, Content>) {
          std::cout << "C|" << x.attribution << "|" << stitch(x.text) << "\n";
        } else if constexpr (std::is_same_v<T, OptionGroup>) {
          for (size_t i = 0; i < x.options.size(); i++) {
            std::cout << "O|" << i << "|" << (x.options[i].is_available ? 1 : 0)
                      << "|" << stitch(x.options[i].text) << "\n";
          }
        } else if constexpr (std::is_same_v<T, MethodCallGet>) {
          std::cout << "Q|" << x.call.method << "|" << args_of(x.call) << "\n";
        } else if constexpr (std::is_same_v<T, MethodCallPost>) {
          std::cout << "P|" << x.call.method << "|" << args_of(x.call) << "\n";
        } else if constexpr (std::is_same_v<T, Notification>) {
          std::cout << "N|" << x.var_name << "|" << op_name(x.mut_type) << "|"
                    << scope_to_str(x.scope) << "\n";
        } else if constexpr (std::is_same_v<T, Exit>) {
          std::cout << "X|" << (x.argument ? Emit::lua_literal(*x.argument) : "")
                    << "\n";
        } else if constexpr (std::is_same_v<T, GoModule>) {
          std::cout << "G|" << x.module_path << "|" << x.start_in_tag << "\n";
        } else if constexpr (std::is_same_v<T, End>) {
          std::cout << "E|" << x.reason << "\n";
        } else if constexpr (std::is_same_v<T, Error>) {
          std::cout << "!|" << x.line_number << "|" << x.message << "\n";
        }
      },
      r);
}

int fail(const char *msg) {
  std::cout << "!|0|" << msg << "\n";
  return 1;
}

} // namespace

int run_headless(const SkalderArgs &args) {
  std::string err;
  auto script = read_script(*args.script, err);
  if (!script) {
    std::cerr << "error: " << err << "\n";
    return 2;
  }

  Engine engine;
  auto load = skalder::load_engine(engine, args.path);
  if (load.exit_code) {
    return load.exit_code;
  }
  if (!skalder::apply_testbed_arg(engine, args.testbed)) {
    return 2;
  }
  Response response = skalder::start_engine(engine, args.start);
  if (args.start && std::holds_alternative<Error>(response) &&
      std::get<Error>(response).code == ERROR_MODULE_TAG_NOT_FOUND) {
    std::cerr << "error: " << std::get<Error>(response).message << "\n";
    return 2;
  }

  size_t cursor = 0;
  auto next_token = [&]() -> const Token * {
    return cursor < script->size() ? &(*script)[cursor++] : nullptr;
  };

  while (true) {
    print_response(response);
    if (std::holds_alternative<Exit>(response) ||
        std::holds_alternative<End>(response)) {
      return 0;
    }
    if (std::holds_alternative<Error>(response)) {
      return 1;
    }
    if (std::holds_alternative<MethodCallPost>(response) ||
        std::holds_alternative<Notification>(response) ||
        std::holds_alternative<GoModule>(response)) {
      response = engine.act(0);
      continue;
    }
    const Token *tok = next_token();
    if (!tok) {
      return fail("script exhausted");
    }
    if (std::holds_alternative<Content>(response) && tok->kind == Token::ACT) {
      response = engine.act(0);
    } else if (std::holds_alternative<OptionGroup>(response) &&
               tok->kind == Token::ACT) {
      response = engine.act(tok->index);
    } else if (std::holds_alternative<MethodCallGet>(response) &&
               tok->kind == Token::QUERY) {
      response = engine.answer(QueryAnswer{tok->value});
    } else {
      return fail("expected a<i> or q<value>");
    }
  }
}
