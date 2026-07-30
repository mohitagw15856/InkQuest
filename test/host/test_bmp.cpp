// Host unit test for the BMP cover decoder.
#include <cstdint>
#include <cstdio>
#include <vector>

#include <inkkit/ByteStream.h>

#include "core/Bmp.h"

using namespace inkquest;

static int g_failures = 0;
#define CHECK(cond)                                                 \
  do {                                                              \
    if (!(cond)) {                                                  \
      ++g_failures;                                                 \
      std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    }                                                               \
  } while (0)

static void put16(std::vector<uint8_t>& b, uint16_t v) {
  b.push_back(v & 0xFF);
  b.push_back(v >> 8);
}
static void put32(std::vector<uint8_t>& b, uint32_t v) {
  b.push_back(v & 0xFF);
  b.push_back((v >> 8) & 0xFF);
  b.push_back((v >> 16) & 0xFF);
  b.push_back((v >> 24) & 0xFF);
}

// A 2x2 24-bpp bottom-up BMP: TL black, TR white, BL white, BR black.
static std::vector<uint8_t> make2x2() {
  std::vector<uint8_t> b;
  b.push_back('B');
  b.push_back('M');
  put32(b, 70);  // file size
  put32(b, 0);   // reserved
  put32(b, 54);  // pixel data offset
  // DIB (BITMAPINFOHEADER)
  put32(b, 40);
  put32(b, 2);   // width
  put32(b, 2);   // height (positive: bottom-up)
  put16(b, 1);   // planes
  put16(b, 24);  // bpp
  put32(b, 0);   // compression
  put32(b, 16);  // image size
  put32(b, 2835);
  put32(b, 2835);
  put32(b, 0);
  put32(b, 0);
  auto white = [&] { b.push_back(0xFF); b.push_back(0xFF); b.push_back(0xFF); };
  auto black = [&] { b.push_back(0x00); b.push_back(0x00); b.push_back(0x00); };
  // Stored bottom row first (y=1): BL white, BR black, + 2 pad bytes.
  white();
  black();
  b.push_back(0);
  b.push_back(0);
  // Stored top row (y=0): TL black, TR white, + 2 pad bytes.
  black();
  white();
  b.push_back(0);
  b.push_back(0);
  return b;
}

int main() {
  std::vector<uint8_t> data = make2x2();
  inkkit::MemoryReader reader(data);
  BmpInfo info;
  bool grid[2][2] = {};
  bool ok = decodeBmp(reader, info, [&](int x, int y, bool black) {
    if (x >= 0 && x < 2 && y >= 0 && y < 2) grid[y][x] = black;
  });
  CHECK(ok);
  CHECK(info.width == 2 && info.height == 2 && info.bpp == 24);
  CHECK(grid[0][0] == true);   // TL black
  CHECK(grid[0][1] == false);  // TR white
  CHECK(grid[1][0] == false);  // BL white
  CHECK(grid[1][1] == true);   // BR black

  // Corrupt the magic: must be rejected.
  data[0] = 'X';
  inkkit::MemoryReader bad(data);
  BmpInfo binfo;
  CHECK(decodeBmp(bad, binfo, [](int, int, bool) {}) == false);

  std::printf("bmp: %s\n", g_failures == 0 ? "OK" : "FAILED");
  return g_failures == 0 ? 0 : 1;
}
