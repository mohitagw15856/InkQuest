"""Ink front-end: compile inklecate JSON into the InkQuest IR.

Ink's compiler (``inklecate -j story.ink``) emits a *runtime* JSON: a tree of
nested containers holding a flat instruction stream rather than a passage graph.
This front-end reconstructs a choose-your-own-adventure graph from a well-defined
subset of that stream:

* ``VAR`` declarations with integer (or 0/1 boolean) initial values;
* knots (top-level named containers) become passages, plus an implicit ``Start``
  passage built from the root's own leading content;
* string output (``^text``) becomes body text, ``"\n"`` becomes a line break;
* choices (``{"*": path}``) are resolved against the knot's named sub-containers;
  the sub-container's ``^`` text is the label and its ``->`` divert the target;
* a trailing plain divert (``->``) with no condition becomes an automatic
  "Continue" choice.

Anything richer (threads, tunnels, functions, LISTs, glue-driven assembly,
variable printing inside text, conditional or looping content, variable diverts)
is outside the subset and raises :class:`~inkquest.errors.CompileError` naming
the construct. See ``docs/FORMAT.md`` for the exact supported grammar.

Choice pointers are resolved by their final path component against every named
container found in the knot subtree; the deep relative path forms ink can emit
are not interpreted. The subset is validated against representative fixtures that
follow the documented inklecate JSON layout; when wiring in a real inklecate
build, re-run ``inkquest validate`` on the produced ``.iqs``.
"""

from __future__ import annotations

from ..errors import CompileError
from ..ir import BodyPart, Choice, Passage, Story, VarDecl
from .twine import slugify

_CONTINUE_LABEL = "Continue"
_IGNORED_CONTROL = {
    "done", "end", "ev", "/ev", "out", "/str", "str", "pop", "nop", "du",
    "void", "/ev ", "^->", "->->", "~ret",
}
_UNSUPPORTED_KEYS = ("temp=", "CNT?", "seq", "G>", "G<", "->t->", "f()", "x()", "%", "VAR?")


_COMMAND_KEYS = {"->", "*", "VAR=", "temp=", "s", "c", "G>", "G<", "CNT?", "seq", "->t->"}


def _is_attributes(d) -> bool:
    """True if a trailing dict is a container's attribute map rather than a
    content command.

    Ink's runtime JSON overloads the final array element: it is the named-content
    and flags object (``#f`` flags, ``#n`` name, named child containers) unless it
    is really a content command such as a divert (``{"->": ...}``) or a choice
    (``{"*": ...}``). This distinguishes the two so a trailing divert is not
    mistaken for attributes.
    """
    if not isinstance(d, dict) or not d:
        return False
    if any(k.startswith("#") for k in d):
        return True
    if any(k in _COMMAND_KEYS for k in d):
        return False
    return all(isinstance(v, list) for v in d.values())


def _named_map(container: list) -> dict:
    """The trailing named-content object of an ink container, or ``{}``."""
    if container and _is_attributes(container[-1]):
        return {k: v for k, v in container[-1].items() if not k.startswith("#")}
    return {}


def _content(container: list) -> list:
    """Positional content of an ink container (all but the trailing attributes)."""
    if container and _is_attributes(container[-1]):
        return container[:-1]
    return list(container)


def _registry(container: list, reg: dict[str, list]) -> None:
    """Index every named sub-container in a knot subtree by its own name."""
    for key, value in _named_map(container).items():
        if isinstance(value, list):
            reg.setdefault(key, value)
            _registry(value, reg)
    for item in _content(container):
        if isinstance(item, list):
            _registry(item, reg)


def _extract_divert(item) -> str | None:
    if isinstance(item, dict) and "->" in item and isinstance(item["->"], str):
        if item.get("var"):
            return None
        return item["->"]
    return None


def _read_global_decl(decl: list) -> list[VarDecl]:
    """Parse a ``global decl`` container into variable declarations."""
    variables: list[VarDecl] = []
    pending = None
    have = False
    for item in _content(decl):
        if isinstance(item, bool):
            pending, have = (1 if item else 0), True
        elif isinstance(item, int):
            pending, have = item, True
        elif isinstance(item, float):
            raise CompileError(f"floating point VAR initial value {item!r} is not supported")
        elif isinstance(item, dict) and "VAR=" in item:
            name = item["VAR="]
            if not have:
                raise CompileError(f"VAR {name!r} has no integer initial value the compiler can read")
            variables.append(VarDecl(name=name, initial=int(pending), type="int"))
            have = False
    return variables


