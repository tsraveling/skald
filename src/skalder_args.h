/** Command line parsing for skalder. Header-only; no engine dependency. */
#pragma once

#include <optional>
#include <ostream>
#include <string>

struct SkalderArgs {
  /** Module file, or project dir in project compile mode. */
  std::string path;
  /** "lua" or "json" when --compile given. */
  std::optional<std::string> compile;
  std::optional<std::string> out;
  bool clean = false;
  std::optional<std::string> script;
  std::optional<std::string> testbed;
  std::optional<std::string> start;
  bool help = false;
  /** Non-empty on a usage error. */
  std::string error;

  static SkalderArgs parse(int argc, char *argv[]) {
    SkalderArgs a;
    auto need_value = [&](int &i, const char *flag) -> const char * {
      if (i + 1 >= argc) {
        a.error = std::string(flag) + " requires a value";
        return nullptr;
      }
      return argv[++i];
    };
    for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];
      if (arg == "--help" || arg == "-h") {
        a.help = true;
      } else if (arg == "--compile") {
        auto v = need_value(i, "--compile");
        if (!v)
          break;
        a.compile = v;
        if (*a.compile != "lua" && *a.compile != "json") {
          a.error = "--compile expects lua or json";
          break;
        }
      } else if (arg == "-o" || arg == "--out") {
        auto v = need_value(i, "-o");
        if (!v)
          break;
        a.out = v;
      } else if (arg == "--clean") {
        a.clean = true;
      } else if (arg == "--script") {
        auto v = need_value(i, "--script");
        if (!v)
          break;
        a.script = v;
      } else if (arg == "--testbed") {
        auto v = need_value(i, "--testbed");
        if (!v)
          break;
        a.testbed = v;
      } else if (arg == "--start") {
        auto v = need_value(i, "--start");
        if (!v)
          break;
        a.start = v;
      } else if (!arg.empty() && arg[0] == '-') {
        a.error = "unknown option " + arg;
        break;
      } else if (a.path.empty()) {
        a.path = arg;
      } else {
        a.error = "unexpected argument " + arg;
        break;
      }
    }
    if (a.error.empty() && !a.help) {
      if (a.compile && a.script) {
        a.error = "--compile and --script are exclusive";
      } else if (!a.compile && a.path.empty()) {
        a.error = "a module path is required";
      } else if (a.clean && !a.compile) {
        a.error = "--clean only applies to --compile";
      } else if (a.out && !a.compile) {
        a.error = "-o only applies to --compile";
      } else if (a.compile && (a.testbed || a.start)) {
        a.error = "--testbed and --start do not apply to --compile";
      }
    }
    return a;
  }

  static void print_usage(std::ostream &os) {
    os << "skalder - Skald module tester and compiler\n"
          "\n"
          "usage:\n"
          "  skalder <module.ska> [--testbed NAME] [--start TAG]\n"
          "      Interactive TUI.\n"
          "  skalder <module.ska> --script FILE [--testbed NAME] [--start "
          "TAG]\n"
          "      Headless run; prints a transcript to stdout.\n"
          "  skalder --compile lua|json [project_dir] [-o OUTDIR] [--clean]\n"
          "      Compile every .ska under the project (dir with one .codex).\n"
          "      project_dir defaults to cwd; OUTDIR to <project_dir>/lua or "
          "/json.\n"
          "  skalder <module.ska> --compile lua|json [-o FILE]\n"
          "      Compile one module without a codex.\n"
          "\n"
          "exit codes: 0 ok, 1 parse or validation error, 2 usage or file "
          "error\n";
  }
};
