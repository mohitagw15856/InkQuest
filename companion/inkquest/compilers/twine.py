"""Twine front-end: compile a Twine story into the InkQuest IR.

Two source shapes are accepted:

* **Twee 3** notation (``:: Passage`` headers), the format the bundled demo ships
  in and the easiest to hand-author or export from Tweego / the Twine app.
* **Published Twine HTML** containing ``<tw-storydata>`` / ``<tw-passagedata>``
  elements, as produced by "Publish to File".

Both Harlowe and SugarCube macro dialects are supported for the subset a
choose-your-own-adventure needs: variable assignment, conditional blocks and the
four link notations. Anything outside that subset raises
:class:`~inkquest.errors.CompileError` naming the passage and the construct, so
the author learns exactly what to change rather than getting silent misbehaviour.
"""

from __future__ import annotations

import html as html_module
import json
import re
from html.parser import HTMLParser

from ..errors import CompileError
from ..expr import parse_expression, variables_in
from ..ir import Binary, BodyPart, Choice, Node, Num, Passage, Setter, Story, Unary, Var, VarDecl

# Passages that configure the story rather than being played.
_TITLE_PASSAGE = "StoryTitle"
_DATA_PASSAGE = "StoryData"
_INIT_PASSAGES = {"StoryInit"}  # SugarCube init; merged into the start passage
_INIT_TAGS = {"startup", "init"}  # Harlowe convention

_LINK_RE = re.compile(r"\[\[(.*?)\]\]", re.DOTALL)