def _resolve_choice(reg: dict[str, list], path: str, knot: str) -> tuple[str, str | None]:
    key = path.split(".")[-1]
    sub = reg.get(key)
    if sub is None or not isinstance(sub, list):
        raise CompileError(f"choice container {path!r} not found or malformed", knot)
    labels: list[str] = []
    target: str | None = None

    def scan(node: list) -> None:
        nonlocal target
        for it in _content(node):
            if isinstance(it, str):
                if it.startswith("^"):
                    labels.append(it[1:])
            elif isinstance(it, list):
                scan(it)
            elif isinstance(it, dict):
                if "s" in it and isinstance(it["s"], list):
                    for s in it["s"]:
                        if isinstance(s, str) and s.startswith("^"):
                            labels.append(s[1:])
                else:
                    div = _extract_divert(it)
                    if div is not None:
                        target = div

    scan(sub)
    label = "".join(labels).strip()
    if not label:
        raise CompileError(f"choice {path!r} has no readable label text", knot)
    return label, target


def _compile_knot(name: str, container: list) -> Passage:
    passage = Passage(name=name)
    reg: dict[str, list] = {}
    _registry(container, reg)
    body: list[str] = []
    fallthrough: str | None = None

    def walk(node: list) -> None:
        nonlocal fallthrough
        for item in _content(node):
            if isinstance(item, str):
                if item.startswith("^"):
                    body.append(item[1:])
                elif item == "\n":
                    body.append("\n")
                elif item in _IGNORED_CONTROL:
                    continue
                # other bare control strings are no-ops for our subset
            elif isinstance(item, list):
                walk(item)
            elif isinstance(item, dict):
                if "*" in item:
                    label, target = _resolve_choice(reg, item["*"], name)
                    passage.choices.append(Choice(text=label, target=target))
                elif "->" in item:
                    div = _extract_divert(item)
                    if div is None:
                        raise CompileError(f"variable divert in knot {name!r} is not supported", name)
                    if "c" in item:
                        raise CompileError(f"conditional divert in knot {name!r} is not supported", name)
                    fallthrough = div
                elif any(k in item for k in _UNSUPPORTED_KEYS):
                    raise CompileError(f"unsupported Ink runtime op {list(item)!r} in knot {name!r}", name)
                # flag-only / named-map dicts are ignored

    walk(container)

    text = "".join(body).strip()
    if text:
        passage.body.append(BodyPart(text=text))
    if not passage.choices:
        # A knot with a plain trailing divert advances automatically; a knot that
        # simply runs out of flow is a natural story ending.
        if fallthrough is not None:
            passage.choices.append(Choice(text=_CONTINUE_LABEL, target=fallthrough))
        else:
            passage.choices.append(Choice(text="The End", target=None))
    return passage


def compile_ink_json(data: dict, *, title: str | None = None, uid: str | None = None) -> Story:
    """Compile a parsed inklecate JSON document into a :class:`Story`."""
    if not isinstance(data, dict) or "root" not in data:
        raise CompileError("not an inklecate JSON document (missing 'root')")
    version = data.get("inkVersion", 0)
    if not isinstance(version, int) or version < 17:
        raise CompileError(f"unsupported inkVersion {version!r} (need >= 17)")
    if data.get("listDefs"):
        raise CompileError("Ink LIST types are not supported")

    root = data["root"]
    if not isinstance(root, list):
        raise CompileError("malformed ink root container")
    root_map = _named_map(root)

    story = Story()
    story.title = title or "Ink Story"
    story.uid = uid or slugify(story.title)

    if "global decl" in root_map:
        story.variables = _read_global_decl(root_map["global decl"])

    knots = [(k, v) for k, v in root_map.items() if k != "global decl" and isinstance(v, list)]

    # The root's own leading content (which may be nested in sub-containers) is
    # the implicit start knot. Include it only when it yields real content.
    start_passage = _compile_knot("Start", root)
    start_has_content = bool(start_passage.body) or start_passage.choices[0].target is not None
    if start_has_content:
        story.passages.append(start_passage)
        story.start = "Start"

    for knot_name, container in knots:
        story.passages.append(_compile_knot(knot_name, container))

    if not story.passages:
        raise CompileError("no knots or start content found in ink JSON")
    if not story.start:
        story.start = story.passages[0].name

    _check_links(story)
    return story


def _check_links(story: Story) -> None:
    names = story.passage_names()
    for passage in story.passages:
        for choice in passage.choices:
            if choice.target and choice.target not in names:
                raise CompileError(f"divert to missing knot {choice.target!r}", passage.name)
