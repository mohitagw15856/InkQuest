// Cross-language contract test: read a .iqs produced by the Python companion and
// prove the C++ StoryEngine parses and plays it. The fixture path is passed as
// argv[1] (the demo story compiled by `inkquest compile`). This is the test that
// keeps the two independent implementations of the format honest.
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <inkkit/ByteStream.h>

#include "core/StoryEngine.h"

using namespace inkquest;

static int g_failures = 0;
#define CHECK(cond)                                                   \
  do {                                                                \
    if (!(cond)) {                                                    \
      ++g_failures;                                                   \
      std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);   \
    }                                                                 \
  } while (0)

static std::vector<uint8_t> readFile(const char* path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: %s <story.iqs>\n", argv[0]);
    return 2;
  }
  std::vector<uint8_t> data = readFile(argv[1]);
  CHECK(data.size() > 32);

  inkkit::MemoryReader reader(data);
  StoryEngine engine;
  CHECK(engine.open(reader));
  std::printf("title=%s uid=%s passages=%u vars=%u\n", engine.meta().title.c_str(),
              engine.meta().uid.c_str(), engine.passageCount(), engine.variableCount());

  CHECK(engine.meta().title == "The Lighthouse Keeper's Lantern");
  CHECK(engine.meta().uid == "inkquest-lighthouse-0001");
  CHECK(engine.passageCount() == 28);
  CHECK(engine.variableCount() == 5);

  // Enter the start passage: its text must be non-empty and its choices parsable.
  PassageView p;
  CHECK(engine.enter(engine.currentPassage(), p));
  CHECK(!engine.visibleText(p).empty());
  CHECK(!p.choices.empty());

  // Walk toward an ending, preferring ending choices and otherwise unvisited
  // targets so back-links do not trap the walk. The engine must never read out
  // of bounds and must reach an ending (choose returns false).
  std::vector<bool> visited(engine.passageCount(), false);
  visited[engine.currentPassage()] = true;
  bool reachedEnding = false;
  for (int step = 0; step < 500 && engine.currentPassage() != kNoPassage; ++step) {
    const Choice* pick = nullptr;
    // First pass: an ending choice, or a visible choice to somewhere new.
    for (const Choice& c : p.choices) {
      if (!engine.choiceVisible(c)) continue;
      if (c.target == kNoPassage || c.target >= engine.passageCount() || !visited[c.target]) {
        pick = &c;
        break;
      }
    }
    // Fallback: any visible choice (all targets already visited).
    if (!pick) {
      for (const Choice& c : p.choices) {
        if (engine.choiceVisible(c)) {
          pick = &c;
          break;
        }
      }
    }
    CHECK(pick != nullptr);
    if (!pick) break;
    if (!engine.choose(*pick, p)) {
      reachedEnding = true;
      break;
    }
    visited[engine.currentPassage()] = true;
  }
  CHECK(reachedEnding);

  std::printf("fixture test: %s\n", g_failures == 0 ? "OK" : "FAILED");
  return g_failures == 0 ? 0 : 1;
}
