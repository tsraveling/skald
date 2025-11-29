#include "skald.h"
#include <iostream>

using namespace Skald;

class SkaldTester {
public:
  Engine engine;

  // STUB: glue text together
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
