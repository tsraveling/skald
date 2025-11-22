#include "skald.h"
#include <iostream>

int main() {
  std::cout << "Performing parser test on /test/test.ska:\n\n";

  Skald::Skald skald;

  skald.trace("../test/test.ska");
  std::cout << "\n\nNow performing the actual parse:\n\n";

  skald.load("../test/test.ska");

  std::cout << "Skald test complete.\n";

  return 0;
}
