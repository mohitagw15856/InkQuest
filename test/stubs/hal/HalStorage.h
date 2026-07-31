// Compile-check stub of the ecosystem HalStorage / HalFile surface that inkkit
// wraps. Mirrors only the methods inkkit and InkQuest call; behaviour is inert.
// It stands in for the missing freeink-sdk HAL so the firmware compiles and
// links off hardware (host g++ compile-check and the PlatformIO ci env). It is
// never part of a shippable device image; the real HAL is supplied on device.
#pragma once

#include <Arduino.h>

#include <cstddef>
#include <cstdint>
#include <functional>

// Open flags inkkit's Storage.cpp references. Guarded so the real Arduino /
// newlib definitions win when present.
#ifndef O_RDONLY
#define O_RDONLY 0x0000
#endif
#ifndef O_WRONLY
#define O_WRONLY 0x0001
#endif
#ifndef O_CREAT
#define O_CREAT 0x0100
#endif
#ifndef O_APPEND
#define O_APPEND 0x0008
#endif

// A file handle on the SD card. Real implementation is backed by SdFat.
class HalFile {
 public:
  explicit operator bool() const { return valid_; }
  unsigned long size() const { return 0; }
  bool seekSet(unsigned long) { return true; }
  unsigned long position() const { return 0; }
  int read(void* /*dst*/, size_t /*n*/) { return 0; }
  size_t write(const void* /*src*/, size_t n) { return n; }
  void close() { valid_ = false; }
  // Directory iteration surface, mirroring the real vendored HalFile.
  bool isOpen() const { return valid_; }
  bool isDirectory() const { return false; }
  void rewindDirectory() {}
  HalFile openNextFile() {
    HalFile f;
    f.valid_ = false;
    return f;
  }
  size_t getName(char* name, size_t len) {
    if (len > 0) name[0] = '\0';
    return 0;
  }

 private:
  bool valid_ = true;
};

class HalStorageClass {
 public:
  bool exists(const char*) { return false; }
  void ensureDirectoryExists(const char*) {}
  bool openFileForRead(const char*, const char*, HalFile&) { return false; }
  bool openFileForWrite(const char*, const char*, HalFile&) { return false; }
  HalFile open(const char*, int) { return HalFile(); }
  String readFile(const char*) { return String(); }
  bool writeFile(const char*, const String&) { return true; }
};

// The SDK exposes this as a global singleton named `Storage`.
inline HalStorageClass Storage;
