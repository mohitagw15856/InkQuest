"""Error types shared across the InkQuest companion."""

from __future__ import annotations


class CompileError(Exception):
    """A story could not be compiled.

    Raised with a human readable message that names the offending passage and,
    where possible, the unsupported construct. The CLI prints these without a
    traceback so authors get a clear, actionable message.
    """

    def __init__(self, message: str, passage: str | None = None) -> None:
        self.passage = passage
        if passage:
            message = f"[{passage}] {message}"
        super().__init__(message)


class ValidationError(Exception):
    """A story compiled but failed a structural validation check."""
