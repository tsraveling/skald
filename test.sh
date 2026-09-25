#!/usr/bin/env bash
# Dev loop: build skald + run tests. Edit code, run `./test.sh`, repeat.
# Forwards extra args to ctest, e.g. `./test.sh -R compile` or `./test.sh --stop-on-failure`.
set -euo pipefail

BUILD_DIR="build"

# Fresh build dir: prefer Ninja when available. Existing build dir: keep
# whatever generator configured it (README uses Makefiles) and just make
# sure the tests option is on.
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    gen=()
    if command -v ninja >/dev/null; then
        gen=(-G Ninja)
    fi
    cmake -S . -B "${BUILD_DIR}" "${gen[@]}" \
        -DSKALD_BUILD_TESTS=ON \
        -DCMAKE_BUILD_TYPE=Debug
else
    cmake -S . -B "${BUILD_DIR}" -DSKALD_BUILD_TESTS=ON >/dev/null
fi

# Incremental build.
cmake --build "${BUILD_DIR}" -j

# --output-on-failure keeps passing runs quiet.
ctest --test-dir "${BUILD_DIR}" --output-on-failure "$@"
