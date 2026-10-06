#pragma once

#include "skalder_args.h"

/** `skalder <module> --script FILE`. Prints a transcript to stdout and
 *  returns the process exit code. */
int run_headless(const SkalderArgs &args);
