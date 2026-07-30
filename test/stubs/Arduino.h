// Minimal host stub of the Arduino core, used only to compile-check InkQuest's
// device translation units off-hardware (see test/device/compile_check.sh). It
// is NOT a functional Arduino core and is never linked into a firmware image;
// the real core comes from the espressif32 platform on device.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// A tiny stand-in for the Arduino String class covering the surface inkkit uses.
class String {
 public:
  String() = default;
  String(const char* s) : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  const char* c_str() const { return s_.c_str(); }
  unsigned length() const { return static_cast<unsigned>(s_.size()); }

 private:
  std::string s_;
};

inline unsigned long millis() { return 0; }
inline void delay(unsigned long) {}

// POSIX-style open flags referenced by inkkit's Storage.cpp.
#ifndef O_RDONLY
#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_CREAT 0x0100
#define O_APPEND 0x0008
#endif
