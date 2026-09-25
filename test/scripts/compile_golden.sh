#!/usr/bin/env bash
# Compiles test/ to Lua in a temp dir and diffs against test/expected.
set -euo pipefail
skalder="$1"; testdir="$2"
out="$(mktemp -d)"; trap 'rm -rf "$out"' EXIT
"$skalder" --compile lua "$testdir" -o "$out" >/dev/null
diff -r -x '*.transcript' "$testdir/expected" "$out"
