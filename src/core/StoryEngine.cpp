#include "core/StoryEngine.h"

#include <cstring>

namespace inkquest {
namespace {

// Little-endian primitive reads off a ByteReader at an explicit cursor. Each
// helper advances `pos` and returns false if the read ran past end of file.
bool readBytes(inkkit::ByteReader& r, uint32_t& pos, void* dst, size_t n) {
  if (!r.seek(pos)) return false;
  size_t got = r.read(dst, n);
  if (got != n) return false;
  pos += static_cast<uint32_t>(n);
  return true;
}

bool readU8(inkkit::ByteReader& r, uint32_t& pos, uint8_t& out) {
  return readBytes(r, pos, &out, 1);
}

bool readU16(inkkit::ByteReader& r, uint32_t& pos, uint16_t& out) {
  uint8_t b[2];
  if (!readBytes(r, pos, b, 2)) return false;
  out = static_cast<uint16_t>(b[0] | (b[1] << 8));
  return true;
}

bool readU32(inkkit::ByteReader& r, uint32_t& pos, uint32_t& out) {
  uint8_t b[4];
  if (!readBytes(r, pos, b, 4)) return false;
  out = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
        (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
  return true;
}

bool readI32(inkkit::ByteReader& r, uint32_t& pos, int32_t& out) {
  uint32_t u;
  if (!readU32(r, pos, u)) return false;
  out = static_cast<int32_t>(u);
  return true;
}

// Read one signed 32-bit literal directly from a bytecode blob at index `i`.
int32_t decodeI32(const std::vector<uint8_t>& code, size_t i) {
  return static_cast<int32_t>(static_cast<uint32_t>(code[i]) | (static_cast<uint32_t>(code[i + 1]) << 8) |
                              (static_cast<uint32_t>(code[i + 2]) << 16) |
                              (static_cast<uint32_t>(code[i + 3]) << 24));
}

}  // namespace

bool StoryEngine::readString(uint32_t& pos, std::string& out) const {
  uint16_t len;
  if (!readU16(*reader_, pos, len)) return false;
  out.assign(len, '\0');
  if (len == 0) return true;
  if (!readBytes(*reader_, pos, &out[0], len)) return false;
  return true;
}

bool StoryEngine::readBytecode(uint32_t& pos, std::vector<uint8_t>& out) const {
  uint16_t len;
  if (!readU16(*reader_, pos, len)) return false;
  out.assign(len, 0);
  if (len == 0) return true;
  return readBytes(*reader_, pos, out.data(), len);
}

bool StoryEngine::readSetters(uint32_t& pos, std::vector<Setter>& out) const {
  uint16_t count;
  if (!readU16(*reader_, pos, count)) return false;
  out.clear();
  out.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    Setter s;
    if (!readU16(*reader_, pos, s.var)) return false;
    if (!readU8(*reader_, pos, s.op)) return false;
    if (!readBytecode(pos, s.expr)) return false;
    out.push_back(std::move(s));
  }
  return true;
}

bool StoryEngine::open(inkkit::ByteReader& reader) {
  reader_ = &reader;
  open_ = false;

  uint32_t pos = 0;
  char magic[4];
  if (!readBytes(reader, pos, magic, 4)) return false;
  if (magic[0] != kMagic0 || magic[1] != kMagic1 || magic[2] != kMagic2 || magic[3] != kMagic3) return false;

  uint8_t version;
  if (!readU8(reader, pos, version)) return false;
  if (version != kFormatVersion) return false;
  if (!readU8(reader, pos, flags_)) return false;

  uint16_t varCount, reserved16;
  uint32_t metaOffset, varTableOffset, passageLutOffset, reserved32;
  if (!readU16(reader, pos, passageCount_)) return false;
  if (!readU16(reader, pos, varCount)) return false;
  if (!readU16(reader, pos, startPassage_)) return false;
  if (!readU16(reader, pos, reserved16)) return false;
  if (!readU32(reader, pos, metaOffset)) return false;
  if (!readU32(reader, pos, varTableOffset)) return false;
  if (!readU32(reader, pos, passageLutOffset)) return false;
  if (!readU32(reader, pos, reserved32)) return false;

  // Meta section.
  uint32_t mp = metaOffset;
  if (!readString(mp, meta_.title)) return false;
  if (!readString(mp, meta_.author)) return false;
  if (!readString(mp, meta_.uid)) return false;
  if (!readString(mp, meta_.coverPath)) return false;

  // Variable table.
  varNames_.clear();
  varInitial_.clear();
  varTypes_.clear();
  varNames_.reserve(varCount);
  varInitial_.reserve(varCount);
  varTypes_.reserve(varCount);
  uint32_t vp = varTableOffset;
  for (uint16_t i = 0; i < varCount; ++i) {
    std::string name;
    int32_t initial;
    uint8_t type;
    if (!readString(vp, name)) return false;
    if (!readI32(reader, vp, initial)) return false;
    if (!readU8(reader, vp, type)) return false;
    varNames_.push_back(std::move(name));
    varInitial_.push_back(initial);
    varTypes_.push_back(type);
  }

  // Passage lookup table: one absolute u32 offset per passage.
  passageLut_.assign(passageCount_, 0);
  uint32_t lp = passageLutOffset;
  for (uint16_t i = 0; i < passageCount_; ++i) {
    if (!readU32(reader, lp, passageLut_[i])) return false;
  }

  open_ = true;
  reset();
  return true;
}

void StoryEngine::reset() {
  values_ = varInitial_;
  current_ = startPassage_;
}

bool StoryEngine::loadPassage(uint16_t idx, PassageView& out) {
  if (!open_ || idx >= passageCount_) return false;
  uint32_t pos = passageLut_[idx];

  out = PassageView{};
  if (!readString(pos, out.name)) return false;
  if (!readSetters(pos, out.onEnter)) return false;

  uint16_t bodyCount;
  if (!readU16(*reader_, pos, bodyCount)) return false;
  out.body.reserve(bodyCount);
  for (uint16_t i = 0; i < bodyCount; ++i) {
    BodyPart part;
    uint8_t kind;
    if (!readU8(*reader_, pos, kind)) return false;
    part.conditional = (kind == BODY_COND_TEXT);
    if (part.conditional) {
      if (!readBytecode(pos, part.cond)) return false;
    }
    if (!readString(pos, part.text)) return false;
    out.body.push_back(std::move(part));
  }

  uint16_t choiceCount;
  if (!readU16(*reader_, pos, choiceCount)) return false;
  out.choices.reserve(choiceCount);
  for (uint16_t i = 0; i < choiceCount; ++i) {
    Choice c;
    if (!readString(pos, c.text)) return false;
    if (!readU16(*reader_, pos, c.target)) return false;
    uint8_t hasCond;
    if (!readU8(*reader_, pos, hasCond)) return false;
    if (hasCond) {
      if (!readBytecode(pos, c.cond)) return false;
    }
    if (!readSetters(pos, c.effects)) return false;
    out.choices.push_back(std::move(c));
  }
  return true;
}

bool StoryEngine::enter(uint16_t idx, PassageView& out) {
  if (!loadPassage(idx, out)) return false;
  current_ = idx;
  applyEffects(out.onEnter);
  return true;
}

bool StoryEngine::choose(const Choice& choice, PassageView& out) {
  applyEffects(choice.effects);
  if (choice.target == kNoPassage) {
    current_ = kNoPassage;
    return false;
  }
  return enter(choice.target, out);
}

bool StoryEngine::choiceVisible(const Choice& choice) const {
  if (choice.cond.empty()) return true;
  return evaluate(choice.cond) != 0;
}

std::string StoryEngine::visibleText(const PassageView& passage) const {
  std::string text;
  for (const BodyPart& part : passage.body) {
    if (part.conditional && evaluate(part.cond) == 0) continue;
    text += part.text;
  }
  return text;
}

void StoryEngine::applyEffects(const std::vector<Setter>& effects) {
  for (const Setter& s : effects) {
    if (s.var >= values_.size()) continue;
    int32_t rhs = evaluate(s.expr);
    switch (s.op) {
      case SET_ASSIGN: values_[s.var] = rhs; break;
      case SET_ADD: values_[s.var] += rhs; break;
      case SET_SUB: values_[s.var] -= rhs; break;
      case SET_MUL: values_[s.var] *= rhs; break;
      default: break;
    }
  }
}

int32_t StoryEngine::evaluate(const std::vector<uint8_t>& code) const {
  // Fixed-size evaluation stack: no heap, bounded depth. Compiled conditions are
  // shallow, so 32 slots is comfortably enough; overflow yields 0 (false).
  constexpr int kStackMax = 32;
  int32_t stack[kStackMax];
  int sp = 0;
  auto push = [&](int32_t v) {
    if (sp < kStackMax) stack[sp++] = v;
  };
  auto pop = [&]() -> int32_t { return sp > 0 ? stack[--sp] : 0; };

  size_t i = 0;
  const size_t n = code.size();
  while (i < n) {
    uint8_t op = code[i++];
    switch (op) {
      case OP_PUSH_I32:
        if (i + 4 > n) return 0;
        push(decodeI32(code, i));
        i += 4;
        break;
      case OP_PUSH_VAR: {
        if (i + 2 > n) return 0;
        uint16_t idx = static_cast<uint16_t>(code[i] | (code[i + 1] << 8));
        i += 2;
        push(idx < values_.size() ? values_[idx] : 0);
        break;
      }
      case OP_ADD: { int32_t b = pop(), a = pop(); push(a + b); break; }
      case OP_SUB: { int32_t b = pop(), a = pop(); push(a - b); break; }
      case OP_MUL: { int32_t b = pop(), a = pop(); push(a * b); break; }
      case OP_NEG: { push(-pop()); break; }
      case OP_EQ: { int32_t b = pop(), a = pop(); push(a == b); break; }
      case OP_NE: { int32_t b = pop(), a = pop(); push(a != b); break; }
      case OP_LT: { int32_t b = pop(), a = pop(); push(a < b); break; }
      case OP_LE: { int32_t b = pop(), a = pop(); push(a <= b); break; }
      case OP_GT: { int32_t b = pop(), a = pop(); push(a > b); break; }
      case OP_GE: { int32_t b = pop(), a = pop(); push(a >= b); break; }
      case OP_AND: { int32_t b = pop(), a = pop(); push((a != 0) && (b != 0)); break; }
      case OP_OR: { int32_t b = pop(), a = pop(); push((a != 0) || (b != 0)); break; }
      case OP_NOT: { push(pop() == 0); break; }
      default: return 0;  // unknown opcode: fail closed
    }
  }
  return pop();
}

}  // namespace inkquest
