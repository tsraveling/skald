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
  Response *current_response;

  enum InputType { CONTINUE, CHOICES, TEXT };
  std::string current_prompt = "";
  std::vector<std::string> current_options;
  InputType expected_input = InputType::CONTINUE;

  // Process a response for consumption
  void process(Response &response) {
    current_response = &response;
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
              current_prompt = "Select an option";
            } else {
              expected_input = InputType::CONTINUE;
              current_prompt = "Spacebar to continue ...";
            }
          } else if constexpr (std::is_same_v<T, Query>) {
            current_prompt = value.call.dbg_desc();
            expected_input = InputType::TEXT;
          } else if constexpr (std::is_same_v<T, Exit>) {
            current_prompt = "Press spacebar to conclude script.";
            expected_input = InputType::CONTINUE;
          } else if constexpr (std::is_same_v<T, GoModule>) {
            current_prompt =
                "Press spacebar to continue to " + value.module_path + "!";
            expected_input = InputType::CONTINUE;
          } else if constexpr (std::is_same_v<T, End>) {
            current_prompt =
                "This is an END response -- we shouldn't be here anymore!";
            expected_input = InputType::CONTINUE;
          } else if constexpr (std::is_same_v<T, Error>) {
            current_prompt = value.message;
            expected_input = InputType::CONTINUE;
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

  void note_system(std::string val) {
    narrative.push_back(
        NarrativeItem{.content = val, .type = NarrativeItemType::SYSTEM});
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

int main() {
  dbg_out_on = false;

  auto screen = ScreenInteractive::Fullscreen();

  std::string path = "../test/test.ska";
  SkaldTester tester{};
  tester.engine.load(path);
  tester.note_system("STARTING MODULE: " + path);

  Response response;
  response = tester.engine.start();
  tester.process(response);

  std::string input_content;
  auto input = Input(&input_content, "string, int, float, true, or false");

  auto component = Renderer(input, [&] {
    // Assemble the narrative log
    Elements log_elements;
    for (size_t i = 0; i < tester.narrative.size(); i++) {
      auto nar = tester.narrative[i];
      bool is_latest = i == tester.narrative.size() - 1;

      // Add a blank line between items
      log_elements.push_back(text(""));
      auto content = text(nar.content);

      switch (nar.type) {
      case NarrativeItemType::SYSTEM:
        content = content | color(Color::DarkSlateGray1);
        break;
      case NarrativeItemType::ERROR:
        content = content | color(Color::DarkRed);
        break;
      case NarrativeItemType::NORMAL:
        auto col =
            is_latest ? color(Color::White) : color(Color::LightSlateGrey);
        auto par = paragraph(nar.content) | col;
        if (nar.attribution != "") {
          log_elements.push_back(
              hbox(text(tester.narrative[i].attribution + ":") |
                       color(Color::Cyan) | bold,
                   par));
        } else {
          log_elements.push_back(par);
        }
        break;
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
