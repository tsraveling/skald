#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <sstream>

class Log {
public:
  template <typename... Args> static void out(Args &&...args) {
    std::ostringstream stream;
    print_with_spaces(stream, std::forward<Args>(args)...);
    std::cout << stream.str();
  }

  template <typename... Args> static void verbose(Args &&...args) {
    std::ostringstream stream;
    print_with_spaces(stream, std::forward<Args>(args)...);
    std::cout << stream.str();
  }

  template <typename... Args> static void err(Args &&...args) {
    std::ostringstream stream;
    print_with_spaces(stream, std::forward<Args>(args)...);
    std::cerr << stream.str();
  }

private:
  template <typename Stream, typename... Args>
  static void print_with_spaces(Stream &stream, Args &&...args) {
    if constexpr (sizeof...(Args) == 0) {
      stream << std::endl;
    } else {
      // Print arguments with spaces
      int idx = 0;
      ((stream << (idx++ == 0 ? "" : " ") << args), ...);
      stream << std::endl;
    }
  }
};

#endif // LOGGER_H
