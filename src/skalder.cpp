#include "debug.h"
#include "skald.h"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <vector>

using namespace ftxui;
using namespace Skald;

namespace {
enum class LogSeverity { DEBUG, INFO, WARN, ERROR };
struct SkalderLog {
  std::string msg;
  LogSeverity severity;
};
std::vector<SkalderLog> log_lines;
constexpr int DEBUG_PANEL_HEIGHT = 12;

void dbg_log(std::string msg, LogSeverity sev = LogSeverity::DEBUG) {
  log_lines.push_back({std::move(msg), sev});
}
} // namespace

enum class NarrativeItemType { NORMAL, SYSTEM, ERROR, INPUT };

struct NarrativeItem {
  std::string attribution;
  std::string content;
  NarrativeItemType type = NarrativeItemType::NORMAL;
};

struct NarrativeOption {
  std::string text;
  bool is_available;
};

class SkaldTester {
public:
  /** List of content; latest is current. */
  std::vector<NarrativeItem> narrative;

  /** The current response we are going to be responding to */
  Response *current_response;

  enum InputType { CONTINUE, CHOICES, TEXT };
  std::string current_prompt = "";
  std::vector<NarrativeOption> current_options;
  InputType expected_input = InputType::CONTINUE;

  // Process a response for consumption
  void process(Response &response) {

    Response &resp = response;

    // First eliminate autos
    for (;;) {
      if (auto *q = std::get_if<Query>(&resp)) {
        if (!q->expects_response) {
          note_system(q->call.dbg_desc());
          dbg_log("Query w/ auto-response: " + q->call.dbg_desc());
          resp = engine.answer(std::nullopt);
          continue;
        }
      }
      break;
    }

    current_response = &resp;
    current_options.clear();

    std::visit(
        [&](const auto &value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, Content>) {
            dbg_log("processing Content");
            narrative.push_back(NarrativeItem{.attribution = value.attribution,
                                              .content = stitch(value.text)});
            expected_input = InputType::CONTINUE;
            current_prompt = "Spacebar to continue ...";
          } else if constexpr (std::is_same_v<T, OptionGroup>) {
            dbg_log("processing OptionGroup with " +
                    std::to_string(value.options.size()) + " options");
            expected_input = InputType::CHOICES;
            for (size_t i = 0; i < value.options.size(); i++) {
              auto &opt = value.options[i];
              current_options.push_back(NarrativeOption{
                  .text = stitch(opt.text), .is_available = opt.is_available});
            }
            current_prompt = "Select an option";
          } else if constexpr (std::is_same_v<T, Query>) {
            dbg_log("Processing Query");
            current_prompt = value.call.dbg_desc();
            expected_input = InputType::TEXT;
          } else if constexpr (std::is_same_v<T, Exit>) {
            dbg_log("Processing Exit");
            current_prompt = "Press spacebar to conclude script.";
            expected_input = InputType::CONTINUE;
          } else if constexpr (std::is_same_v<T, GoModule>) {
            dbg_log("Processing GoModule");
            current_prompt =
                "Press spacebar to continue to " + value.module_path + "!";
            expected_input = InputType::CONTINUE;
          } else if constexpr (std::is_same_v<T, End>) {
            dbg_log("Processing End");
            current_prompt =
                "This is an END response -- we shouldn't be here anymore!";
            expected_input = InputType::CONTINUE;
          } else if constexpr (std::is_same_v<T, Error>) {
            dbg_log(value.message, LogSeverity::ERROR);
            current_prompt = value.message;
            expected_input = InputType::CONTINUE;
          }
        },
        resp);
  }

  /** This will stitch a vector of Skald chunks together into one string */
  static std::string stitch(const std::vector<Chunk> &chunks) {
    std::string ret = "";
    for (auto &chunk : chunks) {
      ret += chunk.text;
    }
    return ret;
  }

  void note_system(std::string val) {
    narrative.push_back(
        NarrativeItem{.content = val, .type = NarrativeItemType::SYSTEM});
  }
  void note_input(std::string val) {
    narrative.push_back(
        NarrativeItem{.content = val, .type = NarrativeItemType::INPUT});
  }
  void note_error(std::string val) {
    narrative.push_back(
        NarrativeItem{.content = val, .type = NarrativeItemType::ERROR});
  }

  // STUB: Add error log here

  // Perform a "continue" (spacebar) action
  Response do_continue(Response &response) {
    return std::visit(
        [&](const auto &value) -> Response {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, Content>) {
            return engine.act(0);
          } else if constexpr (std::is_same_v<T, OptionGroup>) {
            // TODO: Consider erroring out here if we require a choice
            return engine.act(0);
          } else if constexpr (std::is_same_v<T, Exit>) {
            return End{"Module EXIT."};
          } else if constexpr (std::is_same_v<T, Query>) {
            return engine.answer(std::nullopt);
          } else {
            // TODO: Better error handling if this is a problem
            return End("Exiting because tried to continue on an unknown "
                       "Response type.");
          }
        },
        response);
  }

  // Perform a "choice selection" (num keys) action
  Response do_choice(Response &response, int choice) {
    if (choice < current_options.size()) {
      auto chose = current_options[choice];
      note_input(chose.text);
    } else {
      note_error("Chose " + std::to_string(choice) + ", but there are only " +
                 std::to_string(current_options.size()) + " options!");
    }
    return std::visit(
        [&](const auto &value) -> Response {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, OptionGroup>) {
            return engine.act(choice);
          } else {
            // TODO: Better error handling if this is a problem
            return End{"Exited because we tried to process a choice on "
                       "something that wasn't an OptionGroup!"};
          }
        },
        response);
  }

  // Perform a "query answer" (text entry) action
  Response do_input(Response &response, std::string txt) {
    return std::visit(
        [&](const auto &value) -> Response {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, Query>) {
            if (value.expects_response) {
              SimpleRValue parsed = false;
              if (txt == "true") {
                parsed = true;
              } else if (txt == "false") {
                parsed = false;
              } else {
                // Try int first (more restrictive than float)
                char *end;
                long lval = std::strtol(txt.c_str(), &end, 10);
                if (*end == '\0' && end != txt.c_str()) {
                  parsed = static_cast<int>(lval);
                } else {
                  // Try float
                  float fval = std::strtof(txt.c_str(), &end);
                  if (*end == '\0' && end != txt.c_str()) {
                    parsed = fval;
                  } else {
                    parsed = txt;
                  }
                }
              }
              note_system(value.call.dbg_desc() + " <- " + txt);
              return engine.answer(QueryAnswer{parsed});
            }
            note_error("Failed to parse [" + txt +
                       "] into a valid Skald value");
            return Error(
                ERROR_UNKNOWN,
                "Failed to parse [" + txt + "] into a valid Skald value", 0);
          } else {
            note_error("We got an input, but this isn't a query!");
            return Error(ERROR_UNKNOWN,
                         "We got an input, but this isn't a query!", 0);
          }
        },
        response);
  }

  Engine engine;
};

