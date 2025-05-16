#include "skald.h"
#include <iostream>

int main() {
  std::cout << "Hello, Skald!\n\n";

  Skald::Skald skald;
  skald.load("../test/test.ska");

  std::cout << "Skald test complete.\n";

  return 0;
}
