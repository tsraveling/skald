#include "debug.h"

bool dbg_out_on = true;
std::function<void(const std::string &)> dbg_sink;
