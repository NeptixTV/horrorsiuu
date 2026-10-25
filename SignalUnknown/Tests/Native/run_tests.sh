#!/usr/bin/env bash
# Builds and runs the engine-independent logic tests (no Unreal Engine required).
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${TMPDIR:-/tmp}/signal_unknown_tests"
mkdir -p "$OUT"
CXX="${CXX:-g++}"
FLAGS=(-std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -fno-exceptions -fno-rtti)

"$CXX" "${FLAGS[@]}" "$HERE/LogicTests.cpp" -o "$OUT/LogicTests"
"$OUT/LogicTests"

if [[ "${1:-}" == "--preview" ]]; then
  "$CXX" "${FLAGS[@]}" "$HERE/MapPreview.cpp" -o "$OUT/MapPreview"
  "$OUT/MapPreview" "$OUT/map_preview.ppm" 800
  echo "map preview written to $OUT/map_preview.ppm"
fi
