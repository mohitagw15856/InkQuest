#include "device/InkQuestApp.h"

#ifdef ARDUINO

#include "core/TextWrap.h"
#include "device/BmpCover.h"
#include "device/Config.h"
#include "device/SaveStore.h"

namespace inkquest {
namespace {

// Rows available for content below the one-line title bar.
int viewportRows(const TextRenderer& r) {
  const int usable = r.height() - kMarginTop - r.lineHeight() /*title*/ - kMarginTop;
  const int rows = usable / r.lineHeight();
  return rows < 1 ? 1 : rows;
}

int contentCols(const TextRenderer& r) { return r.columnsFor(r.width() - 2 * kMarginX); }

}  // namespace

void InkQuestApp::begin() {
  library_ = scanLibrary();
  librarySel_ = 0;
  screen_ = Screen::Library;
  markActivity();
  requestPaint(/*full=*/true);
}

void InkQuestApp::tick() {
  // Idle autosave-and-sleep.
  if (power_.millis() - lastActivityMs_ >= kIdleSleepMs) {
    autosaveAndSleep();  // does not return on hardware
    return;
  }

  const bool any = pressed(kBtnUp) || pressed(kBtnDown) || pressed(kBtnSelect) || pressed(kBtnBack);
  if (any) markActivity();

  switch (screen_) {
    case Screen::Library: handleLibraryInput(); break;
    case Screen::StoryMenu: handleStoryMenuInput(); break;
    case Screen::Reading: handleReadingInput(); break;
    case Screen::SlotPicker: handleSlotPickerInput(); break;
  }

  if (dirty_) render();
}

// ---------------------------------------------------------------------------
// Library screen
// ---------------------------------------------------------------------------

void InkQuestApp::handleLibraryInput() {
  if (library_.empty()) {
    if (pressed(kBtnSelect) || pressed(kBtnBack)) {
      library_ = scanLibrary();  // let the reader retry after inserting a card
      requestPaint(true);
    }
    return;
  }
  if (pressed(kBtnDown)) {
    librarySel_ = (librarySel_ + 1) % static_cast<int>(library_.size());
    requestPaint();
  } else if (pressed(kBtnUp)) {
    librarySel_ = (librarySel_ + static_cast<int>(library_.size()) - 1) % static_cast<int>(library_.size());
    requestPaint();
  } else if (pressed(kBtnSelect)) {
    openSelectedStory();
  }
}

void InkQuestApp::openSelectedStory() {
  if (librarySel_ < 0 || librarySel_ >= static_cast<int>(library_.size())) return;
  if (loadStoryFile(library_[librarySel_])) {
    storyMenuSel_ = 0;
    screen_ = Screen::StoryMenu;
    requestPaint(true);
  }
}

bool InkQuestApp::loadStoryFile(const LibraryEntry& entry) {
  closeStory();
  if (!inkkit::sd::openRead(kTag, entry.path.c_str(), storyFile_)) return false;
  storyReader_.emplace(storyFile_);
  if (!engine_.open(*storyReader_)) {
    storyReader_.reset();
    return false;
  }
  storyOpen_ = true;
  storyUid_ = engine_.meta().uid;
  storyTitle_ = engine_.meta().title;
  storyCover_ = engine_.meta().coverPath.empty()
                    ? std::string()
                    : std::string(kStoriesDir) + "/" + engine_.meta().coverPath;
  return true;
}

// ---------------------------------------------------------------------------
// Per-story menu
// ---------------------------------------------------------------------------

void InkQuestApp::handleStoryMenuInput() {
  const bool hasAuto = peekSlot(storyUid_, kAutosaveSlot).valid;
  const int count = hasAuto ? 4 : 3;  // Continue?, New, Load, Back
  if (pressed(kBtnDown)) {
    storyMenuSel_ = (storyMenuSel_ + 1) % count;
    requestPaint();
  } else if (pressed(kBtnUp)) {
    storyMenuSel_ = (storyMenuSel_ + count - 1) % count;
    requestPaint();
  } else if (pressed(kBtnBack)) {
    closeStory();
    library_ = scanLibrary();
    screen_ = Screen::Library;
    requestPaint(true);
  } else if (pressed(kBtnSelect)) {
    int sel = storyMenuSel_;
    if (!hasAuto) sel += 1;  // shift past the absent "Continue"
    switch (sel) {
      case 0: continueAutosave(); break;
      case 1: startNewGame(); break;
      case 2: slotPickerForSave_ = false; slotSel_ = 0; screen_ = Screen::SlotPicker; requestPaint(true); break;
      case 3:
        closeStory();
        library_ = scanLibrary();
        screen_ = Screen::Library;
        requestPaint(true);
        break;
      default: break;
    }
  }
}

void InkQuestApp::startNewGame() {
  engine_.reset();
  engine_.enter(engine_.currentPassage(), passage_);
  enterCurrentPassage();
}

void InkQuestApp::continueAutosave() {
  if (readSlot(engine_, storyUid_, kAutosaveSlot)) {
    engine_.loadPassage(engine_.currentPassage(), passage_);
    enterCurrentPassage();
  }
}

void InkQuestApp::enterCurrentPassage() {
  bodyLines_ = wrapText(engine_.visibleText(passage_), contentCols(renderer_));
  scrollTop_ = 0;
  readingSel_ = 0;
  interacted_ = false;
  screen_ = Screen::Reading;
  requestPaint(true);
}

// ---------------------------------------------------------------------------
// Reading screen
// ---------------------------------------------------------------------------

std::vector<int> InkQuestApp::visibleChoiceIndices() const {
  std::vector<int> out;
  for (int i = 0; i < static_cast<int>(passage_.choices.size()); ++i) {
    if (engine_.choiceVisible(passage_.choices[i])) out.push_back(i);
  }
  return out;
}

void InkQuestApp::handleReadingInput() {
  const std::vector<int> vis = visibleChoiceIndices();
  const int count = static_cast<int>(vis.size());

  if (pressed(kBtnBack)) {
    // Autosave the current position and return to the story menu.
    if (storyOpen_) writeSlot(engine_, storyUid_, kAutosaveSlot, /*autosave=*/true, nowSeconds());
    storyMenuSel_ = 0;
    screen_ = Screen::StoryMenu;
    requestPaint(true);
    return;
  }
  if (count == 0) return;

  if (pressed(kBtnDown)) {
    interacted_ = true;
    readingSel_ = (readingSel_ + 1) % count;
    requestPaint();
  } else if (pressed(kBtnUp)) {
    interacted_ = true;
    readingSel_ = (readingSel_ + count - 1) % count;
    requestPaint();
  } else if (pressed(kBtnSelect)) {
    takeChoice(readingSel_);
  }
}

void InkQuestApp::takeChoice(int visibleChoiceIndex) {
  const std::vector<int> vis = visibleChoiceIndices();
  if (visibleChoiceIndex < 0 || visibleChoiceIndex >= static_cast<int>(vis.size())) return;
  const Choice& choice = passage_.choices[vis[visibleChoiceIndex]];
  if (engine_.choose(choice, passage_)) {
    enterCurrentPassage();  // moved to a new passage
  } else {
    // Story ended: autosave is meaningless past the end, so clear it and return
    // to the story menu.
    screen_ = Screen::StoryMenu;
    storyMenuSel_ = 0;
    requestPaint(true);
  }
}

// ---------------------------------------------------------------------------
// Slot picker (manual save / load)
// ---------------------------------------------------------------------------

void InkQuestApp::handleSlotPickerInput() {
  // Entries: slots 1..kManualSlots (save flow), or autosave + slots (load flow).
  const int count = kManualSlots;  // manual slots only; autosave handled via Continue
  if (pressed(kBtnBack)) {
    screen_ = Screen::StoryMenu;
    requestPaint(true);
    return;
  }
  if (pressed(kBtnDown)) {
    slotSel_ = (slotSel_ + 1) % count;
    requestPaint();
  } else if (pressed(kBtnUp)) {
    slotSel_ = (slotSel_ + count - 1) % count;
    requestPaint();
  } else if (pressed(kBtnSelect)) {
    const int slot = slotSel_ + 1;  // slots are 1-based; 0 is the autosave
    if (slotPickerForSave_) {
      writeSlot(engine_, storyUid_, slot, /*autosave=*/false, nowSeconds());
      screen_ = Screen::Reading;
      requestPaint(true);
    } else if (readSlot(engine_, storyUid_, slot)) {
      engine_.loadPassage(engine_.currentPassage(), passage_);
      enterCurrentPassage();
    }
  }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void InkQuestApp::render() {
  switch (screen_) {
    case Screen::Library: renderLibrary(); break;
    case Screen::StoryMenu: renderStoryMenu(); break;
    case Screen::Reading: renderReading(); break;
    case Screen::SlotPicker: renderSlotPicker(); break;
  }
  renderer_.flush(fullRefresh_);
  dirty_ = false;
  fullRefresh_ = false;
}

void InkQuestApp::renderLibrary() {
  renderer_.clear();
  const int lh = renderer_.lineHeight();
  renderer_.textInverted(kMarginX, kMarginTop, "InkQuest Library", lh);
  int y = kMarginTop + lh + kLineGap;

  if (library_.empty()) {
    renderer_.text(kMarginX, y, "No stories found.");
    renderer_.text(kMarginX, y + lh, "Add .iqs files to");
    renderer_.text(kMarginX, y + 2 * lh, kStoriesDir);
    return;
  }

  const int rows = viewportRows(renderer_);
  int top = 0;
  if (librarySel_ >= rows) top = librarySel_ - rows + 1;
  for (int i = 0; i < rows && top + i < static_cast<int>(library_.size()); ++i) {
    const LibraryEntry& e = library_[top + i];
    std::string label = e.title;
    if (e.hasProgress) label += "  *";
    if (top + i == librarySel_) {
      renderer_.textInverted(kMarginX, y, label, lh);
    } else {
      renderer_.text(kMarginX, y, label);
    }
    y += lh;
  }
}

void InkQuestApp::renderStoryMenu() {
  renderer_.clear();
  const int lh = renderer_.lineHeight();
  renderer_.textInverted(kMarginX, kMarginTop, storyTitle_, lh);
  int y = kMarginTop + lh + kLineGap;

  // Show the cover thumbnail in the top-right, if the story ships one.
  if (!storyCover_.empty()) {
    const int box = renderer_.height() / 3;
    drawCover(renderer_, storyCover_, renderer_.width() - box - kMarginX, kMarginTop + lh + kLineGap, box, box);
  }

  const bool hasAuto = peekSlot(storyUid_, kAutosaveSlot).valid;
  std::vector<std::string> items;
  if (hasAuto) items.push_back("Continue");
  items.push_back("New game");
  items.push_back("Load a save");
  items.push_back("Back to library");

  for (int i = 0; i < static_cast<int>(items.size()); ++i) {
    if (i == storyMenuSel_) {
      renderer_.textInverted(kMarginX, y, items[i], lh);
    } else {
      renderer_.text(kMarginX, y, items[i]);
    }
    y += lh;
  }
}

void InkQuestApp::renderReading() {
  renderer_.clear();
  const int lh = renderer_.lineHeight();
  renderer_.textInverted(kMarginX, kMarginTop, storyTitle_, lh);
  const int top = kMarginTop + lh + kLineGap;
  const int rows = viewportRows(renderer_);

  // Build the flat row list: body, a blank spacer, then the visible choices.
  const std::vector<int> vis = visibleChoiceIndices();
  std::vector<std::string> allRows = bodyLines_;
  allRows.push_back("");
  const int firstChoiceRow = static_cast<int>(allRows.size());
  for (size_t i = 0; i < vis.size(); ++i) {
    allRows.push_back(std::string("> ") + passage_.choices[vis[i]].text);
  }

  // Keep the highlighted choice visible once the reader has started choosing.
  if (interacted_ && !vis.empty()) {
    const int selRow = firstChoiceRow + readingSel_;
    if (selRow < scrollTop_) scrollTop_ = selRow;
    if (selRow >= scrollTop_ + rows) scrollTop_ = selRow - rows + 1;
  }
  if (scrollTop_ < 0) scrollTop_ = 0;
  const int maxTop = static_cast<int>(allRows.size()) > rows ? static_cast<int>(allRows.size()) - rows : 0;
  if (scrollTop_ > maxTop) scrollTop_ = maxTop;

  int y = top;
  for (int i = 0; i < rows && scrollTop_ + i < static_cast<int>(allRows.size()); ++i) {
    const int rowIndex = scrollTop_ + i;
    const std::string& rowText = allRows[rowIndex];
    const bool isChoice = rowIndex >= firstChoiceRow;
    const int choiceIdx = rowIndex - firstChoiceRow;
    if (isChoice && choiceIdx == readingSel_ && !vis.empty()) {
      renderer_.textInverted(kMarginX, y, rowText, lh);
    } else {
      renderer_.text(kMarginX, y, rowText);
    }
    y += lh;
  }
}

void InkQuestApp::renderSlotPicker() {
  renderer_.clear();
  const int lh = renderer_.lineHeight();
  renderer_.textInverted(kMarginX, kMarginTop, slotPickerForSave_ ? "Save to slot" : "Load a save", lh);
  int y = kMarginTop + lh + kLineGap;

  for (int i = 0; i < kManualSlots; ++i) {
    const int slot = i + 1;
    SaveInfo info = peekSlot(storyUid_, slot);
    std::string label = "Slot " + std::to_string(slot) + ": ";
    if (info.valid) {
      label += "passage " + std::to_string(info.currentPassage);
    } else {
      label += "empty";
    }
    if (i == slotSel_) {
      renderer_.textInverted(kMarginX, y, label, lh);
    } else {
      renderer_.text(kMarginX, y, label);
    }
    y += lh;
  }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void InkQuestApp::autosaveAndSleep() {
  if (storyOpen_ && screen_ == Screen::Reading) {
    writeSlot(engine_, storyUid_, kAutosaveSlot, /*autosave=*/true, nowSeconds());
  }
  power_.deepSleep();  // parks the panel and powers down; does not return
}

void InkQuestApp::closeStory() {
  storyReader_.reset();
  storyOpen_ = false;
  storyUid_.clear();
  storyTitle_.clear();
}

}  // namespace inkquest

#endif  // ARDUINO
