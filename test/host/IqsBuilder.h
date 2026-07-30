// Tiny in-memory .iqs writer used only by the host tests. It mirrors the layout
// documented in docs/FORMAT.md so the tests can build stories by hand and feed
// them straight to StoryEngine through an inkkit::MemoryReader. The production
// writer lives in the Python companion (companion/inkquest/iqs.py); this exists
// so the C++ reader can be tested without a Python step.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/Iqs.h"

namespace iqtest {

using Bytes = std::vector<uint8_t>;

inline void putU16(Bytes& b, uint16_t v) {
  b.push_back(v & 0xFF);
  b.push_back((v >> 8) & 0xFF);
}
inline void putU32(Bytes& b, uint32_t v) {
  b.push_back(v & 0xFF);
  b.push_back((v >> 8) & 0xFF);
  b.push_back((v >> 16) & 0xFF);
  b.push_back((v >> 24) & 0xFF);
}
inline void putI32(Bytes& b, int32_t v) { putU32(b, static_cast<uint32_t>(v)); }
inline void putStr(Bytes& b, const std::string& s) {
  putU16(b, static_cast<uint16_t>(s.size()));
  b.insert(b.end(), s.begin(), s.end());
}

// Fluent expression bytecode builder (postfix).
struct Expr {
  Bytes code;
  Expr& pushInt(int32_t v) {
    code.push_back(inkquest::OP_PUSH_I32);
    putI32(code, v);
    return *this;
  }
  Expr& pushVar(uint16_t i) {
    code.push_back(inkquest::OP_PUSH_VAR);
    putU16(code, i);
    return *this;
  }
  Expr& op(inkquest::Op o) {
    code.push_back(o);
    return *this;
  }
};

struct Setter {
  uint16_t var;
  inkquest::SetOp op;
  Expr expr;
};

struct BodyPart {
  bool conditional = false;
  Expr cond;
  std::string text;
};

struct Choice {
  std::string text;
  uint16_t target;
  bool hasCond = false;
  Expr cond;
  std::vector<Setter> effects;
};

struct Passage {
  std::string name;
  std::vector<Setter> onEnter;
  std::vector<BodyPart> body;
  std::vector<Choice> choices;
};

struct Var {
  std::string name;
  int32_t initial;
  inkquest::VarType type;
};

inline void putSetters(Bytes& b, const std::vector<Setter>& setters) {
  putU16(b, static_cast<uint16_t>(setters.size()));
  for (const Setter& s : setters) {
    putU16(b, s.var);
    b.push_back(static_cast<uint8_t>(s.op));
    putU16(b, static_cast<uint16_t>(s.expr.code.size()));
    b.insert(b.end(), s.expr.code.begin(), s.expr.code.end());
  }
}

struct Story {
  std::string title, author, uid, coverPath;
  uint16_t start = 0;
  uint8_t flags = 0;
  std::vector<Var> vars;
  std::vector<Passage> passages;

  Bytes build() const {
    Bytes meta;
    putStr(meta, title);
    putStr(meta, author);
    putStr(meta, uid);
    putStr(meta, coverPath);

    Bytes varTable;
    for (const Var& v : vars) {
      putStr(varTable, v.name);
      putI32(varTable, v.initial);
      varTable.push_back(static_cast<uint8_t>(v.type));
    }

    // Encode each passage body, recording where it will land.
    std::vector<Bytes> passageBlobs;
    for (const Passage& p : passages) {
      Bytes pb;
      putStr(pb, p.name);
      putSetters(pb, p.onEnter);
      putU16(pb, static_cast<uint16_t>(p.body.size()));
      for (const BodyPart& part : p.body) {
        pb.push_back(part.conditional ? inkquest::BODY_COND_TEXT : inkquest::BODY_TEXT);
        if (part.conditional) {
          putU16(pb, static_cast<uint16_t>(part.cond.code.size()));
          pb.insert(pb.end(), part.cond.code.begin(), part.cond.code.end());
        }
        putStr(pb, part.text);
      }
      putU16(pb, static_cast<uint16_t>(p.choices.size()));
      for (const Choice& c : p.choices) {
        putStr(pb, c.text);
        putU16(pb, c.target);
        pb.push_back(c.hasCond ? 1 : 0);
        if (c.hasCond) {
          putU16(pb, static_cast<uint16_t>(c.cond.code.size()));
          pb.insert(pb.end(), c.cond.code.begin(), c.cond.code.end());
        }
        putSetters(pb, c.effects);
      }
      passageBlobs.push_back(std::move(pb));
    }

    // Layout: [header 32][meta][varTable][passage blobs...][passage LUT].
    uint32_t metaOffset = inkquest::kHeaderSize;
    uint32_t varTableOffset = metaOffset + static_cast<uint32_t>(meta.size());
    uint32_t firstPassageOffset = varTableOffset + static_cast<uint32_t>(varTable.size());

    std::vector<uint32_t> lut;
    uint32_t cursor = firstPassageOffset;
    for (const Bytes& pb : passageBlobs) {
      lut.push_back(cursor);
      cursor += static_cast<uint32_t>(pb.size());
    }
    uint32_t passageLutOffset = cursor;

    Bytes out;
    out.push_back(inkquest::kMagic0);
    out.push_back(inkquest::kMagic1);
    out.push_back(inkquest::kMagic2);
    out.push_back(inkquest::kMagic3);
    out.push_back(inkquest::kFormatVersion);
    out.push_back(flags);
    putU16(out, static_cast<uint16_t>(passages.size()));
    putU16(out, static_cast<uint16_t>(vars.size()));
    putU16(out, start);
    putU16(out, 0);  // reserved16
    putU32(out, metaOffset);
    putU32(out, varTableOffset);
    putU32(out, passageLutOffset);
    putU32(out, 0);  // reserved32
    while (out.size() < inkquest::kHeaderSize) out.push_back(0);  // pad to header size

    out.insert(out.end(), meta.begin(), meta.end());
    out.insert(out.end(), varTable.begin(), varTable.end());
    for (const Bytes& pb : passageBlobs) out.insert(out.end(), pb.begin(), pb.end());
    for (uint32_t off : lut) putU32(out, off);
    return out;
  }
};

}  // namespace iqtest
