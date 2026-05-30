#include "debug.h"

bool dbg_out_on = true;
bool dbg_always_cout = true;
std::function<void(const std::string &)> dbg_sink;
