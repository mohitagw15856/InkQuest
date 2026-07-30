"""InkQuest companion: compile and validate interactive fiction into .iqs."""

from __future__ import annotations

__version__ = "0.1.0"

from .compilers import compile_ink_json, compile_twine
from .iqs import read_iqs, write_iqs
from .validate import validate_iqs, validate_story

__all__ = [
    "__version__",
    "compile_twine",
    "compile_ink_json",
    "write_iqs",
    "read_iqs",
    "validate_story",
    "validate_iqs",
]
