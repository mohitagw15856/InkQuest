"""Command line interface: ``inkquest compile`` and ``inkquest validate``."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from .compilers import compile_ink_json, compile_twine
from .errors import CompileError
from .ir import Story
from .iqs import read_iqs, write_iqs
from .validate import validate_iqs, validate_story

_TWINE_SUFFIXES = {".twee", ".tw", ".tws", ".html", ".htm"}
_INK_SUFFIXES = {".json", ".ink.json"}


def _detect_source_kind(path: Path, explicit: str | None) -> str:
    if explicit and explicit != "auto":
        return explicit
    suffix = path.suffix.lower()
    if suffix in _TWINE_SUFFIXES:
        return "twine"
    if suffix == ".json":
        return "ink"
    raise CompileError(
        f"cannot infer source format from {path.name!r}; pass --from twine|ink"
    )


def _compile_source(path: Path, kind: str, title: str | None, uid: str | None) -> Story:
    text = path.read_text(encoding="utf-8")
    if kind == "twine":
        return compile_twine(text, title=title, uid=uid)
    if kind == "ink":
        try:
            data = json.loads(text)
        except json.JSONDecodeError as exc:
            raise CompileError(f"input is not valid JSON: {exc}") from exc
        return compile_ink_json(data, title=title, uid=uid)
    raise CompileError(f"unknown source kind {kind!r}")


def _cmd_compile(args: argparse.Namespace) -> int:
    src = Path(args.input)
    if not src.exists():
        print(f"error: input file not found: {src}", file=sys.stderr)
        return 2
    kind = _detect_source_kind(src, args.from_)
    story = _compile_source(src, kind, args.title, args.uid)

    report = validate_story(story)
    if not report.ok and not args.force:
        print("error: story failed validation (use --force to compile anyway):", file=sys.stderr)
        print(report.format(), file=sys.stderr)
        return 1

    data = write_iqs(story)
    out = Path(args.output) if args.output else src.with_suffix(".iqs")
    out.write_bytes(data)
    print(f"compiled {story.uid!r}: {len(story.passages)} passages, "
          f"{len(story.variables)} variables -> {out} ({len(data)} bytes)")
    if report.dead_ends or report.unreachable:
        print("warning: validation issues remain (compiled with --force):")
        print(report.format())
    return 0


def _cmd_validate(args: argparse.Namespace) -> int:
    src = Path(args.input)
    if not src.exists():
        print(f"error: input file not found: {src}", file=sys.stderr)
        return 2

    if src.suffix.lower() == ".iqs":
        report = validate_iqs(read_iqs(src.read_bytes()))
    else:
        kind = _detect_source_kind(src, args.from_)
        story = _compile_source(src, kind, None, None)
        report = validate_story(story)

    print(report.format())
    return 0 if report.ok else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="inkquest",
        description="Compile and validate interactive fiction for the InkQuest reader.",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    compile_p = sub.add_parser("compile", help="compile a Twine or Ink story into .iqs")
    compile_p.add_argument("input", help="source file (.twee/.tw/.html or inklecate .json)")
    compile_p.add_argument("-o", "--output", help="output .iqs path (default: alongside input)")
    compile_p.add_argument("--from", dest="from_", choices=["auto", "twine", "ink"], default="auto",
                           help="source format (default: infer from extension)")
    compile_p.add_argument("--title", help="override the story title")
    compile_p.add_argument("--uid", help="override the story uid (save-slot identity)")
    compile_p.add_argument("--force", action="store_true", help="compile even if validation fails")
    compile_p.set_defaults(func=_cmd_compile)

    validate_p = sub.add_parser("validate", help="report dead-ends and unreachable passages")
    validate_p.add_argument("input", help="source file or compiled .iqs")
    validate_p.add_argument("--from", dest="from_", choices=["auto", "twine", "ink"], default="auto")
    validate_p.set_defaults(func=_cmd_validate)

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except CompileError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
