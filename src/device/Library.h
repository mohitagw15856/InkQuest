// Scans /inkquest/stories for compiled stories and reads each one's metadata for
// the library screen. Only the small header, variable table and passage LUT are
// read per story (StoryEngine::open); passage bodies stay on the card.
#pragma once

#ifdef ARDUINO

#include <string>
#include <vector>

namespace inkquest {

struct LibraryEntry {
  std::string path;    // absolute .iqs path
  std::string title;
  std::string uid;
  std::string cover;   // relative BMP path, empty if none
  bool hasProgress = false;  // an autosave exists for this story
};

// Enumerate stories on the card, sorted by title. Missing directory yields an
// empty list rather than an error.
std::vector<LibraryEntry> scanLibrary();

}  // namespace inkquest

#endif  // ARDUINO
