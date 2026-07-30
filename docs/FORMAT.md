# The InkQuest story format (`.iqs`) and related formats

This document is the byte level specification of the compiled story format the
companion writes and the device reads, the save format, the expression bytecode,
and the supported subsets of Twine and Ink. It is the shared contract between
`companion/inkquest/iqs.py` and `src/core/StoryEngine.cpp`; keep both in step
with any change here, and bump the format version.

All integers are little endian. A `String` is a `uint16` byte length followed by
that many UTF-8 bytes (no terminator). A `Bytecode` blob is a `uint16` byte
length followed by that many bytes.

## `.iqs` layout

### Header (32 bytes, at offset 0)

| Offset | Type      | Field            | Notes                                   |
|-------:|-----------|------------------|-----------------------------------------|
| 0      | char[4]   | magic            | ASCII `IQS1`                            |
| 4      | uint8     | formatVersion    | currently `1`                           |
| 5      | uint8     | flags            | bit 0: story ships a cover BMP          |
| 6      | uint16    | passageCount     |                                         |
| 8      | uint16    | variableCount    |                                         |
| 10     | uint16    | startPassage     | index of the first passage              |
| 12     | uint16    | reserved         | `0`                                     |
| 14     | uint32    | metaOffset       | absolute offset of the meta section     |
| 18     | uint32    | varTableOffset   | absolute offset of the variable table   |
| 22     | uint32    | passageLutOffset | absolute offset of the passage LUT      |
| 26     | uint32    | reserved         | `0`                                     |
| 30     | uint16    | padding          | `0`, brings the header to 32 bytes      |

Sections are addressed by absolute offset, so a reader seeks rather than assuming
a fixed order. The writer lays them out as: header, meta, variable table,
passage records, passage LUT.

### Meta section (at `metaOffset`)

Four strings in order: `title`, `author`, `uid`, `coverPath`. The `uid` is the
stable identifier used for the save directory. `coverPath` is a path relative to
`/inkquest/stories`, or empty when there is no cover.

### Variable table (at `varTableOffset`)

`variableCount` records, each:

| Type   | Field   | Notes                                        |
|--------|---------|----------------------------------------------|
| String | name    |                                              |
| int32  | initial | value at story start                         |
| uint8  | type    | 0 int, 1 bool, 2 flag (all stored as int32)  |

Variable *index* is the position in this table; conditions and effects refer to
variables by index.

### Passage lookup table (at `passageLutOffset`)

`passageCount` `uint32` absolute offsets, one per passage, in passage index
order. To load a passage the reader seeks to `LUT[index]`.

### Passage record (at `LUT[index]`)

| Type              | Field       | Notes                                  |
|-------------------|-------------|----------------------------------------|
| String            | name        |                                        |
| Setters           | onEnter      | applied when the passage is entered   |
| uint16            | bodyCount   |                                        |
| BodyPart[bodyCount] | body       | see below                              |
| uint16            | choiceCount |                                        |
| Choice[choiceCount] | choices    | see below                              |

**BodyPart**

| Type     | Field | Notes                                         |
|----------|-------|-----------------------------------------------|
| uint8    | kind  | 0 always shown, 1 conditional                 |
| Bytecode | cond  | present only when kind is 1                    |
| String   | text  |                                                |

**Choice**

| Type     | Field       | Notes                                    |
|----------|-------------|------------------------------------------|
| String   | text        | the label shown to the reader            |
| uint16   | target      | passage index, or `0xFFFF` to end        |
| uint8    | hasCond     | 0 or 1                                    |
| Bytecode | cond        | present only when hasCond is 1           |
| Setters  | effects     | applied when the choice is taken         |

**Setters** is a `uint16` count followed by that many:

| Type     | Field | Notes                                    |
|----------|-------|------------------------------------------|
| uint16   | var   | variable index                           |
| uint8    | op    | 0 `=`, 1 `+=`, 2 `-=`, 3 `*=`            |
| Bytecode | expr  | right hand side expression               |

## Expression and condition bytecode

Expressions and conditions are compiled to postfix stack bytecode, evaluated on a
small integer stack. A non zero result is true. Each opcode is one byte followed
by any inline operand.

