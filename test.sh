#!/usr/bin/env bash
# Dev loop: build skald + run tests. Edit code, run `./test.sh`, repeat.
# Forwards extra args to ctest, e.g. `./test.sh -R grammar` or `./test.sh --stop-on-failure`.
set -euo pipefail

BUILD_DIR="build"

# First run only: generate Ninja build files with tests enabled + debug symbols.
# Skipped on subsequent runs since CMake re-runs itself automatically when
# CMakeLists.txt changes.
if [ ! -f "${BUILD_DIR}/build.ninja" ]; then
    cmake -S . -B "${BUILD_DIR}" -G Ninja \
        -DSKALD_BUILD_TESTS=ON \
        -DCMAKE_BUILD_TYPE=Debug
fi

# Incremental build — Ninja only recompiles what changed.
cmake --build "${BUILD_DIR}" -j

# Run both test suites (grammar + e2e). --output-on-failure shows doctest output
# only when something fails, keeping passing runs quiet.
ctest --test-dir "${BUILD_DIR}" --output-on-failure "$@"
