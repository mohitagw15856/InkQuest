#!/usr/bin/env bash
# Build and run the InkQuest host unit tests with a plain host compiler. No
# device toolchain is required: the story core is Arduino-free and only needs
# inkkit's host-portable ByteStream.h on the include path.
#
# INKKIT points at an inkkit checkout (its src/ holds inkkit/ByteStream.h).
# Locally this defaults to a sibling ../Inkkit; CI clones inkkit and sets it.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
INKKIT="${INKKIT:-$root/../Inkkit}"

if [[ ! -f "$INKKIT/src/inkkit/ByteStream.h" ]]; then
  echo "error: inkkit not found at '$INKKIT' (set INKKIT to an inkkit checkout)" >&2
  exit 2
fi

CXX="${CXX:-g++}"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

echo "Compiling host tests (CXX=$CXX, INKKIT=$INKKIT)"
"$CXX" -std=gnu++2a -Wall -Wextra -Werror \
  -I"$root/src" \
  -I"$INKKIT/src" \
  -I"$here" \
  "$root/src/core/StoryEngine.cpp" \
  "$root/src/core/SaveGame.cpp" \
  "$here/test_core.cpp" \
  -o "$out/test_core"

"$out/test_core"

echo "Compiling text-wrap tests"
"$CXX" -std=gnu++2a -Wall -Wextra -Werror \
  -I"$root/src" \
  "$root/src/core/TextWrap.cpp" \
  "$here/test_textwrap.cpp" \
  -o "$out/test_textwrap"
"$out/test_textwrap"

echo "Compiling BMP decoder tests"
"$CXX" -std=gnu++2a -Wall -Wextra -Werror \
  -I"$root/src" \
  -I"$INKKIT/src" \
  "$root/src/core/Bmp.cpp" \
  "$here/test_bmp.cpp" \
  -o "$out/test_bmp"
"$out/test_bmp"

# Cross-language contract: parse a .iqs produced by the Python companion.
fixture="$root/stories/the-lighthouse-keeper/the-lighthouse-keeper.iqs"
if [[ -f "$fixture" ]]; then
  echo "Compiling cross-language fixture test"
  "$CXX" -std=gnu++2a -Wall -Wextra -Werror \
    -I"$root/src" \
    -I"$INKKIT/src" \
    "$root/src/core/StoryEngine.cpp" \
    "$here/test_fixture.cpp" \
    -o "$out/test_fixture"
  "$out/test_fixture" "$fixture"
else
  echo "note: $fixture not present, skipping cross-language fixture test"
fi
