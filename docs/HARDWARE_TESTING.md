# Hardware testing checklist

The story core, the save format, word wrapping and the BMP decoder are all unit
tested on the host, and the whole device layer is compile checked against stub
HAL headers. The items below are the parts that can only be judged on a real
Xteink X4/X3, either because they depend on the panel and buttons or because they
depend on exact device-layer behaviour. Each corresponds to a `TODO(hardware-test)`
marker in the source.

Work through this list on device and adjust the single marked location if the
hardware disagrees.

## Display

- [ ] **Framebuffer bit order and polarity.**
      `src/device/TextRenderer.cpp` assumes a set bit is white, a cleared bit is
      black, and that bit 7 (MSB) is the leftmost pixel in a byte. If text renders
      inverted or mirrored within a byte, flip the mask or the clear value there.
- [ ] **Refresh cadence.** Confirm that full refresh on screen changes and fast
      refresh otherwise looks right and does not ghost. Tune in `InkQuestApp`.
- [ ] **Panel dimensions.** The layout reads width and height from inkkit at run
      time, so it should adapt, but confirm the margins in `src/device/Config.h`
      suit the real resolution.

## Input

- [ ] **Button map.** `src/device/Config.h` maps logical Up, Down, Select and
      Back onto raw inkkit button indices 0..3 as a best guess from CrossPoint.
      Confirm each physical button and correct the indices in that one place.
- [ ] **Interactive feel.** Button responsiveness, the reader pagination and the
      highlight behaviour (`InkQuestApp`) can only be judged in the hand.
- [ ] **Poll interval.** `src/main.cpp` polls every 20 ms as a placeholder; tune
      against responsiveness and refresh timing.

## Storage and power

- [ ] **Storage init.** `src/main.cpp` does not call an explicit SD mount. Confirm
      whether the `Storage` singleton needs a `begin()`/mount before
      `setup()`, and add it if so.
- [ ] **File close semantics.** `src/device/SaveStore.cpp` relies on `HalFile`
      flushing and closing on scope exit (the SDK build defines
      `DESTRUCTOR_CLOSES_FILE`). Confirm saves are durably written; close
      explicitly if not.
- [ ] **Autosave on sleep.** Confirm the idle timeout in `Config.h` triggers an
      autosave and a clean deep sleep, and that waking returns to a usable state.
- [ ] **Save timestamps.** Saves store `power.millis() / 1000` as a proxy
      timestamp. If the device has a real time clock via the SDK, feed epoch
      seconds instead so the slot picker can show real dates.

## Directory iteration

- [ ] **Library scan.** `Library::scanLibrary` relies on `inkkit::sd::listFiles`,
      which inkkit itself flags as needing confirmation against the pinned SDK
      directory iteration API. Confirm stories under `/inkquest/stories` are
      enumerated.

## Fonts and covers

- [ ] **Font legibility.** The bundled 5x7 font is a placeholder (see
      `docs/INKKIT_GAPS.md`). Confirm it is readable at the panel's pixel density,
      or move to a richer font engine.
- [ ] **Cover rendering.** Confirm a 1 bpp and a 24 bpp cover BMP both render the
      right way up and thresholded sensibly.
