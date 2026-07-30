// Save-slot management on the SD card, layered over inkkit::sd and the
// host-tested inkquest::SaveGame serialisation.
//
// Layout:  /inkquest/saves/<story-uid>/auto.iqv     (slot 0, the autosave)
//          /inkquest/saves/<story-uid>/slot1.iqv    (manual slots 1..kManualSlots)
//
// Slot 0 is written automatically on sleep; slots 1..N are the player's manual
// saves. Every file is a self-describing .iqv (see SaveGame.h) tagged with the
// story uid, so a slot can never be restored into the wrong story.
#pragma once

#ifdef ARDUINO

#include <string>

#include "core/SaveGame.h"
#include "core/StoryEngine.h"

namespace inkquest {

constexpr int kAutosaveSlot = 0;

// Absolute path of a slot file for a given story uid.
std::string slotPath(const std::string& uid, int slot);

// Create /inkquest/saves and the per-story directory if needed.
bool ensureSavesDir(const std::string& uid);

// Read a slot's header without applying it. `valid` is false when the slot is
// empty or unreadable.
SaveInfo peekSlot(const std::string& uid, int slot);

// Serialise the engine's state into a slot. `timestamp` is stored verbatim.
bool writeSlot(const StoryEngine& engine, const std::string& uid, int slot, bool autosave, uint32_t timestamp);

// Restore a slot into `engine` (which must already have opened the story).
bool readSlot(StoryEngine& engine, const std::string& uid, int slot);

}  // namespace inkquest

#endif  // ARDUINO
