#include "device/Library.h"

#ifdef ARDUINO

#include <algorithm>

#include <inkkit/SdStream.h>
#include <inkkit/Storage.h>

#include "core/StoryEngine.h"
#include "device/Config.h"
#include "device/SaveStore.h"

namespace inkquest {

std::vector<LibraryEntry> scanLibrary() {
  std::vector<LibraryEntry> entries;

  inkkit::sd::listFiles(kStoriesDir, kStoryExt, [&](const std::string& path) {
    HalFile file;
    if (!inkkit::sd::openRead(kTag, path.c_str(), file)) return;
    inkkit::SdFileReader reader(file);
    StoryEngine engine;
    if (!engine.open(reader)) return;  // skip unreadable / wrong-version files

    LibraryEntry entry;
    entry.path = path;
    entry.title = engine.meta().title;
    entry.uid = engine.meta().uid;
    entry.cover = engine.meta().coverPath;
    entry.hasProgress = peekSlot(entry.uid, kAutosaveSlot).valid;
    entries.push_back(std::move(entry));
  });

  std::sort(entries.begin(), entries.end(),
            [](const LibraryEntry& a, const LibraryEntry& b) { return a.title < b.title; });
  return entries;
}

}  // namespace inkquest

#endif  // ARDUINO
