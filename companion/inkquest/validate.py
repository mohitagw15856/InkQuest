"""Structural validation: dead-end and unreachable-passage detection.

Works on the IR :class:`~inkquest.ir.Story` (used before writing an ``.iqs``) and
on an already-compiled :class:`~inkquest.iqs.IqsStory` (used when validating a
distributed file). Reachability follows every choice edge regardless of its
condition, which is the safe over-approximation: a passage reachable only when a
variable holds is still counted as reachable.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from .ir import Story
from .iqs import NO_PASSAGE, IqsStory


@dataclass
class Report:
    start: str = ""
    passage_count: int = 0
    unreachable: list[str] = field(default_factory=list)
    dead_ends: list[str] = field(default_factory=list)
    endings: list[str] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not self.unreachable and not self.dead_ends

    def format(self) -> str:
        lines = [
            f"start passage: {self.start}",
            f"passages: {self.passage_count}",
            f"endings: {len(self.endings)}",
        ]
        if self.unreachable:
            lines.append(f"unreachable passages ({len(self.unreachable)}):")
            lines += [f"  - {name}" for name in self.unreachable]
        if self.dead_ends:
            lines.append(f"dead-end passages ({len(self.dead_ends)}):")
            lines += [f"  - {name}" for name in self.dead_ends]
        if self.ok:
            lines.append("no problems found")
        return "\n".join(lines)


def validate_story(story: Story) -> Report:
    """Validate an IR story: build the passage graph and walk it from start."""
    names = [p.name for p in story.passages]
    start = story.start or (names[0] if names else "")

    edges: dict[str, list[str | None]] = {}
    for passage in story.passages:
        edges[passage.name] = [c.target for c in passage.choices]

    return _analyse(start, names, edges)


def validate_iqs(story: IqsStory) -> Report:
    """Validate a compiled story straight from its ``.iqs`` bytes."""
    passages = list(story.passages())
    names = [p.name for p in passages]
    start = names[story.start] if story.start < len(names) else ""

    edges: dict[str, list[str | None]] = {}
    for p in passages:
        targets: list[str | None] = []
        for c in p.choices:
            targets.append(None if c.target == NO_PASSAGE else names[c.target])
        edges[p.name] = targets
    return _analyse(start, names, edges)


def _analyse(start: str, names: list[str], edges: dict[str, list[str | None]]) -> Report:
    report = Report(start=start, passage_count=len(names))

    # Reachability from the start passage.
    reachable: set[str] = set()
    if start in edges:
        stack = [start]
        while stack:
            node = stack.pop()
            if node in reachable:
                continue
            reachable.add(node)
            for target in edges.get(node, []):
                if target is not None and target not in reachable:
                    stack.append(target)

    report.unreachable = sorted(n for n in names if n not in reachable)

    for name in names:
        targets = edges.get(name, [])
        if not targets:
            report.dead_ends.append(name)
        elif all(t is None for t in targets):
            report.endings.append(name)
    report.dead_ends.sort()
    report.endings.sort()
    return report
