#pragma once
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

// #define DEBUG_LOGS

extern bool dbg_out_on;
extern bool dbg_always_cout;

// Optional sink for dbg_out. If set (e.g. by an ftxui frontend that owns
// stdout), dbg_out routes formatted lines here instead of std::cout. Leave
// unset for plain CLI builds to keep cout behavior.
extern std::function<void(const std::string &)> dbg_sink;

#ifdef DEBUG_LOGS
#define dbg_out(x)                                                             \
  do {                                                                         \
    if (dbg_out_on) {                                                          \
      std::ostringstream _dbg_oss;                                             \
      _dbg_oss << std::fixed << std::setprecision(2) << x;                     \
      if (dbg_sink) {                                                          \
        dbg_sink(_dbg_oss.str());                                              \
      }                                                                        \
      if (!dbg_sink || dbg_always_cout) {                                      \
        std::cout << _dbg_oss.str() << std::endl;                              \
      }                                                                        \
    }                                                                          \
  } while (0)
#else
#define dbg_out(x) ((void)0)
#endif
