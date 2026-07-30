// Device-side configuration: SD-card layout and the physical button map.
//
// The SD paths follow the CrossPoint convention of a single hidden app root; here
// that root is /inkquest, so a card can carry CrossPoint books and InkQuest
// stories side by side without collision.
#pragma once

#include <cstdint>

namespace inkquest {

// SD-card layout.
constexpr const char* kAppRoot = "/inkquest";
constexpr const char* kStoriesDir = "/inkquest/stories";
constexpr const char* kSavesDir = "/inkquest/saves";
constexpr const char* kStoryExt = ".iqs";
constexpr const char* kSaveExt = ".iqv";

// Short owner tag passed to inkkit::sd::openRead / openWrite for SDK logging.
constexpr const char* kTag = "IQ";

// Number of manual save slots per story (slot 0 is reserved for the autosave).
constexpr int kManualSlots = 3;

// Logical buttons. The mapping onto raw inkkit button indices is hardware
// specific and must be checked on device.
enum class Button : uint8_t { Up, Down, Select, Back };

// TODO(hardware-test): confirm these raw inkkit button indices against the
// Xteink X4/X3 button harness. They are the best guess from the CrossPoint
// mapping and are the single place to change if the panel reports differently.
constexpr uint8_t kBtnUp = 0;
constexpr uint8_t kBtnDown = 1;
constexpr uint8_t kBtnSelect = 2;
constexpr uint8_t kBtnBack = 3;

// Idle milliseconds before the reader autosaves and enters deep sleep.
constexpr uint32_t kIdleSleepMs = 60u * 1000u;

// Layout margins in pixels.
constexpr int kMarginX = 6;
constexpr int kMarginTop = 6;
constexpr int kLineGap = 2;   // extra pixels between text lines
constexpr int kGlyphGap = 1;  // extra pixels between glyphs

}  // namespace inkquest
