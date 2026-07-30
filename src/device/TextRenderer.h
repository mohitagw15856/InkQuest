// Draws text and simple UI chrome into inkkit's 1-bit framebuffer.
//
// inkkit exposes the panel framebuffer but no font or text engine (see
// docs/INKKIT_GAPS.md), so InkQuest brings its own compact 5x7 bitmap font and
// word wrapping. The wrapping itself lives in the host-tested core
// (inkquest::wrapText); this class only rasterises glyphs.
//
// Framebuffer convention (from inkkit/Display.h): a set bit is white, a cleared
// bit is black. We paint black ink on a white page.
#pragma once

#ifdef ARDUINO

#include <cstdint>
#include <string>
#include <vector>

#include <inkkit/Display.h>

namespace inkquest {

class TextRenderer {
 public:
  explicit TextRenderer(inkkit::Display& display) : display_(display) {}

  int width() const { return display_.width(); }
  int height() const { return display_.height(); }

  // Fill the whole page white.
  void clear();

  // Paint a single black pixel, ignoring out-of-bounds coordinates.
  void pixel(int x, int y);

  void hline(int x, int y, int len);

  // Draw one glyph with its top-left at (x, y). Returns the x advance.
  int glyph(int x, int y, char c);

  // Draw a single line of text (no wrapping). Returns the x advance.
  int text(int x, int y, const std::string& s);

  // Draw `s` inverted (white on black) within a filled bar of height `barH`.
  void textInverted(int x, int y, const std::string& s, int barH);

  // How many glyphs fit in `pixels` of width for the current font.
  int columnsFor(int pixels) const;

  // Pixel height of one text line including the configured line gap.
  int lineHeight() const;

  // Push the framebuffer to the panel. `full` requests a clean full refresh.
  void flush(bool full) { display_.flush(full); }

 private:
  void fillRect(int x, int y, int w, int h, bool black);
  inkkit::Display& display_;
};

}  // namespace inkquest

#endif  // ARDUINO
