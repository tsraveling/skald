#!/usr/bin/env bash
# Compiles test/errors, expects exit 1, and diffs diagnostics against the golden.
set -uo pipefail
skalder="$1"; testdir="$2"
out="$(mktemp -d)/out"; trap 'rm -rf "$(dirname "$out")"' EXIT
diag="$("$skalder" --compile lua "$testdir/errors" -o "$out" 2>&1 >/dev/null)"
code=$?
[ "$code" -eq 1 ] || { echo "expected exit 1, got $code"; exit 1; }
diff "$testdir/errors/expected.stderr" <(printf '%s\n' "$diag")
