// Streaming BMP decoder for story cover images, host-portable.
//
// Covers are pre-processed on the companion side to small 1-bpp or 24-bpp
// Windows BMPs (see docs/FORMAT.md). The decoder reads the header and then one
// padded row at a time through an inkkit::ByteReader, invoking a plot callback
// per pixel, so nothing larger than a single row is ever held in RAM. The device
// wrapper (src/device/BmpCover) plots straight into the framebuffer; the host
// tests plot into a buffer to check the geometry and thresholding.
#pragma once

#include <cstdint>
#include <functional>

#include <inkkit/ByteStream.h>

namespace inkquest {

struct BmpInfo {
  int width = 0;
  int height = 0;
  uint16_t bpp = 0;
};

// Decode `reader` as a BMP, calling `plot(x, y, black)` for each pixel with a
// top-left origin (rows are de-flipped for bottom-up files). Only uncompressed
// 1-bpp and 24-bpp images are supported. Returns false on any unsupported or
// malformed input, filling `info` with what was parsed when possible.
bool decodeBmp(inkkit::ByteReader& reader, BmpInfo& info,
               const std::function<void(int x, int y, bool black)>& plot);

}  // namespace inkquest
