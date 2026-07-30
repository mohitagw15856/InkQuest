// Host unit tests for the InkQuest story core. Builds stories in memory with the
// test-only IqsBuilder, runs them through StoryEngine, and round-trips saves.
// Compiled and run by test/host/run.sh and by CI; no device toolchain needed.
#include <cstdio>
#include <cstdlib>
#include <string>

#include <inkkit/ByteStream.h>

#include "IqsBuilder.h"
#include "core/SaveGame.h"
#include "core/StoryEngine.h"

using namespace inkquest;
using iqtest::Expr;
using iqtest::Story;
// iqtest::Choice and iqtest::Setter stay fully qualified below because names of
// the same spelling also live in the inkquest namespace pulled in above.

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    ++g_checks;                                                            \
    if (!(cond)) {                                                         \
      ++g_failures;                                                        \
      std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
    }                                                                      \
  } while (0)

// Build a small branching story exercising variables, effects, conditional
// choices and conditional body text.
//
// vars: gold(int=0, idx0), hasKey(flag=0, idx1)
// p0 gate:   "You stand at the gate."
//              -> "Take the key" [hasKey=1, gold+=5] -> p1
//              -> "Leave"                             -> p2
// p1 hall:   "The hall is cold." + (if hasKey) "The key gleams."
//              -> "Open the vault"  cond hasKey==1  -> p3
//              -> "Search the floor" cond hasKey==0 -> p2
// p2 outside:"You walk into the rain." -> "The End" -> end
// p3 vault:  (if gold>=5) "Gold spills out. You are rich." -> "The End" -> end
static Story makeStory() {
  Story s;
  s.title = "The Gate";
  s.author = "Tests";
  s.uid = "the-gate";
  s.start = 0;
  s.vars = {{"gold", 0, VAR_INT}, {"hasKey", 0, VAR_FLAG}};

  iqtest::Passage gate;
  gate.name = "gate";
  gate.body = {{false, {}, "You stand at the gate."}};
  {
    iqtest::Choice take;
    take.text = "Take the key";
    take.target = 1;
    take.effects = {{1, SET_ASSIGN, Expr().pushInt(1)}, {0, SET_ADD, Expr().pushInt(5)}};
    iqtest::Choice leave;
    leave.text = "Leave";
    leave.target = 2;
    gate.choices = {take, leave};
  }

  iqtest::Passage hall;
  hall.name = "hall";
  hall.body = {{false, {}, "The hall is cold."},
               {true, Expr().pushVar(1).pushInt(1).op(OP_EQ), "The key gleams."}};
  {
    iqtest::Choice open;
    open.text = "Open the vault";
    open.target = 3;
    open.hasCond = true;
    open.cond = Expr().pushVar(1).pushInt(1).op(OP_EQ);
    iqtest::Choice search;
    search.text = "Search the floor";
    search.target = 2;
    search.hasCond = true;
    search.cond = Expr().pushVar(1).pushInt(0).op(OP_EQ);
    hall.choices = {open, search};
  }

  iqtest::Passage outside;
  outside.name = "outside";
  outside.body = {{false, {}, "You walk into the rain."}};
  {
    iqtest::Choice end;
    end.text = "The End";
    end.target = kNoPassage;
    outside.choices = {end};
  }

  iqtest::Passage vault;
  vault.name = "vault";
  vault.body = {{true, Expr().pushVar(0).pushInt(5).op(OP_GE), "Gold spills out. You are rich."}};
  {
    iqtest::Choice end;
    end.text = "The End";
    end.target = kNoPassage;
    vault.choices = {end};
  }

  s.passages = {gate, hall, outside, vault};
  return s;
}

static void testHeaderAndMeta() {
  std::printf("testHeaderAndMeta\n");
  iqtest::Bytes data = makeStory().build();
  inkkit::MemoryReader reader(data);
  StoryEngine engine;
  CHECK(engine.open(reader));
  CHECK(engine.passageCount() == 4);
  CHECK(engine.variableCount() == 2);
  CHECK(engine.meta().title == "The Gate");
  CHECK(engine.meta().uid == "the-gate");
  CHECK(engine.variableName(0) == "gold");
  CHECK(engine.currentPassage() == 0);
}

static void testKeyPath() {
  std::printf("testKeyPath\n");
  iqtest::Bytes data = makeStory().build();
  inkkit::MemoryReader reader(data);
  StoryEngine engine;
  CHECK(engine.open(reader));

  PassageView p;
  CHECK(engine.enter(0, p));
  CHECK(engine.visibleText(p) == "You stand at the gate.");
  CHECK(p.choices.size() == 2);

  // Take the key: sets hasKey=1, gold+=5, moves to the hall.
  CHECK(engine.choose(p.choices[0], p));
  CHECK(engine.currentPassage() == 1);
  CHECK(engine.variableValue(0) == 5);  // gold
  CHECK(engine.variableValue(1) == 1);  // hasKey
  // Conditional body text now visible.
  CHECK(engine.visibleText(p) == "The hall is cold.The key gleams.");
  // Only the "Open the vault" choice is visible (hasKey==1).
  CHECK(engine.choiceVisible(p.choices[0]) == true);
  CHECK(engine.choiceVisible(p.choices[1]) == false);

  CHECK(engine.choose(p.choices[0], p));  // open vault
  CHECK(engine.currentPassage() == 3);
  CHECK(engine.visibleText(p) == "Gold spills out. You are rich.");
  // Ending choice returns false (story over).
  CHECK(engine.choose(p.choices[0], p) == false);
  CHECK(engine.currentPassage() == kNoPassage);
}

