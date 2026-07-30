"""Reader and writer for the compiled ``.iqs`` story format.

This is the Python side of the same contract implemented in C++ by
``src/core/StoryEngine.cpp``. The byte layout is documented in
``docs/FORMAT.md``; the two implementations are kept in lock-step and the
cross-language fixture test in ``companion/tests`` plus the C++ host tests both
exercise it.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field

from .errors import CompileError
from .expr import compile_expr
from .ir import Story

MAGIC = b"IQS1"
FORMAT_VERSION = 1
HEADER_SIZE = 32
NO_PASSAGE = 0xFFFF

FLAG_HAS_COVER = 1 << 0

_SET_OP = {"=": 0, "+=": 1, "-=": 2, "*=": 3}
_VAR_TYPE = {"int": 0, "bool": 1, "flag": 2}
_VAR_TYPE_REV = {v: k for k, v in _VAR_TYPE.items()}


def _u16(v: int) -> bytes:
    return struct.pack("<H", v)


def _u32(v: int) -> bytes:
    return struct.pack("<I", v)


def _i32(v: int) -> bytes:
    return struct.pack("<i", v)


def _string(s: str) -> bytes:
    data = s.encode("utf-8")
    if len(data) > 0xFFFF:
        raise CompileError(f"string too long to serialise ({len(data)} bytes): {s[:32]!r}...")
    return _u16(len(data)) + data


# ---------------------------------------------------------------------------
# Writer
# ---------------------------------------------------------------------------


def write_iqs(story: Story) -> bytes:
    """Serialise a validated :class:`~inkquest.ir.Story` into ``.iqs`` bytes."""
    if not story.passages:
        raise CompileError("story has no passages")

    name_to_index = {p.name: i for i, p in enumerate(story.passages)}
    if len(name_to_index) != len(story.passages):
        raise CompileError("duplicate passage names are not allowed")

    var_index = {v.name: i for i, v in enumerate(story.variables)}

    start_name = story.start or story.passages[0].name
    if start_name not in name_to_index:
        raise CompileError(f"start passage {start_name!r} does not exist")

    def encode_setters(setters) -> bytes:
        out = bytearray(_u16(len(setters)))
        for s in setters:
            if s.var not in var_index:
                raise CompileError(f"assignment to unknown variable ${s.var}")
            if s.op not in _SET_OP:
                raise CompileError(f"unknown assignment operator {s.op!r}")
            code = compile_expr(s.expr, var_index)
            out += _u16(var_index[s.var]) + bytes([_SET_OP[s.op]]) + _u16(len(code)) + code
        return bytes(out)

    def target_index(name: str | None) -> int:
        if name is None:
            return NO_PASSAGE
        if name not in name_to_index:
            raise CompileError(f"choice points at unknown passage {name!r}")
        return name_to_index[name]

    # Meta section.
    meta = _string(story.title) + _string(story.author) + _string(story.uid) + _string(story.cover)

    # Variable table.
    var_table = bytearray()
    for v in story.variables:
        var_table += _string(v.name) + _i32(v.initial) + bytes([_VAR_TYPE.get(v.type, 0)])

    # Passage blobs.
    passage_blobs: list[bytes] = []
    for p in story.passages:
        pb = bytearray()
        pb += _string(p.name)
        pb += encode_setters(p.on_enter)
        pb += _u16(len(p.body))
        for part in p.body:
            if part.cond is not None:
                code = compile_expr(part.cond, var_index)
                pb += bytes([1]) + _u16(len(code)) + code
            else:
                pb += bytes([0])
            pb += _string(part.text)
        pb += _u16(len(p.choices))
        for c in p.choices:
            pb += _string(c.text)
            pb += _u16(target_index(c.target))
            if c.cond is not None:
                code = compile_expr(c.cond, var_index)
                pb += bytes([1]) + _u16(len(code)) + code
            else:
                pb += bytes([0])
            pb += encode_setters(c.effects)
        passage_blobs.append(bytes(pb))

    meta_offset = HEADER_SIZE
    var_table_offset = meta_offset + len(meta)
    first_passage_offset = var_table_offset + len(var_table)

    lut = []
    cursor = first_passage_offset
    for pb in passage_blobs:
        lut.append(cursor)
        cursor += len(pb)
    passage_lut_offset = cursor

    flags = FLAG_HAS_COVER if story.cover else 0

    header = bytearray()
    header += MAGIC
    header += bytes([FORMAT_VERSION, flags])
    header += _u16(len(story.passages))
    header += _u16(len(story.variables))
    header += _u16(name_to_index[start_name])
    header += _u16(0)  # reserved16
    header += _u32(meta_offset)
    header += _u32(var_table_offset)
    header += _u32(passage_lut_offset)
    header += _u32(0)  # reserved32
    header += b"\x00" * (HEADER_SIZE - len(header))  # pad to header size

    out = bytearray(header)
    out += meta
    out += var_table
    for pb in passage_blobs:
        out += pb
    for off in lut:
        out += _u32(off)
    return bytes(out)


# ---------------------------------------------------------------------------
# Reader + a small runtime, used by validate and the tests. This mirrors
# StoryEngine on the device so the same story plays identically off-device.
# ---------------------------------------------------------------------------


@dataclass
class RSetter:
    var: int
    op: int
    expr: bytes


@dataclass
class RBody:
    text: str
    cond: bytes | None


@dataclass
class RChoice:
    text: str
    target: int
    cond: bytes | None
    effects: list[RSetter] = field(default_factory=list)


@dataclass
class RPassage:
    name: str
    on_enter: list[RSetter]
    body: list[RBody]
    choices: list[RChoice]


class _Cursor:
    def __init__(self, data: bytes, pos: int = 0):
        self.data = data
        self.pos = pos

    def u8(self) -> int:
        v = self.data[self.pos]
        self.pos += 1
        return v

    def u16(self) -> int:
        v = struct.unpack_from("<H", self.data, self.pos)[0]
        self.pos += 2
        return v

    def u32(self) -> int:
        v = struct.unpack_from("<I", self.data, self.pos)[0]
        self.pos += 4
        return v

    def i32(self) -> int:
        v = struct.unpack_from("<i", self.data, self.pos)[0]
        self.pos += 4
        return v

    def string(self) -> str:
        n = self.u16()
        s = self.data[self.pos : self.pos + n].decode("utf-8")
        self.pos += n
        return s

    def blob(self) -> bytes:
        n = self.u16()
        b = self.data[self.pos : self.pos + n]
        self.pos += n
        return b


class IqsStory:
    """A parsed ``.iqs`` file with a reference runtime for off-device replay."""

    def __init__(self, data: bytes):
        if data[:4] != MAGIC:
            raise CompileError("not an .iqs file (bad magic)")
        c = _Cursor(data, 4)
        version = c.u8()
        if version != FORMAT_VERSION:
            raise CompileError(f"unsupported .iqs version {version}")
        self.flags = c.u8()
        self.passage_count = c.u16()
        var_count = c.u16()
        self.start = c.u16()
        c.u16()  # reserved
        meta_off = c.u32()
        var_off = c.u32()
        lut_off = c.u32()

        m = _Cursor(data, meta_off)
        self.title = m.string()
        self.author = m.string()
        self.uid = m.string()
        self.cover = m.string()

        v = _Cursor(data, var_off)
        self.var_names: list[str] = []
        self.var_initial: list[int] = []
        self.var_types: list[str] = []
        for _ in range(var_count):
            self.var_names.append(v.string())
            self.var_initial.append(v.i32())
            self.var_types.append(_VAR_TYPE_REV.get(v.u8(), "int"))

        lut = _Cursor(data, lut_off)
        self._offsets = [lut.u32() for _ in range(self.passage_count)]
        self._data = data

    # -- structural access -------------------------------------------------

    def passage(self, index: int) -> RPassage:
        c = _Cursor(self._data, self._offsets[index])
        name = c.string()
        on_enter = self._read_setters(c)
        body: list[RBody] = []
        for _ in range(c.u16()):
            kind = c.u8()
            cond = c.blob() if kind == 1 else None
            body.append(RBody(c.string(), cond))
        choices: list[RChoice] = []
        for _ in range(c.u16()):
            text = c.string()
            target = c.u16()
            has_cond = c.u8()
            cond = c.blob() if has_cond else None
            effects = self._read_setters(c)
            choices.append(RChoice(text, target, cond, effects))
        return RPassage(name, on_enter, body, choices)

    @staticmethod
    def _read_setters(c: _Cursor) -> list[RSetter]:
        setters = []
        for _ in range(c.u16()):
            var = c.u16()
            op = c.u8()
            setters.append(RSetter(var, op, c.blob()))
        return setters

    def passages(self):
        for i in range(self.passage_count):
            yield self.passage(i)

    # -- reference runtime -------------------------------------------------

    def new_state(self) -> "IqsRuntime":
        return IqsRuntime(self)


class IqsRuntime:
    """Reference VM mirroring StoryEngine::evaluate and the choose/enter loop."""

    def __init__(self, story: IqsStory):
        self.story = story
        self.values = list(story.var_initial)
        self.current = story.start

    def evaluate(self, code: bytes) -> int:
        stack: list[int] = []
        i = 0
        n = len(code)
        while i < n:
            op = code[i]
            i += 1
            if op == 0x01:  # PUSH_I32
                stack.append(struct.unpack_from("<i", code, i)[0])
                i += 4
            elif op == 0x02:  # PUSH_VAR
                idx = struct.unpack_from("<H", code, i)[0]
                i += 2
                stack.append(self.values[idx] if idx < len(self.values) else 0)
            elif op == 0x10:
                b, a = stack.pop(), stack.pop()
                stack.append(a + b)
            elif op == 0x11:
                b, a = stack.pop(), stack.pop()
                stack.append(a - b)
            elif op == 0x12:
                b, a = stack.pop(), stack.pop()
                stack.append(a * b)
            elif op == 0x13:
                stack.append(-stack.pop())
            elif op == 0x20:
                b, a = stack.pop(), stack.pop()
                stack.append(int(a == b))
            elif op == 0x21:
                b, a = stack.pop(), stack.pop()
                stack.append(int(a != b))
            elif op == 0x22:
                b, a = stack.pop(), stack.pop()
                stack.append(int(a < b))
            elif op == 0x23:
                b, a = stack.pop(), stack.pop()
                stack.append(int(a <= b))
            elif op == 0x24:
                b, a = stack.pop(), stack.pop()
                stack.append(int(a > b))
            elif op == 0x25:
                b, a = stack.pop(), stack.pop()
                stack.append(int(a >= b))
            elif op == 0x30:
                b, a = stack.pop(), stack.pop()
                stack.append(int(bool(a) and bool(b)))
            elif op == 0x31:
                b, a = stack.pop(), stack.pop()
                stack.append(int(bool(a) or bool(b)))
            elif op == 0x32:
                stack.append(int(stack.pop() == 0))
            else:
                return 0
        return stack.pop() if stack else 0

    def _apply(self, setters: list[RSetter]) -> None:
        for s in setters:
            rhs = self.evaluate(s.expr)
            if s.op == 0:
                self.values[s.var] = rhs
            elif s.op == 1:
                self.values[s.var] += rhs
            elif s.op == 2:
                self.values[s.var] -= rhs
            elif s.op == 3:
                self.values[s.var] *= rhs

    def enter(self, index: int) -> RPassage:
        p = self.story.passage(index)
        self.current = index
        self._apply(p.on_enter)
        return p

    def visible_text(self, passage: RPassage) -> str:
        out = []
        for part in passage.body:
            if part.cond is not None and self.evaluate(part.cond) == 0:
                continue
            out.append(part.text)
        return "".join(out)

    def choice_visible(self, choice: RChoice) -> bool:
        return choice.cond is None or self.evaluate(choice.cond) != 0

    def choose(self, choice: RChoice) -> RPassage | None:
        self._apply(choice.effects)
        if choice.target == NO_PASSAGE:
            self.current = NO_PASSAGE
            return None
        return self.enter(choice.target)


def read_iqs(data: bytes) -> IqsStory:
    return IqsStory(data)
