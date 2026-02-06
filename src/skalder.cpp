#include "debug.h"
#include "skald.h"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace ftxui;
using namespace Skald;

enum class NarrativeItemType { NORMAL, SYSTEM, ERROR };

struct NarrativeItem {
  std::string attribution;
  std::string content;
  NarrativeItemType type = NarrativeItemType::NORMAL;
};

class SkaldTester {
public:
  /** List of content; latest is current. */
  std::vector<NarrativeItem> narrative;

  /** The current response we are going to be responding to */
  Response &current_response;

  enum InputType { CONTINUE, CHOICES, TEXT };
  std::string current_prompt;
  std::vector<std::string> current_options;
  InputType expected_input;

  /** Gets the input type expected for a given response */
  // STUB: Delete me, obsoleted below
  InputType input_type_for(Response &response) {
    return std::visit(
        [](const auto &value) -> InputType {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, Content>) {
            if (value.options.size() > 0)
              return InputType::CHOICES;
            return InputType::CONTINUE;
          } else if constexpr (std::is_same_v<T, Query>) {
            return InputType::TEXT;
          } else if constexpr (std::is_same_v<T, Exit>) {
            return InputType::CONTINUE;
          } else if constexpr (std::is_same_v<T, GoModule>) {
            return InputType::CONTINUE;
          } else if constexpr (std::is_same_v<T, End>) {
            return InputType::CONTINUE;
          } else if constexpr (std::is_same_v<T, Error>) {
            return InputType::CONTINUE;
          }
        },
        response);
  }

  // Process a response for consumption
  void process(Response &response) {
    current_response = response;
    current_options.clear();

    std::visit(
        [&](const auto &value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, Content>) {
            narrative.push_back(NarrativeItem{.attribution = value.attribution,
                                              .content = stitch(value.text)});
            if (value.options.size() > 0) {
              expected_input = InputType::CHOICES;
              for (size_t i = 0; i < value.options.size(); i++) {
                auto &opt = value.options[i];
                // STUB: Add text for option here
              }
            } else {
              expected_input = InputType::CONTINUE;
              current_prompt = "...";
            }
          } else if constexpr (std::is_same_v<T, Query>) {
            // STUB: Ask for text
          } else if constexpr (std::is_same_v<T, Exit>) {
            // STUB: make a log and continue to exit
          } else if constexpr (std::is_same_v<T, GoModule>) {
            // STUB: make a log and continue to jump
          } else if constexpr (std::is_same_v<T, End>) {
            // STUB: print out the error
          } else if constexpr (std::is_same_v<T, Error>) {
          }
        },
        response);
  }

  /** This will stitch a vector of Skald chunks together into one string */
  static std::string stitch(const std::vector<Chunk> &chunks) {
    std::string ret = "";
    for (auto &chunk : chunks) {
      ret += chunk.text;
    }
    return ret;
  }

  // STUB: Add error log here

  // Perform a "continue" (spacebar) action
  Response do_continue(Response &response) {
    return std::visit(
        [&](const auto &value) -> Response {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, Content>) {
            return engine.act(0);
          } else if constexpr (std::is_same_v<T, Exit>) {
            return End{};
          } else if constexpr (std::is_same_v<T, Query>) {
            return engine.answer(std::nullopt);
          } else {
            // TODO: Better error handling if this is a problem
            return End{};
          }
        },
        response);
  }

  // Perform a "choice selection" (num keys) action
  Response do_choice(Response &response, int choice) {
    return std::visit(
        [&](const auto &value) -> Response {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, Content>) {
            return engine.act(choice);
          } else {
            // TODO: Better error handling if this is a problem
            return End{};
          }
        },
        response);
  }

  // Perform a "query answer" (text entry) action
  Response do_input(Response &response, std::string txt) {
    return std::visit(
        [&](const auto &value) -> Response {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, Query()>) {
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
              return engine.answer(QueryAnswer{parsed});
            }
            // TODO: better error handling if answer not expected
            return End{};
          } else {
            // TODO: Better error handling if this is a problem
            return End{};
          }
        },
        response);
  }

  // Response handle_query(const Query &query) {
  //   if (query.expects_response) {
  //     std::cout << "\nQUERY: " << query.call.method << " > ";
  //   } else {
  //     std::cout << "\nCALL: " << query.call.method << " (input anything) > ";
  //   }
  //   std::string val;
  //   std::cin >> val;

  //   std::optional<QueryAnswer> ret = std::nullopt;
  //   if (query.expects_response) {
  //     SimpleRValue parsed = false;
  //     if (val == "true") {
  //       parsed = true;
  //     } else if (val == "false") {
  //       parsed = false;
  //     } else {
  //       // Try int first (more restrictive than float)
  //       char *end;
  //       long lval = std::strtol(val.c_str(), &end, 10);
  //       if (*end == '\0' && end != val.c_str()) {
  //         parsed = static_cast<int>(lval);
  //       } else {
  //         // Try float
  //         float fval = std::strtof(val.c_str(), &end);
  //         if (*end == '\0' && end != val.c_str()) {
  //           parsed = fval;
  //         } else {
  //           parsed = val;
  //         }
  //       }
  //     }
  //     ret = QueryAnswer{parsed};
  //   }
  //   return engine.answer(ret);
  // }

  // Response handle_content(const Content &content) {
  //   std::cout << stitch(content.text) << "\n";
  //   int selected_choice = 0;
  //   if (content.options.size() > 0) {
  //     std::cout << "\n";
  //     for (size_t i = 0; i < content.options.size(); i++) {
  //       auto opt = content.options[i];
  //       std::cout << (i + 1) << ". " << (opt.is_available ? "" : "<X>")
  //                 << stitch(opt.text) << "\n";
  //     }

  //     while (true) {
  //       std::cout << "\n> ";
  //       std::cin >> selected_choice;
  //       selected_choice--;
  //       if (selected_choice < 0 || selected_choice >= content.options.size())
  //       {
  //         std::cout << "Choice out of bounds! Try again.\n";
  //         continue;
  //       }
  //       if (!content.options[selected_choice].is_available) {
  //         std::cout << "Choice unavailable! Try again.\n";
  //         continue;
  //       }
  //       break;
  //     }

  //     return engine.act(selected_choice);
  //   } else {
  //     int discard;
  //     std::cout << "\n (continue) > ";
  //     std::cin >> discard;
  //     return engine.act(0);
  //   }
  // }

  Engine engine;

  // STUB: debug store
  // STUB: Process and return choices
};

