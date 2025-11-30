#include "skald.h"
#include <iostream>
#include <string>
#include <vector>

using namespace Skald;

class SkaldTester {
private:
  /** This will stitch a vector of Skald chunks together into one string */
  std::string stitch(std::vector<Chunk> &chunks) {
    std::string ret = "";
    for (auto &chunk : chunks) {
      ret += chunk.text;
    }
    return ret;
  }

public:
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

  return 0;
}
