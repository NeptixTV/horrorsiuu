#!/usr/bin/env bash
# Syntax check of the Unreal C++ code WITHOUT Unreal Engine.
#
#   Tools/SyntaxCheck/check.sh            all .cpp files
#   Tools/SyntaxCheck/check.sh Vehicle    only files whose path contains "Vehicle"
#
# Steps: mini-UHT (gen_uht.py: generated headers + UHT/replication rules), engine header wrappers
# pointing to the mock (mock/*.h), then `clang++ -fsyntax-only` with UE-like warnings as errors,
# then lint.py (format strings, includes, encoding).
#
# IMPORTANT: passing this check does NOT mean the project builds with the real engine. The mock
# only imitates the engine API that the project uses. Build with Unreal Engine to be sure.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$(cd "$HERE/../../Source/SignalUnknown" && pwd)"
OUT="${SU_SYNTAX_OUT:-${TMPDIR:-/tmp}/su_syntax_check}"
FILTER="${1:-}"
CXX="${CXX:-clang++}"

rm -rf "$OUT"
mkdir -p "$OUT/ue"

status=0
python3 "$HERE/gen_uht.py" "$SRC" "$OUT" || status=1

# every engine header used by the project -> wrapper that includes the mock
grep -rhoE '#include [<"][^>"]+\.h[>"]' "$SRC" | sed -E 's/#include [<"]//;s/[>"]$//' | sort -u | while read -r h; do
	case "$h" in *.generated.h) continue ;; esac
	if [ -f "$SRC/$h" ] || [ -n "$(find "$SRC" -path "*/$h" -print -quit)" ]; then
		continue
	fi
	mkdir -p "$(dirname "$OUT/ue/$h")"
	printf '#pragma once\n#include "UEMockEngine.h"\n' > "$OUT/ue/$h"
done

FLAGS=(-std=c++20 -fsyntax-only -ferror-limit=60
	-Wall -Wshadow-all -Werror
	-Wno-switch -Wno-unused-private-field -Wno-tautological-compare -Wno-unknown-pragmas
	-Wno-invalid-offsetof -Wno-unused-but-set-variable -Wno-unused-lambda-capture
	-Wno-undefined-var-template -Wno-undefined-inline -Wno-undefined-internal
	-I"$SRC" -I"$OUT/generated" -I"$OUT/ue" -I"$HERE/mock")

count=0
failed=0
while read -r cpp; do
	if [ -n "$FILTER" ] && [[ "$cpp" != *"$FILTER"* ]]; then
		continue
	fi
	count=$((count + 1))
	if ! out=$("$CXX" "${FLAGS[@]}" "$cpp" 2>&1); then
		failed=$((failed + 1))
		echo "=== ${cpp#$SRC/}"
		echo "$out" | head -n "${SU_SYNTAX_LINES:-40}"
	fi
done < <(find "$SRC" -name '*.cpp' | sort)

echo "syntax check: $count files, $failed with errors"
[ "$failed" -eq 0 ] || status=1

python3 "$HERE/lint.py" "$SRC" || status=1
exit $status
