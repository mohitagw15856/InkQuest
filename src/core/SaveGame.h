// Save / restore serialisation for InkQuest, host-portable.
//
// A save captures exactly the mutable engine state: the current passage index
// and the full int32 variable vector, tagged with the story uid so a slot cannot
// be restored into the wrong story. Saves are tiny (a few dozen bytes) and are
// written through an inkkit::ByteWriter, so the device path streams straight to
// the SD card and the host tests round-trip through memory.
//
// On-disk layout (.iqv), all integers little-endian:
//   "IQV1"          4 bytes magic
//   version         u8   (kSaveVersion)
//   flags           u8   (bit0: autosave)
//   timestamp       u32  (epoch seconds, 0 if unknown)
//   currentPassage  u16
//   storyUid        u16 length-prefixed UTF-8
//   varCount        u16
//   values          int32 * varCount
#pragma once

#include <cstdint>
#include <string>

#include <inkkit/ByteStream.h>

#include "core/StoryEngine.h"

namespace inkquest {

constexpr char kSaveMagic0 = 'I';
constexpr char kSaveMagic1 = 'Q';
constexpr char kSaveMagic2 = 'V';
constexpr char kSaveMagic3 = '1';
constexpr uint8_t kSaveVersion = 1;

enum SaveFlags : uint8_t {
  SAVE_AUTOSAVE = 1 << 0,
};

// Metadata recovered from a save without fully applying it (used by the slot
// picker to show "Chapter, passage, when").
struct SaveInfo {
  bool valid = false;
  bool autosave = false;
  uint32_t timestamp = 0;
  uint16_t currentPassage = kNoPassage;
  std::string storyUid;
};

// Serialise the engine's current state into `writer`. Returns bytes written, or
// 0 on failure.
size_t writeSave(const StoryEngine& engine, inkkit::ByteWriter& writer, uint32_t timestamp, bool autosave);

// Peek at a save's header without mutating any engine. Leaves `reader` cursor
// undefined afterwards.
SaveInfo peekSave(inkkit::ByteReader& reader);

// Restore a save into `engine`. The engine must already have `open()`ed the
// matching story. Fails (returns false, engine untouched) if the magic, version,
// story uid or variable count do not match.
bool readSave(StoryEngine& engine, inkkit::ByteReader& reader);

}  // namespace inkquest
