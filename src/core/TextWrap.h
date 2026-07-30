// Word wrapping for the reader, kept host-portable so the layout logic is unit
// tested off-device. The device renderer (src/device/TextRenderer) converts a
// pixel width into a column count for the active font and calls wrapText; the
// wrapping itself is pure string handling with no Arduino dependency.
#pragma once

#include <string>
#include <vector>

namespace inkquest {

// Greedily wrap `text` to at most `maxCols` columns per line. Explicit '\n'
// characters force line breaks and blank lines are preserved so paragraph
// structure survives. A single word longer than `maxCols` is hard-broken so it
// never overflows the display. `maxCols` <= 0 is treated as 1.
std::vector<std::string> wrapText(const std::string& text, int maxCols);

}  // namespace inkquest
