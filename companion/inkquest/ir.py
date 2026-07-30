"""Intermediate representation shared by every front-end compiler.

Twine and Ink stories are parsed into this dialect-neutral graph, which the
validator inspects and the writer (:mod:`inkquest.iqs`) serialises into the
compiled ``.iqs`` binary. Keeping the IR small and explicit means a new source
format only has to target these dataclasses.
"""

from __future__ import annotations

from dataclasses import dataclass, field

# ---------------------------------------------------------------------------
# Expression AST. Produced by inkquest.expr and consumed by inkquest.iqs when
# compiling to stack bytecode. Kept as plain nodes so it is trivially testable.
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Num:
    value: int


@dataclass(frozen=True)
class Var:
    name: str


@dataclass(frozen=True)
class Unary:
    op: str  # 'neg' | 'not'
    operand: "Node"


@dataclass(frozen=True)
class Binary:
    op: str  # + - * == != < <= > >= and or
    left: "Node"
    right: "Node"


Node = Num | Var | Unary | Binary


# ---------------------------------------------------------------------------
# Story graph.
# ---------------------------------------------------------------------------


@dataclass
class VarDecl:
    name: str
    initial: int = 0
    type: str = "int"  # 'int' | 'bool' | 'flag'


@dataclass
class Setter:
    var: str
    op: str  # '=' | '+=' | '-=' | '*='
    expr: Node


@dataclass
class BodyPart:
    text: str
    cond: Node | None = None


@dataclass
class Choice:
    text: str
    target: str | None  # passage name, or None to end the story
    cond: Node | None = None
    effects: list[Setter] = field(default_factory=list)


@dataclass
class Passage:
    name: str
    body: list[BodyPart] = field(default_factory=list)
    choices: list[Choice] = field(default_factory=list)
    on_enter: list[Setter] = field(default_factory=list)

    def plain_text(self) -> str:
        """Concatenation of every body run, ignoring conditions (for previews)."""
        return "".join(part.text for part in self.body)


@dataclass
class Story:
    title: str = "Untitled"
    author: str = ""
    uid: str = "story"
    start: str = ""
    cover: str = ""
    variables: list[VarDecl] = field(default_factory=list)
    passages: list[Passage] = field(default_factory=list)

    def passage_names(self) -> set[str]:
        return {p.name for p in self.passages}

    def var_names(self) -> set[str]:
        return {v.name for v in self.variables}

    def find(self, name: str) -> Passage | None:
        for passage in self.passages:
            if passage.name == name:
                return passage
        return None
