"""Tests for the Ink (inklecate JSON) front-end against representative fixtures.

The fixtures follow the documented inklecate runtime layout: nested containers,
``^`` text output, ``{"*": path}`` choices resolved to ``c-N`` sub-containers,
plain ``->`` diverts and a ``global decl`` container for variables.
"""

import pytest

from inkquest.compilers.ink import compile_ink_json
from inkquest.errors import CompileError
from inkquest.iqs import read_iqs, write_iqs

FIXTURE = {
    "inkVersion": 21,
    "root": [
        ["^You wake in a cell.", "\n", {"->": "hallway"}],
        {
            "global decl": [5, {"VAR=": "gold"}, "done", {"#f": 1}],
            "hallway": [
                [
                    "^A cold hallway stretches out.",
                    "\n",
                    {"*": ".^.c-0", "flg": 20},
                    {"*": ".^.c-1", "flg": 20},
                    {
                        "c-0": [{"s": ["^Go left", "/str"]}, "\n", {"->": "leftroom"}, {"#f": 5}],
                        "c-1": ["^Go right", "\n", {"->": "rightroom"}, {"#f": 5}],
                    },
                ],
                {"#f": 1},
            ],
            "leftroom": [["^You find the exit. The end.", "\n", "done"], {"#f": 1}],
            "rightroom": [["^A dead end.", "\n", {"->": "hallway"}], {"#f": 1}],
        },
    ],
    "listDefs": {},
}


def test_variables_from_global_decl():
    story = compile_ink_json(FIXTURE, title="Cell")
    gold = next(v for v in story.variables if v.name == "gold")
    assert gold.initial == 5


def test_knots_become_passages():
    story = compile_ink_json(FIXTURE, title="Cell")
    names = story.passage_names()
    assert {"Start", "hallway", "leftroom", "rightroom"} <= names
    assert story.start == "Start"


def test_start_has_continue_to_hallway():
    story = compile_ink_json(FIXTURE, title="Cell")
    start = story.find("Start")
    assert "You wake in a cell." in start.plain_text()
    assert start.choices[0].target == "hallway"


def test_choices_resolved_with_labels_and_targets():
    story = compile_ink_json(FIXTURE, title="Cell")
    hallway = story.find("hallway")
    got = {(c.text, c.target) for c in hallway.choices}
    assert got == {("Go left", "leftroom"), ("Go right", "rightroom")}


def test_natural_end_is_an_ending_not_a_dead_end():
    story = compile_ink_json(FIXTURE, title="Cell")
    left = story.find("leftroom")
    assert len(left.choices) == 1
    assert left.choices[0].target is None  # ending


def test_full_playthrough_via_runtime():
    story = read_iqs(write_iqs(compile_ink_json(FIXTURE, title="Cell")))
    rt = story.new_state()
    p = rt.enter(story.start)
    assert rt.values[story.var_names.index("gold")] == 5
    p = rt.choose(p.choices[0])  # Continue -> hallway
    # Go left -> leftroom -> ending.
    left = [c for c in p.choices if c.text == "Go left"][0]
    p = rt.choose(left)
    assert "You find the exit." in rt.visible_text(p)
    assert rt.choose(p.choices[0]) is None


def test_list_types_rejected():
    data = dict(FIXTURE)
    data["listDefs"] = {"colours": {"red": 1}}
    with pytest.raises(CompileError):
        compile_ink_json(data)


def test_bad_version_rejected():
    with pytest.raises(CompileError):
        compile_ink_json({"inkVersion": 10, "root": [[]], "listDefs": {}})


def test_missing_root_rejected():
    with pytest.raises(CompileError):
        compile_ink_json({"inkVersion": 21})


def test_unsupported_runtime_op_errors():
    data = {
        "inkVersion": 21,
        "root": [
            ["^Hi.", {"->": "k"}],
            {"k": [["^In k.", {"seq": 1}, "done"], {"#f": 1}]},
        ],
        "listDefs": {},
    }
    with pytest.raises(CompileError) as exc:
        compile_ink_json(data)
    assert "unsupported" in str(exc.value).lower()
