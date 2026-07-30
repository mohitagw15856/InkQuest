"""Expression parsing and stack-bytecode compilation.

One small grammar serves both Twine dialects. It accepts Harlowe spellings
(``is``, ``is not``) and SugarCube / C style spellings (``==``, ``!=``, ``eq``,
``neq``, ``gt`` ...), plus ``and`` / ``or`` / ``not`` and the ``&&`` / ``||`` /
``!`` symbols. Only integers and booleans are supported; a float literal or an
unknown operator raises :class:`CompileError` so the author gets a clear message
rather than silently wrong output.

The emitted bytecode matches the opcodes in ``src/core/Iqs.h`` exactly and is
evaluated on the device by ``StoryEngine::evaluate``.
"""

from __future__ import annotations

import re

from .errors import CompileError
from .ir import Binary, Node, Num, Unary, Var

# Opcodes (must match src/core/Iqs.h).
OP_PUSH_I32 = 0x01
OP_PUSH_VAR = 0x02
OP_ADD = 0x10
OP_SUB = 0x11
OP_MUL = 0x12
OP_NEG = 0x13
OP_EQ = 0x20
OP_NE = 0x21
OP_LT = 0x22
OP_LE = 0x23
OP_GT = 0x24
OP_GE = 0x25
OP_AND = 0x30
OP_OR = 0x31
OP_NOT = 0x32

_BINOP_TO_OPCODE = {
    "+": OP_ADD,
    "-": OP_SUB,
    "*": OP_MUL,
    "==": OP_EQ,
    "!=": OP_NE,
    "<": OP_LT,
    "<=": OP_LE,
    ">": OP_GT,
    ">=": OP_GE,
    "and": OP_AND,
    "or": OP_OR,
}

# Word operators normalised to a canonical symbol.
_WORD_OPS = {
    "is": "==",
    "eq": "==",
    "neq": "!=",
    "ne": "!=",
    "gt": ">",
    "gte": ">=",
    "lt": "<",
    "lte": "<=",
    "and": "and",
    "or": "or",
    "not": "not",
    "to": "to",
}

_TOKEN_RE = re.compile(
    r"""
    \s*(?:
        (?P<var>\$?[A-Za-z_][A-Za-z0-9_]*)   # $var or bareword / keyword
      | (?P<num>-?\d+(?P<float>\.\d+)?)       # integer (float flagged for error)
      | (?P<op><=|>=|==|!=|&&|\|\||[-+*<>!()=])
    )
    """,
    re.VERBOSE,
)


class _Tok:
    __slots__ = ("kind", "value")

    def __init__(self, kind: str, value):
        self.kind = kind
        self.value = value

    def __repr__(self):  # pragma: no cover - debugging aid
        return f"{self.kind}:{self.value!r}"


def _tokenize(text: str) -> list[_Tok]:
    toks: list[_Tok] = []
    pos = 0
    n = len(text)
    while pos < n:
        if text[pos].isspace():
            pos += 1
            continue
        m = _TOKEN_RE.match(text, pos)
        if not m or m.start(0) == m.end(0):
            raise CompileError(f"cannot parse expression near {text[pos:pos + 12]!r}")
        pos = m.end(0)
        if m.group("float"):
            raise CompileError(f"floating point values are not supported: {m.group('num')!r}")
        if m.group("num") is not None:
            toks.append(_Tok("num", int(m.group("num"))))
        elif m.group("var") is not None:
            word = m.group("var")
            low = word.lower()
            if low in ("true", "false"):
                toks.append(_Tok("num", 1 if low == "true" else 0))
            elif low in _WORD_OPS:
                toks.append(_Tok("word", low))
            elif word.startswith("$"):
                toks.append(_Tok("var", word[1:]))
            else:
                # A bareword variable (SugarCube allows $ but Ink uses bare names).
                toks.append(_Tok("var", word))
        else:
            op = m.group("op")
            if op == "&&":
                op = "and"
            elif op == "||":
                op = "or"
            elif op == "!":
                op = "not"
            toks.append(_Tok("op", op))
    return toks


