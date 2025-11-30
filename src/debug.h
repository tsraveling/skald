#pragma once
#include <iomanip>
#include <iostream>

#define DEBUG_LOGS

extern bool dbg_out_on;

#ifdef DEBUG_LOGS
#define dbg_out(x)                                                             \
  do {                                                                         \
    if (dbg_out_on) {                                                          \
      std::cout << std::fixed << std::setprecision(2) << x << std::endl;       \
    }                                                                          \
  } while (0)
#else
#define dbg_out(x) ((void)0)
#endif
