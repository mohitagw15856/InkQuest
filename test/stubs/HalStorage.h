// Compile-check stub of the ecosystem HalStorage / HalFile surface that inkkit
// wraps. Mirrors only the methods inkkit and InkQuest call; behaviour is inert.
// Never linked into a firmware image.
#pragma once

#include <Arduino.h>

#include <cstddef>
#include <cstdint>
#include <functional>

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
  void listDir(const char*, const std::function<void(const char*, bool, size_t)>&) {}
};

// The SDK exposes this as a global singleton named `Storage`.
inline HalStorageClass Storage;