class _Parser:
    def __init__(self, toks: list[_Tok]):
        self.toks = toks
        self.i = 0

    def _peek(self) -> _Tok | None:
        return self.toks[self.i] if self.i < len(self.toks) else None

    def _next(self) -> _Tok | None:
        t = self._peek()
        if t is not None:
            self.i += 1
        return t

    def _is_word(self, *words: str) -> bool:
        t = self._peek()
        return t is not None and t.kind == "word" and t.value in words

    def _is_op(self, *ops: str) -> bool:
        t = self._peek()
        return t is not None and t.kind == "op" and t.value in ops

    def parse(self) -> Node:
        node = self._or()
        if self._peek() is not None:
            raise CompileError(f"unexpected trailing tokens in expression: {self.toks[self.i:]}")
        return node

    def _or(self) -> Node:
        node = self._and()
        while self._is_word("or") or self._is_op("or"):
            self._next()
            node = Binary("or", node, self._and())
        return node

    def _and(self) -> Node:
        node = self._not()
        while self._is_word("and") or self._is_op("and"):
            self._next()
            node = Binary("and", node, self._not())
        return node

    def _not(self) -> Node:
        if self._is_word("not") or self._is_op("not"):
            self._next()
            return Unary("not", self._not())
        return self._compare()

    def _compare(self) -> Node:
        node = self._add()
        t = self._peek()
        if t is None:
            return node
        op = None
        if t.kind == "op" and t.value in ("==", "!=", "<", "<=", ">", ">="):
            op = t.value
            self._next()
        elif t.kind == "op" and t.value == "=":  # SugarCube tolerates '=' as compare in <<if>>
            op = "=="
            self._next()
        elif t.kind == "word" and t.value == "is":
            self._next()
            if self._is_word("not"):
                self._next()
                op = "!="
            else:
                op = "=="
        elif t.kind == "word" and t.value in ("eq", "neq", "ne", "gt", "gte", "lt", "lte"):
            op = _WORD_OPS[t.value]
            self._next()
        if op is None:
            return node
        return Binary(op, node, self._add())

    def _add(self) -> Node:
        node = self._mul()
        while self._is_op("+", "-"):
            op = self._next().value
            node = Binary(op, node, self._mul())
        return node

    def _mul(self) -> Node:
        node = self._unary()
        while self._is_op("*"):
            self._next()
            node = Binary("*", node, self._unary())
        return node

    def _unary(self) -> Node:
        if self._is_op("-"):
            self._next()
            return Unary("neg", self._unary())
        return self._primary()

    def _primary(self) -> Node:
        t = self._next()
        if t is None:
            raise CompileError("unexpected end of expression")
        if t.kind == "num":
            return Num(t.value)
        if t.kind == "var":
            return Var(t.value)
        if t.kind == "op" and t.value == "(":
            node = self._or()
            close = self._next()
            if close is None or not (close.kind == "op" and close.value == ")"):
                raise CompileError("missing closing parenthesis in expression")
            return node
        raise CompileError(f"unexpected token in expression: {t.value!r}")


def parse_expression(text: str) -> Node:
    """Parse a boolean / arithmetic expression into an AST."""
    text = text.strip()
    if not text:
        raise CompileError("empty expression")
    toks = _tokenize(text)
    if not toks:
        raise CompileError("empty expression")
    return _Parser(toks).parse()


def variables_in(node: Node) -> set[str]:
    """Collect every variable name referenced by an expression tree."""
    if isinstance(node, Var):
        return {node.name}
    if isinstance(node, Unary):
        return variables_in(node.operand)
    if isinstance(node, Binary):
        return variables_in(node.left) | variables_in(node.right)
    return set()


def _emit_i32(value: int) -> bytes:
    return int(value & 0xFFFFFFFF).to_bytes(4, "little")


def compile_expr(node: Node, var_index: dict[str, int]) -> bytes:
    """Compile an expression AST to postfix stack bytecode."""
    out = bytearray()

    def emit(n: Node) -> None:
        if isinstance(n, Num):
            out.append(OP_PUSH_I32)
            out.extend(_emit_i32(n.value))
        elif isinstance(n, Var):
            if n.name not in var_index:
                raise CompileError(f"unknown variable ${n.name} used in expression")
            out.append(OP_PUSH_VAR)
            out.extend(int(var_index[n.name]).to_bytes(2, "little"))
        elif isinstance(n, Unary):
            emit(n.operand)
            out.append(OP_NEG if n.op == "neg" else OP_NOT)
        elif isinstance(n, Binary):
            emit(n.left)
            emit(n.right)
            out.append(_BINOP_TO_OPCODE[n.op])
        else:  # pragma: no cover - exhaustive
            raise CompileError(f"cannot compile expression node {n!r}")

    emit(node)
    return bytes(out)
