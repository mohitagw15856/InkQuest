// InkQuest story virtual machine: the host-portable heart of the runner.
//
// StoryEngine opens a compiled .iqs story through an inkkit::ByteReader, keeps
// only the small per-story lookup tables and variable state resident, and streams
// one passage at a time from the SD card (or, on the host, from memory). It has
// no Arduino / freeink-sdk dependency, so the exact same object that runs on the
// device is exercised by the host unit tests in test/host.
//
// Memory discipline (see docs/ARCHITECTURE.md):
//   * Resident: the passage offset LUT (4 bytes each), variable names and int32
//     values. For a 200-passage story that is well under 2 KB.
//   * Transient: exactly one PassageView at a time. A passage is small (a few
//     hundred bytes of text plus a handful of choices); the whole story is never
//     held in RAM at once.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <inkkit/ByteStream.h>

#include "core/Iqs.h"

namespace inkquest {

// A compiled assignment: `var <op>= <expr bytecode>`.
struct Setter {
  uint16_t var = 0;
  uint8_t op = SET_ASSIGN;
  std::vector<uint8_t> expr;
};

// One run of passage body text, optionally gated by a condition.
struct BodyPart {
  bool conditional = false;
  std::vector<uint8_t> cond;  // empty unless `conditional`
  std::string text;
};

// A selectable choice. `target` is a passage index, or kNoPassage to end.
struct Choice {
  std::string text;
  uint16_t target = kNoPassage;
  std::vector<uint8_t> cond;  // empty means always shown
  std::vector<Setter> effects;
};

// A fully-parsed single passage. This is the only story content held in RAM at
// any given moment.
struct PassageView {
  std::string name;
  std::vector<Setter> onEnter;
  std::vector<BodyPart> body;
  std::vector<Choice> choices;
};

// Story-level metadata read from the meta section.
struct StoryMeta {
  std::string title;
  std::string author;
  std::string uid;        // stable identifier used for save-slot directories
  std::string coverPath;  // relative BMP path, empty if none
};

class StoryEngine {
 public:
  StoryEngine() = default;

  // Parse the header, variable table and passage LUT from `reader`. The reader
  // must outlive the engine. Returns false on a malformed or unsupported file.
  bool open(inkkit::ByteReader& reader);

  bool isOpen() const { return open_; }
  const StoryMeta& meta() const { return meta_; }
  uint16_t passageCount() const { return passageCount_; }
  uint16_t variableCount() const { return static_cast<uint16_t>(varNames_.size()); }
  const std::string& variableName(uint16_t i) const { return varNames_[i]; }

  // Reset all variables to their declared initial values and move to the start
  // passage without applying its onEnter effects (call enter() to do that).
  void reset();

  // Current passage index (kNoPassage before the first enter()).
  uint16_t currentPassage() const { return current_; }

  // Load passage `idx` into `out` without changing engine state. Returns false
  // on a bad index or truncated record.
  bool loadPassage(uint16_t idx, PassageView& out);

  // Enter passage `idx`: load it, set it as current, and apply its onEnter
  // effects to the variable state. The parsed passage is returned in `out` for
  // rendering. Returns false if the passage cannot be loaded.
  bool enter(uint16_t idx, PassageView& out);

  // True if `choice` should be offered given the current variable state.
  bool choiceVisible(const Choice& choice) const;

  // Apply a choice's effects then enter its target, returning the new passage in
  // `out`. If the target is kNoPassage the story ends: state is updated, current
  // becomes kNoPassage and false is returned.
  bool choose(const Choice& choice, PassageView& out);

  // Concatenate the visible body parts of `passage` into one string, honouring
  // conditional runs against the current variable state.
  std::string visibleText(const PassageView& passage) const;

  // Direct variable access, used by save/restore.
  int32_t variableValue(uint16_t i) const { return values_[i]; }
  void setVariableValue(uint16_t i, int32_t v) { values_[i] = v; }
  void setCurrentPassage(uint16_t idx) { current_ = idx; }

  // Evaluate an expression / condition bytecode blob against current state.
  int32_t evaluate(const std::vector<uint8_t>& code) const;

 private:
  bool readString(uint32_t& pos, std::string& out) const;
  bool readBytecode(uint32_t& pos, std::vector<uint8_t>& out) const;
  bool readSetters(uint32_t& pos, std::vector<Setter>& out) const;
  void applyEffects(const std::vector<Setter>& effects);

  inkkit::ByteReader* reader_ = nullptr;
  bool open_ = false;

  StoryMeta meta_;
  uint8_t flags_ = 0;
  uint16_t passageCount_ = 0;
  uint16_t startPassage_ = 0;
  uint16_t current_ = kNoPassage;

  std::vector<std::string> varNames_;
  std::vector<int32_t> varInitial_;
  std::vector<uint8_t> varTypes_;
  std::vector<int32_t> values_;
  std::vector<uint32_t> passageLut_;
};

}  // namespace inkquest
