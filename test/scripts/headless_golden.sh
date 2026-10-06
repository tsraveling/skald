#!/usr/bin/env bash
# Runs test_2.ska headless with testbed alpha and diffs the transcript.
set -euo pipefail
skalder="$1"; testdir="$2"
"$skalder" "$testdir/test_2.ska" --script "$testdir/scripts/test_2.inputs" --testbed alpha \
  | diff "$testdir/expected/test_2.transcript" -
