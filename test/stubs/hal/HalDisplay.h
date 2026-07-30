// Compile-check stub of the ecosystem HalDisplay surface inkkit wraps.
#pragma once

#include <cstdint>

class HalDisplay {
 public:
  static constexpr uint16_t DISPLAY_WIDTH = 400;
  static constexpr uint16_t DISPLAY_HEIGHT = 300;
  static constexpr uint16_t DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
  static constexpr uint32_t BUFFER_SIZE = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;

  enum RefreshMode { FULL_REFRESH, FAST_REFRESH };

  void begin() {}
  uint8_t* getFrameBuffer() { return buffer_; }
  void displayBuffer(RefreshMode) {}
  void deepSleep() {}

 private:
  uint8_t buffer_[BUFFER_SIZE] = {};
};

inline HalDisplay display;
