#include "core/Bmp.h"

#include <vector>

namespace inkquest {
namespace {

bool readAt(inkkit::ByteReader& r, uint32_t pos, void* dst, size_t n) {
  if (!r.seek(pos)) return false;
  return r.read(dst, n) == n;
}

uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t u32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
int32_t i32(const uint8_t* p) { return static_cast<int32_t>(u32(p)); }

// Rec. 601 luma; below mid-grey counts as ink (black).
bool isBlack(uint8_t b, uint8_t g, uint8_t r) {
  const int lum = (77 * r + 150 * g + 29 * b) >> 8;
  return lum < 128;
}

}  // namespace

bool decodeBmp(inkkit::ByteReader& reader, BmpInfo& info,
               const std::function<void(int x, int y, bool black)>& plot) {
  uint8_t fileHeader[14];
  if (!readAt(reader, 0, fileHeader, sizeof(fileHeader))) return false;
  if (fileHeader[0] != 'B' || fileHeader[1] != 'M') return false;
  const uint32_t pixelOffset = u32(fileHeader + 10);

  uint8_t dib[40];
  if (!readAt(reader, 14, dib, sizeof(dib))) return false;
  const uint32_t dibSize = u32(dib + 0);
  if (dibSize < 40) return false;
  const int32_t width = i32(dib + 4);
  const int32_t rawHeight = i32(dib + 8);
  const uint16_t bpp = u16(dib + 14);
  const uint32_t compression = u32(dib + 16);

  info.width = width;
  info.height = rawHeight < 0 ? -rawHeight : rawHeight;
  info.bpp = bpp;

  if (compression != 0) return false;           // only BI_RGB (uncompressed)
  if (width <= 0 || info.height <= 0) return false;
  if (bpp != 1 && bpp != 24) return false;

  const bool bottomUp = rawHeight > 0;
  const int height = info.height;

  // 1-bpp palette: two BGRA entries just after the DIB header. Decide which
  // palette index is the ink colour.
  bool index0Black = true;
  if (bpp == 1) {
    uint8_t pal[8];
    if (!readAt(reader, 14 + dibSize, pal, sizeof(pal))) return false;
    const bool e0 = isBlack(pal[0], pal[1], pal[2]);
    index0Black = e0;  // if entry 0 is dark, a 0 bit is ink
  }

  const int rowBytes = ((bpp * width + 31) / 32) * 4;  // padded to 4 bytes
  std::vector<uint8_t> row(static_cast<size_t>(rowBytes));

  for (int y = 0; y < height; ++y) {
    const int srcRow = bottomUp ? (height - 1 - y) : y;
    if (!readAt(reader, pixelOffset + static_cast<uint32_t>(srcRow) * rowBytes, row.data(), rowBytes))
      return false;
    for (int x = 0; x < width; ++x) {
      bool black;
      if (bpp == 24) {
        const uint8_t* px = &row[static_cast<size_t>(x) * 3];
        black = isBlack(px[0], px[1], px[2]);
      } else {  // 1 bpp
        const uint8_t byte = row[static_cast<size_t>(x >> 3)];
        const bool bit = (byte >> (7 - (x & 7))) & 1;
        const bool bitIsIndex1 = bit;
        black = bitIsIndex1 ? !index0Black : index0Black;
      }
      plot(x, y, black);
    }
  }
  return true;
}

}  // namespace inkquest
