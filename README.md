# InkQuest

InkQuest is an interactive fiction runner (choose your own adventure) for the
Xteink X4/X3 pocket e-reader (ESP32-C3, 4.2 inch e-ink). It plays branching
stories with variables, conditionals and multiple endings, saves your progress
to the SD card, and comes with a desktop tool that compiles Twine and Ink stories
into its compact on device format.

InkQuest is a standalone firmware. It uses
[inkkit](https://github.com/mohitagw15856/inkkit) for the device layer (display,
SD, input, power) and treats the
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) as an
architectural reference rather than a base to fork. See
[ARCHITECTURE.md](ARCHITECTURE.md) for the reasoning.

## Screenshots

Placeholder. Add device photographs and framebuffer captures here.

- `docs/images/library.png` : the story library
- `docs/images/reading.png` : a passage with choices
- `docs/images/slots.png` : the save slot picker

## Features

On device:

- Runs compiled stories from `/inkquest/stories/*.iqs`.
- Passage text with word wrapped layout; choices selected with the physical
  buttons.
- Numeric and boolean variables, inventory flags, conditional choices and
  conditional text.
- Multiple save slots per story on the SD card, plus autosave on sleep.
- A library screen with per story progress and an optional cover image (BMP).

Companion desktop tool (`companion/`, a Python CLI):

- `inkquest compile` compiles Twine (Harlowe or SugarCube export) and Ink
  (inklecate JSON) stories into `.iqs`, with clear errors for unsupported
  features.
- `inkquest validate` reports dead ends and unreachable passages.

## Compatibility

- Built for the Xteink X4/X3 on the ESP32-C3, on the freeink-sdk hardware layer
  through inkkit.
- Designed and tested against **CrossPoint v1.5.0** as the reference firmware.
  InkQuest does not require CrossPoint: it is its own app. It follows the same SD
  card convention of a single hidden app root (InkQuest uses `/inkquest`), so one
  card can carry CrossPoint books and InkQuest stories side by side.

## Repository layout

```
src/core/        portable story engine, saves, word wrap, BMP decoder (host tested)
src/device/      on device UI, rendering, storage, input (built on inkkit)
src/main.cpp     Arduino setup/loop shell
companion/       Python CLI: Twine / Ink -> .iqs, and validation, with pytest
stories/         the bundled demo story (Twine source and compiled .iqs)
test/host/       host unit tests for the core (plain g++)
test/device/     device compile check against stub HAL headers
docs/            format spec, hardware testing checklist, inkkit gaps
scripts/         the font generator
```

## Install and flash (PlatformIO)

InkQuest builds with [PlatformIO](https://platformio.org/).

```sh
# From the repository root:
pio run -e xteink                 # build
pio run -e xteink -t upload       # build and flash over USB
pio device monitor                # serial monitor at 115200
```

The device build needs the freeink-sdk hardware libraries (the `Hal*` layer that
inkkit wraps) on the PlatformIO `lib_deps`, exactly as any inkkit based firmware
does. See the comments in [platformio.ini](platformio.ini) and
[docs/HARDWARE_TESTING.md](docs/HARDWARE_TESTING.md).

Then prepare an SD card:

```
/inkquest/stories/the-lighthouse-keeper.iqs
/inkquest/stories/the-lighthouse-keeper.bmp   (optional cover)
```

## Authoring and compiling stories

```sh
cd companion
python3 -m inkquest validate ../stories/the-lighthouse-keeper/the-lighthouse-keeper.twee
python3 -m inkquest compile  ../stories/the-lighthouse-keeper/the-lighthouse-keeper.twee \
    -o ../stories/the-lighthouse-keeper/the-lighthouse-keeper.iqs
```

Copy the resulting `.iqs` to `/inkquest/stories` on the card. The bundled demo,
"The Lighthouse Keeper's Lantern", is an original public domain setting of about
30 passages; its Twine source and compiled output are in
`stories/the-lighthouse-keeper/`.

## Building and testing

No hardware is needed to run the tests.

```sh
# Companion (Python)
cd companion && python3 -m pytest

# Portable core (host C++)
INKKIT=/path/to/inkkit ./test/host/run.sh

# Device layer compile check (host C++, against stub HAL headers)
INKKIT=/path/to/inkkit ./test/device/compile_check.sh
```

CI runs all of the above on every push. See
[.github/workflows/ci.yml](.github/workflows/ci.yml).

## Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) : design and the fork versus standalone
  decision.
- [docs/FORMAT.md](docs/FORMAT.md) : the `.iqs` and save byte formats, the
  bytecode, and the supported Twine and Ink subsets.
- [docs/HARDWARE_TESTING.md](docs/HARDWARE_TESTING.md) : the on device checklist.
- [docs/INKKIT_GAPS.md](docs/INKKIT_GAPS.md) : what inkkit does not yet provide.
- [CONTRIBUTING.md](CONTRIBUTING.md) : how to contribute.

## Licence

MIT. See [LICENSE](LICENSE). Compatible with inkkit (MIT) and CrossPoint (MIT).
