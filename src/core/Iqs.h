// InkQuest compiled-story format (.iqs) constants.
//
// This header is the single C++ source of truth for the on-disk format that the
// companion compiler (companion/inkquest/iqs.py) writes and the on-device reader
// consumes. It is deliberately free of any Arduino / freeink-sdk / inkkit device
// dependency so the whole story core compiles and is unit-tested on the host,
// mirroring the way inkkit keeps ByteStream.h host-portable.
//
// See docs/FORMAT.md for the byte-level layout and rationale.
#pragma once

#include <cstdint>

namespace inkquest {

// Magic bytes at offset 0: the ASCII string "IQS1".
constexpr char kMagic0 = 'I';
constexpr char kMagic1 = 'Q';
constexpr char kMagic2 = 'S';
constexpr char kMagic3 = '1';

// The format version written into the header. Bumped on any incompatible change.
constexpr uint8_t kFormatVersion = 1;

// Fixed header size in bytes.
constexpr uint32_t kHeaderSize = 32;

// Sentinel passage index meaning "no target" (a choice that ends the story).
constexpr uint16_t kNoPassage = 0xFFFF;

// Header flag bits.
enum HeaderFlags : uint8_t {
  FLAG_HAS_COVER = 1 << 0,  // the story ships a cover.bmp alongside the .iqs
};

// Expression / condition bytecode. Evaluated on a small integer stack; for a
// condition, a non-zero result means true. Operands are little-endian and
// follow the opcode byte inline.
enum Op : uint8_t {
  OP_PUSH_I32 = 0x01,  // + int32 literal
  OP_PUSH_VAR = 0x02,  // + uint16 variable index
  OP_ADD = 0x10,       // a b -> a+b
  OP_SUB = 0x11,       // a b -> a-b
  OP_MUL = 0x12,       // a b -> a*b
  OP_NEG = 0x13,       // a   -> -a
  OP_EQ = 0x20,        // a b -> a==b
  OP_NE = 0x21,        // a b -> a!=b
  OP_LT = 0x22,        // a b -> a<b
  OP_LE = 0x23,        // a b -> a<=b
  OP_GT = 0x24,        // a b -> a>b
  OP_GE = 0x25,        // a b -> a>=b
  OP_AND = 0x30,       // a b -> a&&b
  OP_OR = 0x31,        // a b -> a||b
  OP_NOT = 0x32,       // a   -> !a
};

// Assignment operators used by a Setter (passage onEnter effects and per-choice
// effects). The right-hand side is an expression bytecode blob.
enum SetOp : uint8_t {
  SET_ASSIGN = 0,  // var  = expr
  SET_ADD = 1,     // var += expr
  SET_SUB = 2,     // var -= expr
  SET_MUL = 3,     // var *= expr
};

// Body-part kinds inside a passage record.
enum BodyKind : uint8_t {
  BODY_TEXT = 0,       // always-shown text run
  BODY_COND_TEXT = 1,  // text run shown only when its condition evaluates true
};

// Variable declared type. Informational only: every value is stored as int32 at
// runtime (booleans and inventory flags are 0 / 1). The type guides the
// companion validator and any future UI.
enum VarType : uint8_t {
  VAR_INT = 0,
  VAR_BOOL = 1,
  VAR_FLAG = 2,  // inventory flag; semantically boolean
};

}  // namespace inkquest
