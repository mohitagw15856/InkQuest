// Compile-check stub of the ecosystem HalPowerManager surface inkkit wraps.
#pragma once

#include <HalGPIO.h>

class HalPowerManager {
 public:
  void startDeepSleep(HalGPIO&) {}
};

inline HalPowerManager powerManager;
