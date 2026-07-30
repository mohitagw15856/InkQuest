// Draws a story cover BMP from the SD card into the framebuffer, streaming it a
// row at a time through the host-tested core decoder (inkquest::decodeBmp).
#pragma once

#ifdef ARDUINO

#include <string>

#include "device/TextRenderer.h"

namespace inkquest {

// Draw the BMP at `path` with its top-left at (x, y), clipped to a maxW x maxH
// box. Black pixels are inked; white pixels leave the page background. Returns
// false if the file is missing or not a supported BMP.
bool drawCover(TextRenderer& renderer, const std::string& path, int x, int y, int maxW, int maxH);

}  // namespace inkquest

#endif  // ARDUINO