static void testNoKeyPath() {
  std::printf("testNoKeyPath\n");
  iqtest::Bytes data = makeStory().build();
  inkkit::MemoryReader reader(data);
  StoryEngine engine;
  CHECK(engine.open(reader));

  PassageView p;
  CHECK(engine.enter(0, p));
  CHECK(engine.choose(p.choices[1], p));  // Leave
  CHECK(engine.currentPassage() == 2);
  CHECK(engine.variableValue(1) == 0);  // hasKey untouched
  CHECK(engine.visibleText(p) == "You walk into the rain.");
}

static void testConditionalTextHiddenWithoutKey() {
  std::printf("testConditionalTextHiddenWithoutKey\n");
  iqtest::Bytes data = makeStory().build();
  inkkit::MemoryReader reader(data);
  StoryEngine engine;
  CHECK(engine.open(reader));
  PassageView p;
  // Enter the hall directly without a key: gleam text hidden, search visible.
  CHECK(engine.enter(1, p));
  CHECK(engine.visibleText(p) == "The hall is cold.");
  CHECK(engine.choiceVisible(p.choices[0]) == false);  // open vault needs key
  CHECK(engine.choiceVisible(p.choices[1]) == true);   // search
}

static void testSaveRestore() {
  std::printf("testSaveRestore\n");
  iqtest::Bytes data = makeStory().build();
  inkkit::MemoryReader reader(data);
  StoryEngine engine;
  CHECK(engine.open(reader));

  PassageView p;
  engine.enter(0, p);
  engine.choose(p.choices[0], p);  // hall with key, gold 5
  CHECK(engine.currentPassage() == 1);

  inkkit::MemoryWriter saveBuf;
  size_t n = writeSave(engine, saveBuf, 12345u, /*autosave=*/true);
  CHECK(n > 0);

  // Peek reports the right story and slot kind.
  inkkit::MemoryReader saveReader(saveBuf.bytes());
  SaveInfo info = peekSave(saveReader);
  CHECK(info.valid);
  CHECK(info.autosave);
  CHECK(info.timestamp == 12345u);
  CHECK(info.storyUid == "the-gate");
  CHECK(info.currentPassage == 1);

  // Restore into a fresh engine over the same story.
  inkkit::MemoryReader reader2(data);
  StoryEngine engine2;
  CHECK(engine2.open(reader2));
  CHECK(engine2.currentPassage() == 0);  // fresh
  inkkit::MemoryReader saveReader2(saveBuf.bytes());
  CHECK(readSave(engine2, saveReader2));
  CHECK(engine2.currentPassage() == 1);
  CHECK(engine2.variableValue(0) == 5);
  CHECK(engine2.variableValue(1) == 1);
}

static void testSaveRejectsWrongStory() {
  std::printf("testSaveRejectsWrongStory\n");
  iqtest::Bytes data = makeStory().build();
  inkkit::MemoryReader reader(data);
  StoryEngine engine;
  engine.open(reader);
  PassageView p;
  engine.enter(0, p);
  inkkit::MemoryWriter saveBuf;
  writeSave(engine, saveBuf, 1u, false);

  // A different story (different uid) must refuse the save.
  Story other = makeStory();
  other.uid = "other-story";
  iqtest::Bytes otherData = other.build();
  inkkit::MemoryReader otherReader(otherData);
  StoryEngine otherEngine;
  otherEngine.open(otherReader);
  inkkit::MemoryReader saveReader(saveBuf.bytes());
  CHECK(readSave(otherEngine, saveReader) == false);
}

static void testExprArithmetic() {
  std::printf("testExprArithmetic\n");
  // gold=10, hasKey=1; evaluate (gold - 3) * 2 => 14, and (gold>5 && hasKey).
  iqtest::Bytes data = makeStory().build();
  inkkit::MemoryReader reader(data);
  StoryEngine engine;
  engine.open(reader);
  engine.setVariableValue(0, 10);
  engine.setVariableValue(1, 1);
  Expr e1 = Expr().pushVar(0).pushInt(3).op(OP_SUB).pushInt(2).op(OP_MUL);
  CHECK(engine.evaluate(e1.code) == 14);
  Expr e2 = Expr().pushVar(0).pushInt(5).op(OP_GT).pushVar(1).op(OP_AND);
  CHECK(engine.evaluate(e2.code) == 1);
  Expr e3 = Expr().pushVar(1).op(OP_NOT);
  CHECK(engine.evaluate(e3.code) == 0);
}

static void testBadFileRejected() {
  std::printf("testBadFileRejected\n");
  std::vector<uint8_t> junk = {'N', 'O', 'P', 'E', 1, 2, 3, 4};
  inkkit::MemoryReader reader(junk);
  StoryEngine engine;
  CHECK(engine.open(reader) == false);
  CHECK(engine.isOpen() == false);
}

int main() {
  testHeaderAndMeta();
  testKeyPath();
  testNoKeyPath();
  testConditionalTextHiddenWithoutKey();
  testSaveRestore();
  testSaveRejectsWrongStory();
  testExprArithmetic();
  testBadFileRejected();

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
