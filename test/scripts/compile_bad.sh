#!/usr/bin/env bash
# A project with a GO to a missing module must exit 1 and write nothing.
set -uo pipefail
skalder="$1"; testdir="$2"
out="$(mktemp -d)/out"; trap 'rm -rf "$(dirname "$out")"' EXIT
"$skalder" --compile lua "$testdir/bad" -o "$out" >/dev/null 2>&1
code=$?
[ "$code" -eq 1 ] || { echo "expected exit 1, got $code"; exit 1; }
[ ! -e "$out" ] || { echo "outdir was created"; ls -R "$out"; exit 1; }
