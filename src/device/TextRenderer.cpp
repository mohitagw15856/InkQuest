#include "device/TextRenderer.h"

#ifdef ARDUINO

#include <cstring>

#include "device/Config.h"
#include "device/Font5x7.h"

namespace inkquest {

// TODO(hardware-test): the framebuffer bit order (MSB = leftmost pixel) and the
// polarity (set bit = white) are taken from the inkkit/HalDisplay convention and
// must be confirmed on the panel. If text renders inverted or mirrored within a
// byte, flip the mask or the clear value here; nothing else needs to change.
void TextRenderer::clear() {
  uint8_t* fb = display_.framebuffer();
  std::memset(fb, 0xFF, static_cast<size_t>(display_.stride()) * display_.height());
}

void TextRenderer::pixel(int x, int y) {
  if (x < 0 || y < 0 || x >= display_.width() || y >= display_.height()) return;
  uint8_t* fb = display_.framebuffer();
  const int stride = display_.stride();
  uint8_t& byte = fb[y * stride + (x >> 3)];
  const uint8_t mask = static_cast<uint8_t>(0x80 >> (x & 7));
  byte = static_cast<uint8_t>(byte & ~mask);  // clear bit -> black
}

void TextRenderer::fillRect(int x, int y, int w, int h, bool black) {
  for (int yy = y; yy < y + h; ++yy) {
    for (int xx = x; xx < x + w; ++xx) {
      if (black) {
        pixel(xx, yy);
      } else if (xx >= 0 && yy >= 0 && xx < display_.width() && yy < display_.height()) {
        uint8_t* fb = display_.framebuffer();
        uint8_t& byte = fb[yy * display_.stride() + (xx >> 3)];
        byte = static_cast<uint8_t>(byte | (0x80 >> (xx & 7)));  // set bit -> white
      }
    }
  }
}

void TextRenderer::hline(int x, int y, int len) {
  for (int i = 0; i < len; ++i) pixel(x + i, y);
}

int TextRenderer::glyph(int x, int y, char c) {
  const auto code = static_cast<uint8_t>(c);
  const uint8_t* g;
  if (code >= kFontFirst && code <= kFontLast) {
    g = kFont5x7[code - kFontFirst];
  } else {
    g = kFontFallback;
  }
  for (int col = 0; col < kFontWidth; ++col) {
    const uint8_t bits = g[col];
    for (int row = 0; row < kFontHeight; ++row) {
      if (bits & (1u << row)) pixel(x + col, y + row);
    }
  }
  return kFontWidth + kGlyphGap;
}

int TextRenderer::text(int x, int y, const std::string& s) {
  int cx = x;
  for (char c : s) cx += glyph(cx, y, c);
  return cx - x;
}

void TextRenderer::textInverted(int x, int y, const std::string& s, int barH) {
  const int w = static_cast<int>(s.size()) * (kFontWidth + kGlyphGap) + 2 * kGlyphGap;
  fillRect(x - kGlyphGap, y - kLineGap, w, barH, /*black=*/true);
  // Draw glyphs as white by setting bits instead of clearing them.
  int cx = x;
  for (char c : s) {
    const auto code = static_cast<uint8_t>(c);
    const uint8_t* g = (code >= kFontFirst && code <= kFontLast) ? kFont5x7[code - kFontFirst] : kFontFallback;
    for (int col = 0; col < kFontWidth; ++col) {
      const uint8_t bits = g[col];
      for (int row = 0; row < kFontHeight; ++row) {
        if (bits & (1u << row)) {
          const int px = cx + col, py = y + row;
          if (px >= 0 && py >= 0 && px < display_.width() && py < display_.height()) {
            uint8_t* fb = display_.framebuffer();
            uint8_t& byte = fb[py * display_.stride() + (px >> 3)];
            byte = static_cast<uint8_t>(byte | (0x80 >> (px & 7)));
          }
        }
      }
    }
    cx += kFontWidth + kGlyphGap;
  }
}

int TextRenderer::columnsFor(int pixels) const {
  const int per = kFontWidth + kGlyphGap;
  int cols = pixels / per;
  return cols < 1 ? 1 : cols;
}

int TextRenderer::lineHeight() const { return kFontHeight + kLineGap; }

}  // namespace inkquest

#endif  // ARDUINO
