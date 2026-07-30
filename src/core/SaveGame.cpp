#include "core/SaveGame.h"

namespace inkquest {
namespace {

void putU16(inkkit::ByteWriter& w, uint16_t v) {
  uint8_t b[2] = {static_cast<uint8_t>(v & 0xFF), static_cast<uint8_t>((v >> 8) & 0xFF)};
  w.write(b, 2);
}

void putU32(inkkit::ByteWriter& w, uint32_t v) {
  uint8_t b[4] = {static_cast<uint8_t>(v & 0xFF), static_cast<uint8_t>((v >> 8) & 0xFF),
                  static_cast<uint8_t>((v >> 16) & 0xFF), static_cast<uint8_t>((v >> 24) & 0xFF)};
  w.write(b, 4);
}

bool getBytes(inkkit::ByteReader& r, uint32_t& pos, void* dst, size_t n) {
  if (!r.seek(pos)) return false;
  if (r.read(dst, n) != n) return false;
  pos += static_cast<uint32_t>(n);
  return true;
}

bool getU16(inkkit::ByteReader& r, uint32_t& pos, uint16_t& out) {
  uint8_t b[2];
  if (!getBytes(r, pos, b, 2)) return false;
  out = static_cast<uint16_t>(b[0] | (b[1] << 8));
  return true;
}

bool getU32(inkkit::ByteReader& r, uint32_t& pos, uint32_t& out) {
  uint8_t b[4];
  if (!getBytes(r, pos, b, 4)) return false;
  out = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
        (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
  return true;
}

bool getString(inkkit::ByteReader& r, uint32_t& pos, std::string& out) {
  uint16_t len;
  if (!getU16(r, pos, len)) return false;
  out.assign(len, '\0');
  if (len == 0) return true;
  return getBytes(r, pos, &out[0], len);
}

}  // namespace

size_t writeSave(const StoryEngine& engine, inkkit::ByteWriter& writer, uint32_t timestamp, bool autosave) {
  if (!engine.isOpen()) return 0;
  inkkit::MemoryWriter buf;

  const char magic[4] = {kSaveMagic0, kSaveMagic1, kSaveMagic2, kSaveMagic3};
  buf.write(magic, 4);
  uint8_t version = kSaveVersion;
  buf.write(&version, 1);
  uint8_t flags = autosave ? SAVE_AUTOSAVE : 0;
  buf.write(&flags, 1);
  putU32(buf, timestamp);
  putU16(buf, engine.currentPassage());

  const std::string& uid = engine.meta().uid;
  putU16(buf, static_cast<uint16_t>(uid.size()));
  if (!uid.empty()) buf.write(uid.data(), uid.size());

  uint16_t varCount = engine.variableCount();
  putU16(buf, varCount);
  for (uint16_t i = 0; i < varCount; ++i) {
    putU32(buf, static_cast<uint32_t>(engine.variableValue(i)));
  }

  const std::vector<uint8_t>& bytes = buf.bytes();
  return writer.write(bytes.data(), bytes.size());
}

SaveInfo peekSave(inkkit::ByteReader& reader) {
  SaveInfo info;
  uint32_t pos = 0;
  char magic[4];
  if (!getBytes(reader, pos, magic, 4)) return info;
  if (magic[0] != kSaveMagic0 || magic[1] != kSaveMagic1 || magic[2] != kSaveMagic2 || magic[3] != kSaveMagic3)
    return info;
  uint8_t hdr[2];
  if (!getBytes(reader, pos, hdr, 2)) return info;
  if (hdr[0] != kSaveVersion) return info;
  info.autosave = (hdr[1] & SAVE_AUTOSAVE) != 0;
  if (!getU32(reader, pos, info.timestamp)) return info;
  if (!getU16(reader, pos, info.currentPassage)) return info;
  if (!getString(reader, pos, info.storyUid)) return info;
  info.valid = true;
  return info;
}

bool readSave(StoryEngine& engine, inkkit::ByteReader& reader) {
  if (!engine.isOpen()) return false;
  uint32_t pos = 0;
  char magic[4];
  if (!getBytes(reader, pos, magic, 4)) return false;
  if (magic[0] != kSaveMagic0 || magic[1] != kSaveMagic1 || magic[2] != kSaveMagic2 || magic[3] != kSaveMagic3)
    return false;
  uint8_t hdr[2];
  if (!getBytes(reader, pos, hdr, 2)) return false;
  if (hdr[0] != kSaveVersion) return false;

  uint32_t timestamp;
  uint16_t currentPassage;
  std::string uid;
  if (!getU32(reader, pos, timestamp)) return false;
  if (!getU16(reader, pos, currentPassage)) return false;
  if (!getString(reader, pos, uid)) return false;
  if (uid != engine.meta().uid) return false;

  uint16_t varCount;
  if (!getU16(reader, pos, varCount)) return false;
  if (varCount != engine.variableCount()) return false;

  // All checks passed: commit the values.
  for (uint16_t i = 0; i < varCount; ++i) {
    uint32_t v;
    if (!getU32(reader, pos, v)) return false;
    engine.setVariableValue(i, static_cast<int32_t>(v));
  }
  engine.setCurrentPassage(currentPassage);
  return true;
}

}  // namespace inkquest
