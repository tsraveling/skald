#!/usr/bin/env bash
# Compiles test/ to JSON and validates every file with python3. Skips (77)
# without python3.
set -euo pipefail
skalder="$1"; testdir="$2"
command -v python3 >/dev/null || exit 77
out="$(mktemp -d)"; trap 'rm -rf "$out"' EXIT
"$skalder" --compile json "$testdir" -o "$out" >/dev/null
n=0
while IFS= read -r -d '' f; do
  python3 -m json.tool "$f" >/dev/null
  n=$((n + 1))
done < <(find "$out" -name '*.json' -print0)
[ "$n" -gt 0 ]
