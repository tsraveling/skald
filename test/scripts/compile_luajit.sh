#!/usr/bin/env bash
# Compiles test/ to Lua and loads every file with luajit. Skips (77) without
# luajit.
set -euo pipefail
skalder="$1"; testdir="$2"
command -v luajit >/dev/null || exit 77
out="$(mktemp -d)"; trap 'rm -rf "$out"' EXIT
"$skalder" --compile lua "$testdir" -o "$out" >/dev/null
n=0
while IFS= read -r -d '' f; do
  luajit -e "local m = dofile('$f'); assert(m.format_version == 1, 'bad format_version')"
  n=$((n + 1))
done < <(find "$out" -name '*.lua' -print0)
[ "$n" -gt 0 ]
