#!/usr/bin/env bash
# Compile-check the device (ARDUINO-guarded) firmware translation units off
# hardware, against lightweight stub HAL headers. This does not produce a
# runnable image; it proves the device code and inkkit's device layer compile
# together against the expected SDK API surface, catching type and signature
# errors that the host unit tests (which exclude ARDUINO code) cannot.
#
# The real on-hardware build is `pio run -e xteink`, which needs the ecosystem
# freeink-sdk and HAL; see docs/HARDWARE_TESTING.md.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
INKKIT="${INKKIT:-$root/../Inkkit}"
stubs="$root/test/stubs"

if [[ ! -f "$INKKIT/src/inkkit/Display.h" ]]; then
  echo "error: inkkit not found at '$INKKIT' (set INKKIT)" >&2
  exit 2
fi

CXX="${CXX:-g++}"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

flags=(-std=gnu++2a -Wall -Wextra -Werror -DARDUINO -c
       -I"$stubs" -I"$INKKIT/src" -I"$root/src")

units=(
  "$root/src/device/TextRenderer.cpp"
  "$root/src/device/SaveStore.cpp"
  "$root/src/device/Library.cpp"
  "$root/src/device/BmpCover.cpp"
  "$root/src/device/InkQuestApp.cpp"
  "$root/src/main.cpp"
  "$INKKIT/src/inkkit/Storage.cpp"
  "$root/src/core/StoryEngine.cpp"
  "$root/src/core/SaveGame.cpp"
  "$root/src/core/TextWrap.cpp"
  "$root/src/core/Bmp.cpp"
)

echo "Device compile-check (CXX=$CXX, ARDUINO defined, stub HAL)"
for u in "${units[@]}"; do
  echo "  CC $(basename "$u")"
  "$CXX" "${flags[@]}" "$u" -o "$out/$(basename "$u").o"
done
echo "device compile-check: OK"
