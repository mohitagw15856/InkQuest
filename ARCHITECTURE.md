# InkQuest Architecture

InkQuest is an interactive fiction runner (choose your own adventure) for the
Xteink X4/X3 pocket e-reader, built on the ESP32-C3. This document records the
main design decisions, the layering, and the reasoning behind the project shape.

## The core decision: standalone app, not a CrossPoint fork

Before writing any code, the CrossPoint Reader source was cloned into a local
`/reference` folder and studied: its `Activity`/`ActivityManager` main loop, its
`GfxRenderer` and `EpdFont` typography stack, its SD card cache under
`/.crosspoint` with versioned, little endian, length prefixed binary formats,
and its `docs/file-formats.md`. CrossPoint is MIT licensed (Copyright 2025 Dave
Allie).

Three shapes were considered:

- **(a) A CrossPoint fork.** Rejected. CrossPoint is a book reader. Its value is
  the EPUB, TXT and image pipeline, its font cache, its network and OPDS stack.
  An interactive fiction runner shares almost none of that. Forking would inherit
  a very large codebase, most of which InkQuest would immediately disable, and
  would tie InkQuest's release cadence to CrossPoint's. The memory budget on the
  ESP32-C3 (about 380 KB usable RAM) rewards a small, single purpose binary, not
  a trimmed fork of a large one.

- **(b) A module or patch for upstream CrossPoint.** Rejected. Interactive
  fiction is not a reading feature; it is a different application with its own
  content format, save model and input model. It does not belong inside a book
  reader's activity tree, and upstream would reasonably not want to carry it.

- **(c) A standalone firmware app.** Chosen. InkQuest is its own small firmware
  that depends on [inkkit](https://github.com/mohitagw15856/inkkit) for the
  device layer (display framebuffer, SD storage, buttons, power, byte streams)
  and reuses CrossPoint only as an *architectural reference*, not as code.

CrossPoint's influence is in the shape of the design, not in copied source. In
particular InkQuest borrows CrossPoint's discipline of pre processing heavy work
off device and streaming small, versioned, offset indexed binary records from
the SD card at run time. No CrossPoint source files are copied into InkQuest.

### Licensing

InkQuest is MIT licensed, the same licence as CrossPoint (MIT) and inkkit (MIT),
so the choices above are all licence compatible. Were any CrossPoint code to be
reused verbatim in future, MIT to MIT reuse with attribution is permitted; today
none is.

## Layering

InkQuest is split into a portable core and a device layer, mirroring the way
inkkit keeps its `ByteStream` interfaces free of any Arduino dependency.

```
                +-------------------------------------------------+
   companion    |  Python CLI: Twine / Ink  ->  .iqs  + validate  |
   (off device) +-------------------------------------------------+
                                   |  writes .iqs (documented format)
                                   v
                +-------------------------------------------------+
   core         |  StoryEngine, SaveGame, TextWrap, Bmp           |
   (portable,   |  no Arduino / SDK dependency, unit tested on    |
    host tested)|  the host with g++                              |
                +-------------------------------------------------+
                                   |  uses inkkit::ByteReader / ByteWriter
                                   v
                +-------------------------------------------------+
   device       |  TextRenderer, Library, SaveStore, BmpCover,    |
   (#ifdef      |  InkQuestApp state machine, main.cpp            |
    ARDUINO)     |  built on inkkit (Display, Buttons, Power, sd) |
                +-------------------------------------------------+
                                   |  wraps
                                   v
                     inkkit (vendors the Hal* layer + SDK hardware libraries)
```

### Portable core (`src/core`)

- `StoryEngine` opens a compiled `.iqs` through an `inkkit::ByteReader`, keeping
  only the small per story lookup tables and the integer variable state resident
  and streaming exactly one passage at a time. It evaluates the compiled
  condition and effect bytecode on a fixed size integer stack, with no heap use
  during evaluation.
- `SaveGame` serialises and restores the mutable engine state (current passage
  plus the variable vector), tagged with the story uid so a save can never be
  applied to the wrong story.
- `TextWrap` performs word wrapping, and `Bmp` decodes cover images a row at a
  time. Both are pure logic with no device dependency.

The core has no `#include <Arduino.h>` and no SDK header. The exact same objects
that run on the device are exercised by the host unit tests in `test/host`,
including a cross language test that reads a `.iqs` produced by the Python
companion and plays it to an ending.

### Device layer (`src/device`)

Everything here is guarded by `#ifdef ARDUINO` and built on inkkit:

- `TextRenderer` rasterises a compact 5x7 bitmap font and simple chrome into
  inkkit's 1 bit framebuffer.
- `Library` scans `/inkquest/stories/*.iqs` and reads each story's metadata.
- `SaveStore` maps save slots to files under `/inkquest/saves/<uid>/`.
- `BmpCover` streams a cover BMP onto the panel.
- `InkQuestApp` is the state machine: library screen, per story menu, reader and
  slot picker, with autosave on idle sleep.
- `main.cpp` wires the device-layer singletons (`display`, `gpio`, `powerManager`)
  into inkkit's wrappers and runs the Arduino `setup`/`loop`.

## Memory discipline

The ESP32-C3 has little RAM, so the core is built to keep almost nothing
resident:

- Resident per story: the passage offset lookup table (4 bytes per passage),
  the variable names and their `int32` values. For a 200 passage story this is
  well under 2 KB.
- Transient: exactly one parsed passage at a time. A passage is a few hundred
  bytes of text plus a handful of choices; the whole story is never in RAM.
- Expression evaluation uses a fixed 32 slot integer stack, never the heap.
- Cover images are decoded one padded row at a time rather than loaded whole.
- All heavy authoring work (parsing Twine or Ink, laying out the graph,
  compiling expressions to bytecode) happens on the desktop in the companion,
  never on the device.

## The `.iqs` format and the two implementations

The compiled story format is written by the Python companion and read by the C++
core. Both implementations of the byte layout are kept in lock step and are the
single most important contract in the project, so they are covered from both
sides: the Python round trip tests write and re read stories through a reference
runtime, and the C++ host tests parse a `.iqs` that the companion actually
produced. The format itself is documented byte by byte in
[docs/FORMAT.md](docs/FORMAT.md).

## Testing strategy

- **Companion:** full `pytest` suite over the expression compiler, the `.iqs`
  writer and reader, both front ends (Twine and Ink), the validator and the CLI.
- **Core:** host built C++ tests (`test/host/run.sh`) covering the engine,
  saves, word wrapping and the BMP decoder, plus the cross language fixture test.
- **Device layer:** a compile check (`test/device/compile_check.sh`) builds every
  `#ifdef ARDUINO` translation unit with a host compiler against stub HAL headers,
  so the device code and inkkit's device layer are proven to compile against the
  expected SDK API surface even without the hardware.
- **On hardware:** items that can only be judged on the panel are marked in code
  with `TODO(hardware-test)` and listed in
  [docs/HARDWARE_TESTING.md](docs/HARDWARE_TESTING.md).

## What lives where inkkit falls short

inkkit deliberately does not provide a font or text engine, nor image decoding.
Those gaps, and how InkQuest works within them today, are recorded in
[docs/INKKIT_GAPS.md](docs/INKKIT_GAPS.md) rather than being worked around
silently.
