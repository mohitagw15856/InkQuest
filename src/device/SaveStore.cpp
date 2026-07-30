#include "device/SaveStore.h"

#ifdef ARDUINO

#include <inkkit/SdStream.h>
#include <inkkit/Storage.h>

#include "device/Config.h"

namespace inkquest {

std::string slotPath(const std::string& uid, int slot) {
  std::string dir = std::string(kSavesDir) + "/" + uid;
  if (slot == kAutosaveSlot) return dir + "/auto" + kSaveExt;
  return dir + "/slot" + std::to_string(slot) + kSaveExt;
}

bool ensureSavesDir(const std::string& uid) {
  if (!inkkit::sd::ensureDir(kAppRoot)) return false;
  if (!inkkit::sd::ensureDir(kSavesDir)) return false;
  return inkkit::sd::ensureDir((std::string(kSavesDir) + "/" + uid).c_str());
}

SaveInfo peekSlot(const std::string& uid, int slot) {
  SaveInfo info;
  const std::string path = slotPath(uid, slot);
  if (!inkkit::sd::exists(path.c_str())) return info;
  HalFile file;
  if (!inkkit::sd::openRead(kTag, path.c_str(), file)) return info;
  inkkit::SdFileReader reader(file);
  info = peekSave(reader);
  return info;
}

bool writeSlot(const StoryEngine& engine, const std::string& uid, int slot, bool autosave, uint32_t timestamp) {
  if (!ensureSavesDir(uid)) return false;
  const std::string path = slotPath(uid, slot);
  HalFile file;
  if (!inkkit::sd::openWrite(kTag, path.c_str(), file)) return false;
  inkkit::SdFileWriter writer(file);
  size_t n = writeSave(engine, writer, timestamp, autosave);
  // TODO(hardware-test): confirm HalFile flushes/closes on scope exit (the SDK
  // build defines DESTRUCTOR_CLOSES_FILE); otherwise close explicitly here.
  return n > 0;
}

bool readSlot(StoryEngine& engine, const std::string& uid, int slot) {
  const std::string path = slotPath(uid, slot);
  if (!inkkit::sd::exists(path.c_str())) return false;
  HalFile file;
  if (!inkkit::sd::openRead(kTag, path.c_str(), file)) return false;
  inkkit::SdFileReader reader(file);
  return readSave(engine, reader);
}

}  // namespace inkquest

#endif  // ARDUINO
