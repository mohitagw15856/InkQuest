// The InkQuest application: a small state machine over the library, per-story
// menu and reader, driven by inkkit's buttons, display and power wrappers and
// the host-tested story core. All device-facing plumbing (SD, panel, input) is
// borrowed from inkkit; this class owns only InkQuest's own logic and layout.
//
// TODO(hardware-test): the whole interactive layer (button feel, refresh
// cadence, pagination, sleep/wake) can only be judged on the panel. The logic is
// structured so the story engine underneath is already verified on the host; see
// docs/HARDWARE_TESTING.md for the on-device checklist.
#pragma once

#ifdef ARDUINO

#include <optional>
#include <string>
#include <vector>

#include <inkkit/Buttons.h>
#include <inkkit/Power.h>
#include <inkkit/SdStream.h>
#include <inkkit/Storage.h>

#include "core/StoryEngine.h"
#include "device/Library.h"
#include "device/TextRenderer.h"

namespace inkquest {

class InkQuestApp {
 public:
  InkQuestApp(TextRenderer& renderer, inkkit::Buttons& buttons, inkkit::Power& power)
      : renderer_(renderer), buttons_(buttons), power_(power) {}

  // Full repaint on entry; loads the library.
  void begin();

  // Call once per main-loop iteration after buttons_.update(). Handles input,
  // repaints when needed, and autosaves-then-sleeps after the idle timeout.
  void tick();

 private:
  enum class Screen { Library, StoryMenu, Reading, SlotPicker };

  // Screen handlers.
  void handleLibraryInput();
  void handleStoryMenuInput();
  void handleReadingInput();
  void handleSlotPickerInput();

  void renderLibrary();
  void renderStoryMenu();
  void renderReading();
  void renderSlotPicker();
  void render();

  // Transitions.
  void openSelectedStory();
  bool loadStoryFile(const LibraryEntry& entry);
  void startNewGame();
  void continueAutosave();
  void enterCurrentPassage();
  void takeChoice(int visibleChoiceIndex);
  void autosaveAndSleep();
  void closeStory();

  // Helpers.
  std::vector<int> visibleChoiceIndices() const;
  bool pressed(uint8_t button) const { return buttons_.wasPressed(button); }
  uint32_t nowSeconds() const { return power_.millis() / 1000u; }
  void markActivity() { lastActivityMs_ = power_.millis(); }
  void requestPaint(bool full = false) { dirty_ = true; fullRefresh_ = fullRefresh_ || full; }

  TextRenderer& renderer_;
  inkkit::Buttons& buttons_;
  inkkit::Power& power_;

  Screen screen_ = Screen::Library;
  bool dirty_ = true;
  bool fullRefresh_ = true;
  uint32_t lastActivityMs_ = 0;

  // Library state.
  std::vector<LibraryEntry> library_;
  int librarySel_ = 0;

  // Story-menu state.
  int storyMenuSel_ = 0;

  // Slot-picker state (used for both load and save flows).
  int slotSel_ = 0;
  bool slotPickerForSave_ = false;

  // Open-story state. The HalFile and reader must outlive the engine.
  bool storyOpen_ = false;
  std::string storyUid_;
  std::string storyTitle_;
  std::string storyCover_;  // absolute cover BMP path, empty if none
  HalFile storyFile_;
  std::optional<inkkit::SdFileReader> storyReader_;
  StoryEngine engine_;

  // Reading state.
  PassageView passage_;
  std::vector<std::string> bodyLines_;
  int scrollTop_ = 0;          // first visible row
  int readingSel_ = 0;         // index into visibleChoiceIndices()
  bool interacted_ = false;    // has the reader moved the highlight yet
};

}  // namespace inkquest

#endif  // ARDUINO