def slugify(text: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return slug or "story"


# ---------------------------------------------------------------------------
# Parsing the container format into raw passages.
# ---------------------------------------------------------------------------


class _RawPassage:
    def __init__(self, name: str, tags: list[str], body: str):
        self.name = name
        self.tags = tags
        self.body = body


def _parse_twee(text: str) -> list[_RawPassage]:
    passages: list[_RawPassage] = []
    current_header: str | None = None
    current_lines: list[str] = []

    def flush():
        if current_header is None:
            return
        name, tags = _split_header(current_header)
        passages.append(_RawPassage(name, tags, "\n".join(current_lines).strip("\n")))

    for line in text.splitlines():
        if line.startswith("::") and not line.startswith(":::"):
            flush()
            current_header = line[2:].strip()
            current_lines = []
        elif current_header is not None:
            current_lines.append(line)
    flush()
    return passages


def _split_header(header: str) -> tuple[str, list[str]]:
    # "Name [tag1 tag2] {"position":...}"  -> ("Name", ["tag1", "tag2"])
    header = re.sub(r"\{.*?\}\s*$", "", header).strip()
    tags: list[str] = []
    m = re.search(r"\[(.*?)\]\s*$", header)
    if m:
        tags = m.group(1).split()
        header = header[: m.start()].strip()
    return header.replace("\\[", "[").replace("\\]", "]"), tags


class _TwStoryParser(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.passages: list[_RawPassage] = []
        self.story_name = ""
        self.start_pid = ""
        self._pid_to_name: dict[str, str] = {}
        self._in_passage = False
        self._cur_attrs: dict[str, str] = {}
        self._cur_text: list[str] = []

    def handle_starttag(self, tag, attrs):
        a = {k: (v or "") for k, v in attrs}
        if tag == "tw-storydata":
            self.story_name = a.get("name", "")
            self.start_pid = a.get("startnode", "")
        elif tag == "tw-passagedata":
            self._in_passage = True
            self._cur_attrs = a
            self._cur_text = []

    def handle_endtag(self, tag):
        if tag == "tw-passagedata" and self._in_passage:
            name = self._cur_attrs.get("name", "")
            pid = self._cur_attrs.get("pid", "")
            tags = self._cur_attrs.get("tags", "").split()
            body = html_module.unescape("".join(self._cur_text))
            self.passages.append(_RawPassage(name, tags, body.strip("\n")))
            if pid:
                self._pid_to_name[pid] = name
            self._in_passage = False

    def handle_data(self, data):
        if self._in_passage:
            self._cur_text.append(data)


def _parse_html(text: str) -> tuple[list[_RawPassage], str, str]:
    parser = _TwStoryParser()
    parser.feed(text)
    start_name = parser._pid_to_name.get(parser.start_pid, "")
    return parser.passages, parser.story_name, start_name


# ---------------------------------------------------------------------------
# Dialect + macro handling.
# ---------------------------------------------------------------------------


def _detect_dialect(bodies: str) -> str:
    if re.search(r"<<\s*(if|set|print|else|for|link)", bodies):
        return "sugarcube"
    return "harlowe"


def _and(a: Node | None, b: Node | None) -> Node | None:
    if a is None:
        return b
    if b is None:
        return a
    return Binary("and", a, b)


def _not(node: Node) -> Node:
    return Unary("not", node)


class _Segment:
    """A run of passage source carrying the condition it appears under."""

    def __init__(self, cond: Node | None, text: str):
        self.cond = cond
        self.text = text


def _macro_at(source: str, i: int) -> tuple[str, str, int] | None:
    """Parse a Harlowe ``(name: args)`` macro starting at ``source[i] == '('``.

    Returns ``(name, args, end)`` where ``end`` is just past the closing paren,
    or ``None`` if this is not a macro head.
    """
    m = re.match(r"\((else-if|elseif|if|unless|else|set|print|[a-zA-Z-]+)\s*:?", source[i:])
    if not m:
        return None
    name = m.group(1)
    # Find the matching close paren, honouring nested parens and strings.
    depth = 0
    j = i
    in_str = None
    while j < len(source):
        c = source[j]
        if in_str:
            if c == in_str:
                in_str = None
        elif c in "\"'":
            in_str = c
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                args = source[i + 1 + len(name) : j]
                args = args.lstrip()
                if args.startswith(":"):
                    args = args[1:]
                return name, args.strip(), j + 1
        j += 1
    raise CompileError(f"unclosed macro '({name}...' in passage")


def _hook_at(source: str, i: int) -> tuple[str, int] | None:
    """Match a Harlowe hook ``[ ... ]`` at ``source[i] == '['``.

    Depth counts individual brackets; well-formed ``[[links]]`` stay balanced.
    """
    if i >= len(source) or source[i] != "[":
        return None
    depth = 0
    j = i
    while j < len(source):
        c = source[j]
        if c == "[":
            depth += 1
        elif c == "]":
            depth -= 1
            if depth == 0:
                return source[i + 1 : j], j + 1
        j += 1
    raise CompileError("unclosed hook '[' in passage")


def _parse_condition(name: str, args: str, passage: str) -> Node:
    if name in ("if", "elseif", "else-if"):
        return parse_expression(args)
    if name == "unless":
        return _not(parse_expression(args))
    raise CompileError(f"unexpected conditional macro '{name}'", passage)


def _segment_harlowe(source: str, cond: Node | None, passage: str) -> list[_Segment]:
    segments: list[_Segment] = []
    i = 0
    n = len(source)
    text_start = 0

    def flush_text(end: int):
        if end > text_start:
            segments.append(_Segment(cond, source[text_start:end]))

    while i < n:
        if source[i] == "(":
            macro = _macro_at(source, i)
            if macro and macro[0] in ("if", "unless"):
                flush_text(i)
                i = _consume_harlowe_chain(source, i, cond, passage, segments)
                text_start = i
                continue
        i += 1
    flush_text(n)
    return segments


def _consume_harlowe_chain(source, i, cond, passage, segments) -> int:
    """Consume an if/else-if/else chain starting at ``i`` and append segments."""
    branch_conds: list[Node] = []
    accumulated_neg: Node | None = None
    while i < len(source) and source[i] == "(":
        macro = _macro_at(source, i)
        if not macro:
            break
        name, args, end = macro
        if name in ("if", "unless"):
            branch = _parse_condition(name, args, passage)
            effective = branch
            accumulated_neg = _not(branch)
        elif name in ("elseif", "else-if"):
            branch = _parse_condition(name, args, passage)
            effective = _and(accumulated_neg, branch)
            accumulated_neg = _and(accumulated_neg, _not(branch))
        elif name == "else":
            effective = accumulated_neg
        else:
            break
        hook = _hook_at(source, end)
        if hook is None:
            raise CompileError(f"conditional macro '({name}:...)' is not followed by a [hook]", passage)
        hook_text, hook_end = hook
        segments.extend(_segment_harlowe(hook_text, _and(cond, effective), passage))
        branch_conds.append(effective)
        # Skip whitespace and look for a continuation (else / else-if).
        k = hook_end
        while k < len(source) and source[k] in " \t\r\n":
            k += 1
        peek = _macro_at(source, k) if k < len(source) and source[k] == "(" else None
        if peek and peek[0] in ("elseif", "else-if", "else"):
            i = k
            continue
        return hook_end
    return i


# SugarCube: split the body on <<...>> markers and walk the resulting stream.
_SC_MARKER_RE = re.compile(r"<<\s*(/?)(\w+)\s*(.*?)>>", re.DOTALL)


def _segment_sugarcube(source: str, passage: str) -> list[_Segment]:
    tokens: list[tuple] = []  # ('text', s) | ('macro', close, name, args)
    pos = 0
    for m in _SC_MARKER_RE.finditer(source):
        if m.start() > pos:
            tokens.append(("text", source[pos : m.start()]))
        tokens.append(("macro", m.group(1) == "/", m.group(2).lower(), m.group(3).strip()))
        pos = m.end()
    if pos < len(source):
        tokens.append(("text", source[pos:]))

    segments: list[_Segment] = []
    idx = 0

    def walk(cond: Node | None, stop_at) -> None:
        nonlocal idx
        while idx < len(tokens):
            tok = tokens[idx]
            if tok[0] == "text":
                segments.append(_Segment(cond, tok[1]))
                idx += 1
            else:
                _, close, name, args = tok
                if close and name in stop_at:
                    return
                if name in ("elseif", "else") and name in stop_at:
                    return
                if name == "if":
                    idx += 1
                    _walk_if(cond, parse_expression(args))
                elif name == "set":
                    segments.append(_Segment(cond, f"<<set {args}>>"))
                    idx += 1
                elif name in ("print", "=", "for", "link", "-"):
                    raise CompileError(f"unsupported SugarCube macro '<<{name}>>'", passage)
                else:
                    raise CompileError(f"unsupported SugarCube macro '<<{name}>>'", passage)

    def _walk_if(outer: Node | None, first_cond: Node) -> None:
        nonlocal idx
        accumulated_neg: Node | None = None
        branch_cond = first_cond
        accumulated_neg = _not(first_cond)
        while True:
            walk(_and(outer, branch_cond), stop_at={"if", "elseif", "else"})
            if idx >= len(tokens):
                raise CompileError("unclosed <<if>> block", passage)
            _, close, name, args = tokens[idx]
            if close and name == "if":
                idx += 1
                return
            if name == "elseif":
                idx += 1
                branch = parse_expression(args)
                branch_cond = _and(accumulated_neg, branch)
                accumulated_neg = _and(accumulated_neg, _not(branch))
            elif name == "else":
                idx += 1
                branch_cond = accumulated_neg
            else:
                raise CompileError("malformed <<if>> block", passage)

    walk(None, stop_at=set())
    return segments


# ---------------------------------------------------------------------------
# Turning segments into IR (body, choices, setters).
# ---------------------------------------------------------------------------

_SET_RE_HARLOWE = re.compile(r"\(set:\s*(.*?)\)", re.DOTALL)
_SET_RE_SUGAR = re.compile(r"<<set\s+(.*?)>>", re.DOTALL)
_ASSIGN_RE = re.compile(r"^\$?([A-Za-z_]\w*)\s*(to|\+=|-=|\*=|=)\s*(.+)$", re.DOTALL)


def _forbid_unsupported(text: str, passage: str) -> None:
    if "(print:" in text or "(display:" in text:
        raise CompileError("dynamic macros like (print:)/(display:) are not supported", passage)
    for m in re.finditer(r"\((for|either|link-goto|goto|go-to|live|event|dropdown|cycling-link)\s*:", text):
        raise CompileError(f"unsupported Harlowe macro '({m.group(1)}:)'", passage)


def _parse_assignment(clause: str, passage: str) -> tuple[Setter, bool]:
    m = _ASSIGN_RE.match(clause.strip())
    if not m:
        raise CompileError(f"cannot parse assignment {clause.strip()!r}", passage)
    name, op, rhs = m.group(1), m.group(2), m.group(3).strip()
    op = "=" if op == "to" else op
    rhs_lower = rhs.strip().lower()
    is_bool = rhs_lower in ("true", "false")
    return Setter(name, op, parse_expression(rhs)), is_bool


def _extract_setters(text: str, dialect: str, passage: str) -> tuple[str, list[tuple[Setter, bool]]]:
    setters: list[tuple[Setter, bool]] = []

    def repl_harlowe(m):
        for clause in _split_top_commas(m.group(1)):
            setters.append(_parse_assignment(clause, passage))
        return ""

    def repl_sugar(m):
        for clause in re.split(r";|,", m.group(1)):
            if clause.strip():
                setters.append(_parse_assignment(clause, passage))
        return ""

    if dialect == "harlowe":
        text = _SET_RE_HARLOWE.sub(repl_harlowe, text)
    else:
        text = _SET_RE_SUGAR.sub(repl_sugar, text)
    return text, setters


def _split_top_commas(s: str) -> list[str]:
    parts, depth, cur = [], 0, []
    for c in s:
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
        if c == "," and depth == 0:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(c)
    if "".join(cur).strip():
        parts.append("".join(cur))
    return parts


def _parse_link(raw: str) -> tuple[str, str]:
    """Return (display_text, target) for any of the four link notations."""
    raw = raw.strip()
    if "->" in raw:
        disp, target = raw.split("->", 1)
        return disp.strip(), target.strip()
    if "<-" in raw:
        target, disp = raw.split("<-", 1)
        return disp.strip(), target.strip()
    if "|" in raw:
        disp, target = raw.split("|", 1)
        return disp.strip(), target.strip()
    return raw.strip(), raw.strip()


def _clean_text(text: str) -> str:
    # Collapse the whitespace a stripped macro/link leaves behind, but keep
    # paragraph breaks so the on-device layout still has structure.
    text = re.sub(r"[ \t]+\n", "\n", text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()


def compile_twine(source: str, *, title: str | None = None, uid: str | None = None) -> Story:
    """Compile Twee 3 text or published Twine HTML into a :class:`Story`."""
    if "<tw-passagedata" in source or "<tw-storydata" in source:
        raw_passages, html_name, html_start = _parse_html(source)
        story_data_start = html_start
        story_title = title or html_name
    else:
        raw_passages = _parse_twee(source)
        story_data_start = ""
        story_title = title

    if not raw_passages:
        raise CompileError("no passages found in source")

    dialect = _detect_dialect("\n".join(p.body for p in raw_passages))

    story = Story()
    story_uid = uid
    init_setters: list[tuple[Setter, bool]] = []
    play_passages: list[_RawPassage] = []

    for rp in raw_passages:
        if rp.name == _TITLE_PASSAGE:
            story_title = story_title or rp.body.strip()
            continue
        if rp.name == _DATA_PASSAGE:
            try:
                data = json.loads(rp.body)
            except json.JSONDecodeError as exc:
                raise CompileError(f"StoryData is not valid JSON: {exc}") from exc
            story_data_start = story_data_start or data.get("start", "")
            if not story_uid and data.get("ifid"):
                story_uid = slugify(data["ifid"])
            continue
        if rp.name in _INIT_PASSAGES or (set(rp.tags) & _INIT_TAGS):
            _forbid_unsupported(rp.body, rp.name)
            cleaned, setters = _extract_setters(rp.body, dialect, rp.name)
            init_setters.extend(setters)
            continue
        play_passages.append(rp)

    if not play_passages:
        raise CompileError("story has no playable passages")

    start_name = story_data_start or play_passages[0].name
    story.title = story_title or start_name
    story.uid = story_uid or slugify(story.title)
    story.start = start_name

    var_is_bool: dict[str, bool] = {}
    var_is_nonbool: dict[str, bool] = {}

    def record_setter(setter: Setter, is_bool: bool) -> None:
        if is_bool:
            var_is_bool[setter.var] = True
        else:
            var_is_nonbool[setter.var] = True
        _record_expr_vars(setter.expr, var_is_nonbool)

    for passage in play_passages:
        ir_passage = _compile_passage(passage, dialect, record_setter)
        if passage.name == start_name and init_setters:
            ir_passage.on_enter = [s for s, _ in init_setters] + ir_passage.on_enter
            for s, b in init_setters:
                record_setter(s, b)
        story.passages.append(ir_passage)

    # Every variable that is referenced anywhere becomes a declaration.
    all_vars: dict[str, None] = {}
    for passage in story.passages:
        for setter in passage.on_enter:
            all_vars[setter.var] = None
            _collect_vars(setter.expr, all_vars)
        for part in passage.body:
            if part.cond is not None:
                _collect_vars(part.cond, all_vars)
        for choice in passage.choices:
            if choice.cond is not None:
                _collect_vars(choice.cond, all_vars)

    for name in all_vars:
        vtype = "bool" if var_is_bool.get(name) and not var_is_nonbool.get(name) else "int"
        story.variables.append(VarDecl(name=name, initial=0, type=vtype))

    _resolve_and_check_links(story)
    return story


def _record_expr_vars(node: Node, sink: dict[str, bool]) -> None:
    for name in variables_in(node):
        sink.setdefault(name, True)


def _collect_vars(node: Node, sink: dict) -> None:
    for name in variables_in(node):
        sink.setdefault(name, None)


def _compile_passage(rp: _RawPassage, dialect: str, record_setter) -> Passage:
    _forbid_unsupported(rp.body, rp.name)
    if dialect == "harlowe":
        segments = _segment_harlowe(rp.body, None, rp.name)
    else:
        segments = _segment_sugarcube(rp.body, rp.name)

    passage = Passage(name=rp.name)
    for seg in segments:
        text, setters = _extract_setters(seg.text, dialect, rp.name)
        if setters:
            if seg.cond is not None:
                raise CompileError(
                    "a (set:)/<<set>> inside a conditional block is not supported; "
                    "move it to the top of a passage",
                    rp.name,
                )
            for s, b in setters:
                passage.on_enter.append(s)
                record_setter(s, b)

        # Pull links out of the remaining text; each becomes a choice.
        last = 0
        pieces: list[str] = []
        for m in _LINK_RE.finditer(text):
            pieces.append(text[last : m.start()])
            disp, target = _parse_link(m.group(1))
            # A link to the reserved target END (or an empty target) ends the
            # story rather than navigating to a passage.
            if target == "" or target.upper() == "END":
                target = None
            passage.choices.append(Choice(text=disp, target=target, cond=seg.cond))
            last = m.end()
        pieces.append(text[last:])
        body_text = _clean_text("".join(pieces))
        if body_text:
            passage.body.append(BodyPart(text=body_text, cond=seg.cond))
    return passage


def _resolve_and_check_links(story: Story) -> None:
    names = story.passage_names()
    for passage in story.passages:
        for choice in passage.choices:
            if choice.target and choice.target not in names:
                raise CompileError(
                    f"link points at missing passage {choice.target!r}", passage.name
                )