| Opcode | Name      | Operand      | Stack effect            |
|-------:|-----------|--------------|-------------------------|
| 0x01   | PUSH_I32  | int32        | push literal            |
| 0x02   | PUSH_VAR  | uint16 index | push variable value     |
| 0x10   | ADD       |              | a b -> a + b            |
| 0x11   | SUB       |              | a b -> a - b            |
| 0x12   | MUL       |              | a b -> a * b            |
| 0x13   | NEG       |              | a -> -a                 |
| 0x20   | EQ        |              | a b -> a == b           |
| 0x21   | NE        |              | a b -> a != b           |
| 0x22   | LT        |              | a b -> a < b            |
| 0x23   | LE        |              | a b -> a <= b           |
| 0x24   | GT        |              | a b -> a > b            |
| 0x25   | GE        |              | a b -> a >= b           |
| 0x30   | AND       |              | a b -> a && b           |
| 0x31   | OR        |              | a b -> a || b           |
| 0x32   | NOT       |              | a -> !a                 |

An unknown opcode fails closed (evaluates to 0). Booleans are `0` and `1`.

## Save format (`.iqv`)

Saves live at `/inkquest/saves/<uid>/auto.iqv` (slot 0, the autosave) and
`/inkquest/saves/<uid>/slotN.iqv` (manual slots 1..N).

| Type   | Field          | Notes                              |
|--------|----------------|------------------------------------|
| char[4]| magic          | ASCII `IQV1`                       |
| uint8  | version        | currently `1`                      |
| uint8  | flags          | bit 0: autosave                    |
| uint32 | timestamp      | epoch seconds, `0` if unknown      |
| uint16 | currentPassage |                                    |
| String | storyUid       | must match the story               |
| uint16 | varCount       | must match the story               |
| int32[varCount] | values | variable state                    |

Restore fails (and leaves the engine untouched) if the magic, version, uid or
variable count do not match, so a slot cannot be applied to the wrong story or a
changed build.

## Cover images

Covers are small uncompressed Windows BMP files, 1 bpp or 24 bpp, pre processed
on the desktop to the target thumbnail size. The device decodes them one padded
row at a time. Pixels darker than mid grey are inked; lighter pixels leave the
page background. Keep covers small; the display is monochrome.

## Supported Twine subset

The companion compiles Twee 3 notation (`:: Passage` headers) and published
Twine HTML (`<tw-storydata>` / `<tw-passagedata>`), in both the Harlowe and
SugarCube macro dialects. The supported subset is:

- **Links**, all four notations: `[[Target]]`, `[[Display->Target]]`,
  `[[Target<-Display]]`, `[[Display|Target]]`. A link to the reserved target
  `END` (or an empty target) ends the story.
- **Variables and assignment** at the top of a passage: Harlowe
  `(set: $v to EXPR)` and `(set: $v to it + 1)` style compound forms via `+=`,
  `-=`, `*=`; SugarCube `<<set $v = EXPR>>`, `<<set $v to EXPR>>`, `<<set $v += EXPR>>`.
  A startup passage (SugarCube `StoryInit`, or a Harlowe passage tagged `startup`)
  has its assignments run at story start.
- **Conditionals**: Harlowe `(if:)`, `(else-if:)`, `(else:)`, `(unless:)` hooks;
  SugarCube `<<if>>` / `<<elseif>>` / `<<else>>` / `<</if>>`. A link inside a
  conditional becomes a conditional choice; text inside a conditional becomes
  conditional body text.
- **Expressions**: integers and booleans, `+ - *`, comparisons (`is`, `is not`,
  `==`, `!=`, `<`, `<=`, `>`, `>=` and the word forms `eq`, `neq`, `gt`, ...),
  and `and` / `or` / `not` (with `&&` / `||` / `!`).

Anything outside the subset (dynamic macros such as `(print:)`, loops, a `(set:)`
inside a conditional, floating point, unknown macros) raises a clear compile
error naming the passage and the construct.

## Supported Ink subset

The companion reads inklecate JSON (`inklecate -j story.ink`) and reconstructs a
graph from a documented subset of the runtime container tree:

- `VAR` declarations with integer initial values (from the `global decl`
  container);
- knots (and the root's leading content, as an implicit `Start`) become passages;
- `^text` output becomes body text and `\n` a line break;
- choices (`{"*": path}`) resolve against the knot's `c-N` sub containers, whose
  `^` text is the label and `->` divert the target;
- a plain trailing `->` divert becomes an automatic "Continue" choice, and a knot
  that runs out of flow becomes a natural ending.

Threads, tunnels, functions, LISTs, glue driven assembly, variable printing in
text, and conditional or looping content are outside the subset and raise a clear
compile error. Choice pointers are resolved by their final path component. The
subset is validated against representative fixtures that follow the inklecate
layout; when wiring in a real inklecate build, re run `inkquest validate` on the
output.
