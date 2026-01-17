#include "skald.h"
#include <iostream>
#include <string>
#include <vector>

using namespace Skald;

class SkaldTester {
public:
  /** This will stitch a vector of Skald chunks together into one string */
  std::string stitch(const std::vector<Chunk> &chunks) {
    std::string ret = "";
    for (auto &chunk : chunks) {
      ret += chunk.text;
    }
    return ret;
  }

  Response handle_query(const Query &query) { // TODO: Fix this up
    std::cout << "\nQUERY: " << query.call.method << " > ";
    std::string val;
    std::cin >> val;
    // TODO: add support for bools and ints
    return engine.answer(QueryAnswer{val});
  }

  Response handle_content(const Content &content) {
    std::cout << stitch(content.text) << "\n";
    int selected_choice = 0;
    if (content.options.size() > 0) {
      std::cout << "\n";
      for (size_t i = 0; i < content.options.size(); i++) {
        auto opt = content.options[i];
        std::cout << (i + 1) << ". " << (opt.is_available ? "" : "<X>")
                  << stitch(opt.text) << "\n";
      }

      while (true) {
        std::cout << "\n> ";
        std::cin >> selected_choice;
        selected_choice--;
        if (selected_choice < 0 || selected_choice >= content.options.size()) {
          std::cout << "Choice out of bounds! Try again.\n";
          continue;
        }
        if (!content.options[selected_choice].is_available) {
          std::cout << "Choice unavailable! Try again.\n";
          continue;
        }
        break;
      }

      return engine.act(selected_choice);
    } else {
      int discard;
      std::cout << "\n (continue) > ";
      std::cin >> discard;
      return engine.act(0);
    }
  }

  Engine engine;

  // STUB: debug store
  // STUB: PRocess and return choices
};

int main() {
  std::cout << "Performing parser test on /test/test.ska:\n\n";

  SkaldTester tester;

  // skald.trace("../test/test.ska");
  std::cout << "\n\nNow performing the actual parse:\n\n";

  tester.engine.load("../test/test.ska");

  std::cout << "Skald parse complete!\n";
  std::cout << "\n#####################\n";

  Response response;
  response = tester.engine.start();

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
                      << value.line_number << ": " << value.message << " !>\n";
            return End{};
          } else { // Any unhandled cases just exit
            return End{};
          }
        },
        response);

    std::cout << "\n----Q CACHE----\n"
              << tester.engine.dbg_print_cache() << "-----\n";
  }

  std::cout << "\n\nScript concluded!\n";

  return 0;
}
