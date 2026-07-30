// Compile-check stub of the ecosystem HalGPIO surface inkkit wraps.
#pragma once

#include <cstdint>

class HalGPIO {
 public:
  enum class WakeupReason { PowerButton, AfterFlash, AfterUSBPower, Other };

  void update() {}
  bool wasPressed(uint8_t) const { return false; }
  bool wasReleased(uint8_t) const { return false; }
  bool isPressed(uint8_t) const { return false; }
  uint32_t getPowerButtonHeldTime() const { return 0; }
  WakeupReason getWakeupReason() const { return WakeupReason::Other; }
};

inline HalGPIO gpio;