int main(int argc, char *argv[]) {
  dbg_out_on = true;
  dbg_sink = [](const std::string &s) { dbg_log(s); };

  auto screen = ScreenInteractive::Fullscreen();

  // FIXME: Eventually remove this debug option (or flag it out)
  std::string path = (argc < 2) ? "../test/test.ska" : argv[1];

  SkaldTester tester{};
  tester.engine.load(path);
  tester.note_system("STARTING MODULE: " + path);
  dbg_log("STARTING MODULE: " + path);

  Response response;
  response = tester.engine.start();
  tester.process(response);

  std::string input_content;
  std::string exit_reason;
  bool show_logs = false;
  auto input = Input(&input_content, "string, int, float, true, or false");

  auto component = Renderer(input, [&] {
    // Assemble the narrative log
    Elements log_elements;
    for (size_t i = 0; i < tester.narrative.size(); i++) {
      auto nar = tester.narrative[i];
      // bool is_latest = i == tester.narrative.size() - 1;

      // Add a blank line between items
      log_elements.push_back(text(""));
      auto content = paragraph(nar.content);

      switch (nar.type) {
      case NarrativeItemType::SYSTEM:
        content = content | color(Color::DarkSlateGray1);
        log_elements.push_back(content);
        break;
      case NarrativeItemType::ERROR:
        content = content | color(Color::DarkRed);
        log_elements.push_back(content);
        break;
      case NarrativeItemType::INPUT:
        content = content | color(Color::DarkGreen);
        log_elements.push_back(content);
        break;
      case NarrativeItemType::NORMAL:
        // auto col = color(Color::White);
        // is_latest ? color(Color::White) : color(Color::LightSlateGrey);
        auto par = content; // | col;
        if (nar.attribution != "") {
          log_elements.push_back(vbox(text(nar.attribution + ":") |
                                          color(Color::MediumOrchid1) | bold,
                                      hbox(text("  "), par)));
        } else {
          log_elements.push_back(par);
        }
        break;
      }
    }

    // After building log_elements, mark the last one as focused:
    if (!log_elements.empty()) {
      log_elements.back() = log_elements.back() | focus;
    }

    auto log_box = vbox(log_elements) | yframe | flex;
    auto padded_logs = hbox(text("  "), log_box, text("  ")) | flex;

    // SECTION: Prompt
    Element prompt_content;
    switch (tester.expected_input) {
    case SkaldTester::CONTINUE:
      prompt_content = nullptr;
      break;
    case SkaldTester::CHOICES: {
      dbg_log("Setting up a current_options thing");
      Elements choices;
      int i = 1;
      for (auto &opt : tester.current_options) {
        auto line = text(std::to_string(i) + ". " + opt.text);
        if (!opt.is_available) {
          line = line | strikethrough | color(Color::GrayDark);
        }
        choices.push_back(line);
        i++;
      }
      prompt_content = vbox(choices);
    } break;
    case SkaldTester::TEXT:
      prompt_content = input->Render();
      break;
    }

    auto prompt_contents =
        prompt_content ? vbox(text(tester.current_prompt), prompt_content)
                       : text(tester.current_prompt);
    auto prompt_box =
        prompt_contents | borderStyled(ROUNDED, Color::MediumPurple2);

    auto main_box = vbox({
                        padded_logs,
                        prompt_box,
                    }) |
                    borderStyled(ROUNDED, Color::White) | flex;

    if (!show_logs) {
      return main_box;
    }

    Elements debug_elements;
    debug_elements.push_back(filler());
    size_t available = DEBUG_PANEL_HEIGHT - 2;
    size_t start =
        log_lines.size() > available ? log_lines.size() - available : 0;
    for (size_t i = start; i < log_lines.size(); i++) {
      Color c = Color::GrayLight;
      switch (log_lines[i].severity) {
      case LogSeverity::DEBUG:
        c = Color::GrayLight;
        break;
      case LogSeverity::INFO:
        c = Color::White;
        break;
      case LogSeverity::WARN:
        c = Color::Yellow;
        break;
      case LogSeverity::ERROR:
        c = Color::Red;
        break;
      }
      auto line = text(log_lines[i].msg) | color(c);
      if (i == log_lines.size() - 1) {
        line = line | bold;
      }
      debug_elements.push_back(line);
    }
    auto debug_panel = vbox(debug_elements) |
                       size(HEIGHT, EQUAL, DEBUG_PANEL_HEIGHT) |
                       borderStyled(ROUNDED, Color::Yellow);

    return vbox({main_box, debug_panel});
  });

  // This will advance to the next point and exit if we reach an END type.
  auto do_next = [&] {
    if (std::holds_alternative<End>(response)) {
      auto &end = std::get<End>(response);
      exit_reason = end.reason.empty() ? "Exited because narrative reached End."
                                       : end.reason;
      screen.Exit();
      return;
    }
    tester.process(response);
    input_content = "";
  };

  component = CatchEvent(component, [&](Event event) {
    if (event == Event::Escape) {
      exit_reason = "Exited because user pressed Escape.";
      screen.Exit();
      return true;
    }
    if (event == Event::F12) {
      show_logs = !show_logs;
      return true;
    }
    switch (tester.expected_input) {
    case SkaldTester::CONTINUE: {
      if (event == Event::Character(' ') || event == Event::Character('n')) {
        response = tester.do_continue(response);
        do_next();
        return true;
      }
    } break;
    case SkaldTester::TEXT: {
      if (event == Event::Return) {
        response = tester.do_input(response, input_content);
        do_next();
        return true;
      }
    } break;
    case SkaldTester::CHOICES: {
      int selected = -1;

      if (event == Event::Character('1'))
        selected = 1;
      if (event == Event::Character('2'))
        selected = 2;
      if (event == Event::Character('3'))
        selected = 3;
      if (event == Event::Character('4'))
        selected = 4;
      if (event == Event::Character('5'))
        selected = 5;
      if (event == Event::Character('6'))
        selected = 6;
      if (event == Event::Character('7'))
        selected = 7;
      if (event == Event::Character('8'))
        selected = 8;
      if (event == Event::Character('9'))
        selected = 9;
      if (selected > -1 && tester.current_options.size() >= selected) {
        response = tester.do_choice(response, selected - 1);
        do_next();
        return true;
      }
    } break;
    }
    return false;
  });

  screen.Loop(component);

  if (!exit_reason.empty()) {
    std::cout << exit_reason << std::endl;
  }

  std::cout << "\n--- LOGS ---" << std::endl;
  for (const auto &entry : log_lines) {
    const char *prefix = "~ ";
    switch (entry.severity) {
    case LogSeverity::DEBUG:
      prefix = "";
      break;
    case LogSeverity::INFO:
      prefix = "";
      break;
    case LogSeverity::WARN:
      prefix = "[*] ";
      break;
    case LogSeverity::ERROR:
      prefix = "[!] ";
      break;
    }
    std::cout << prefix << entry.msg << std::endl;
  }

  std::cout << "\n\nGoodbye!" << std::endl;
  return 0;
}
