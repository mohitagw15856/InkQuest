# Contributing to InkQuest

Thank you for your interest in InkQuest. This project is small and welcoming;
the notes below keep changes easy to review.

## Getting set up

You need a C++ toolchain (g++ or clang), Python 3.10 or newer, and, for device
builds, [PlatformIO](https://platformio.org/). A checkout of
[inkkit](https://github.com/mohitagw15856/inkkit) is needed to run the C++ tests;
point the `INKKIT` environment variable at it.

```sh
# Companion tests
cd companion && python3 -m pytest

# Core tests (host C++)
INKKIT=/path/to/inkkit ./test/host/run.sh

# Device compile check (host C++, stub HAL)
INKKIT=/path/to/inkkit ./test/device/compile_check.sh
```

All three run in CI on every push and pull request.

## Project shape

- **`src/core`** is portable and must stay free of any Arduino or SDK dependency,
  so it keeps compiling and testing on the host. If you add core logic, add host
  tests for it in `test/host`.
- **`src/device`** is the on device layer. Every translation unit is guarded by
  `#ifdef ARDUINO` and built on inkkit. It does not talk to the SDK directly; if
  you need something inkkit does not offer, record it in `docs/INKKIT_GAPS.md`
  rather than reaching around inkkit.
- **`companion`** is the desktop tool. Keep it dependency free (standard library
  only) and cover new behaviour with `pytest`.

## The story format is a contract

The `.iqs` byte layout is implemented twice, in `companion/inkquest/iqs.py` and
`src/core/StoryEngine.cpp`, and documented in `docs/FORMAT.md`. If you change it:

1. Update all three together.
2. Bump the format version.
3. Update both the Python round trip tests and the C++ cross language fixture
   test.

## Style

- Match the surrounding code. C++ follows the existing brace and naming style;
  Python is standard library, type hinted, small functions.
- Documentation uses British English and avoids em dashes.
- Anything that can only be verified on hardware is marked with a
  `TODO(hardware-test)` comment and listed in `docs/HARDWARE_TESTING.md`.

## Fonts

The bitmap font is generated, not edited by hand. Change the glyph art in
`scripts/gen_font5x7.py` and regenerate:

```sh
python3 scripts/gen_font5x7.py
```

## Commits and pull requests

Keep commits focused and their messages descriptive. Make sure the test commands
above pass before opening a pull request, and mention any `TODO(hardware-test)`
items your change adds.