struct Log {
  std::string attribution = "";
  std::string content = "";

  static Log from_content(Content &content) {
    return Log{
        .attribution = content.attribution,
        .content = SkaldTester::stitch(content.text),
    };
  }
};

int main() {
  dbg_out_on = false;

  std::vector<Log> narrative;
  std::vector<std::string> logs;

  auto screen = ScreenInteractive::Fullscreen();

  std::string path = "../test/test.ska";
  SkaldTester tester;
  tester.engine.load(path);
  narrative.push_back(Log{.content = "STARTING MODULE: " + path});

  Response response;
  response = tester.engine.start();

  std::string input_content;
  auto input = Input(&input_content, "placeholder...");

  auto component = Renderer(input, [&] {
    // Assemble the narrative log
    Elements log_elements;
    for (size_t i = 0; i < narrative.size(); i++) {
      log_elements.push_back(text(""));
      bool is_latest = (i == narrative.size() - 1);
      auto content = text(narrative[i].content);
      if (!is_latest) {
        content = content | color(Color::GrayDark);
      }
      if (narrative[i].attribution != "") {
        log_elements.push_back(
            hbox(text(narrative[i].attribution) | color(Color::Cyan) | bold,
                 text(": "), content));
      } else {
        log_elements.push_back(content);
      }
    }
    auto log_box = vbox({
                       filler(),
                       vbox(log_elements),
                   }) |
                   flex | border;

    // SECTION: Prompt
    auto prompt_content = input->Render();

    auto prompt_box =
        prompt_content | borderStyled(ROUNDED, Color::MediumPurple2);

    return vbox({
               log_box,
               prompt_box,
           }) |
           borderStyled(ROUNDED, Color::White);
  });

  component = CatchEvent(component, [&](Event event) {
    if (event == Event::Escape) {
      screen.Exit();
      return true;
    }
    return false;
  });

  screen.Loop(component);

  /*


  while (true) {
    if (std::holds_alternative<End>(response)) {
      break;
    }
    response = std::visit(
        [&tester](const auto &value) -> Response {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, Content>) {
            return tester.handle_content(value);
          } else if constexpr (std::is_same_v<T, Query>) {
            return tester.handle_query(value);
          } else if constexpr (std::is_same_v<T, Exit>) {
            // TODO: Output args
            std::cout << "EXIT!";
            return End{};
          } else if constexpr (std::is_same_v<T, GoModule>) {
            // TODO: Output specifics
            std::cout << "GOMOD!";
            return End{};
          } else if constexpr (std::is_same_v<T, Error>) {
            std::cout << "<! ERROR code " << value.code << " on line "
                      << value.line_number << ": " << value.message << "
  !>\n"; return End{}; } else { // Any unhandled cases just exit return End{};
          }
        },
        response);

    std::cout << "\n----Q CACHE----\n"
              << tester.engine.dbg_print_cache() << "-----\n";
  }

  std::cout << "\n\nScript concluded!\n";
  */

  return 0;
}
