import pytest

from inkquest.compilers.twine import compile_twine, slugify
from inkquest.errors import CompileError
from inkquest.iqs import read_iqs, write_iqs


def _roundtrip(story):
    """Compile the IR story to bytes and back to a playable reference runtime."""
    return read_iqs(write_iqs(story))


def test_link_notations_all_parse():
    src = """:: Start
Go [[north->North]] or [[South<-go south]] or [[go west|West]] or [[East]].

:: North
End. [[The End->END]]
:: South
End. [[The End->END]]
:: West
End. [[The End->END]]
:: East
End. [[The End->END]]
"""
    story = compile_twine(src)
    start = story.find("Start")
    targets = {c.target for c in start.choices}
    assert targets == {"North", "South", "West", "East"}
    labels = {c.text for c in start.choices}
    assert "go south" in labels and "go west" in labels


def test_harlowe_set_becomes_on_enter_and_variable_declared():
    src = """:: Start
(set: $gold to 5)(set: $flag to true)
You have gold. [[Onward->Next]]

:: Next
Done. [[The End->END]]
"""
    story = compile_twine(src)
    start = story.find("Start")
    assert [s.var for s in start.on_enter] == ["gold", "flag"]
    names = {v.name for v in story.variables}
    assert {"gold", "flag"} <= names
    assert next(v for v in story.variables if v.name == "flag").type == "bool"
    assert next(v for v in story.variables if v.name == "gold").type == "int"


def test_harlowe_conditional_choice_and_text():
    src = """:: Start
(set: $key to true)
(if: $key)[The door is open. [[Enter->Room]]](else:)[It is locked.]
[[Wait->Start]]

:: Room
Inside. [[The End->END]]
"""
    story = compile_twine(src)
    rt = _roundtrip(story).new_state()
    p = rt.enter(rt.current)
    # Conditional body text 'The door is open.' shows because key is true.
    assert "The door is open." in rt.visible_text(p)
    # The Enter choice (conditional on key) is visible; find it.
    enter = [c for c in p.choices if c.text == "Enter"][0]
    assert rt.choice_visible(enter) is True


def test_sugarcube_dialect():
    src = """:: Start
<<set $hp = 3>>
<<if $hp gt 2>>You are strong. [[Fight->Battle]]<<else>>You are weak.<</if>>
[[Flee->Battle]]

:: Battle
End. [[The End->END]]
"""
    story = compile_twine(src)
    rt = _roundtrip(story).new_state()
    p = rt.enter(rt.current)
    assert "You are strong." in rt.visible_text(p)
    assert any(c.text == "Fight" for c in p.choices)


def test_startup_passage_merged_into_start():
    src = """:: Setup [startup]
(set: $courage to 2)

:: Start
Begin. [[Go->Next]]

:: Next
Done. [[The End->END]]
"""
    story = compile_twine(src)
    start = story.find("Start")
    assert start.on_enter[0].var == "courage"
    rt = _roundtrip(story).new_state()
    rt.enter(rt.current)
    idx = story.var_names().__contains__("courage")
    assert idx  # declared
    assert rt.values[read_iqs(write_iqs(story)).var_names.index("courage")] == 2


def test_end_sentinel_is_an_ending():
    src = """:: Start
The end is here. [[Finish->END]]
"""
    story = compile_twine(src)
    assert story.find("Start").choices[0].target is None


def test_missing_link_target_errors():
    src = """:: Start
Go [[nowhere->Ghost]].
"""
    with pytest.raises(CompileError) as exc:
        compile_twine(src)
    assert "Ghost" in str(exc.value)


def test_unsupported_macro_errors_clearly():
    src = """:: Start
(print: $gold) [[Next->Next]]
:: Next
x [[The End->END]]
"""
    with pytest.raises(CompileError) as exc:
        compile_twine(src)
    assert "print" in str(exc.value).lower()


def test_set_inside_conditional_errors():
    src = """:: Start
(if: $x)[(set: $y to 1)] [[Next->Next]]
:: Next
x [[The End->END]]
"""
    with pytest.raises(CompileError) as exc:
        compile_twine(src)
    assert "set" in str(exc.value).lower()


def test_html_publish_format():
    src = """<tw-storydata name="My Tale" startnode="1" format="Harlowe">
<tw-passagedata pid="1" name="Start" tags="">Hello [[world->End2]]</tw-passagedata>
<tw-passagedata pid="2" name="End2" tags="">Bye [[The End->END]]</tw-passagedata>
</tw-storydata>"""
    story = compile_twine(src)
    assert story.title == "My Tale"
    assert story.start == "Start"
    assert story.find("Start").choices[0].target == "End2"


def test_slugify():
    assert slugify("The Lighthouse Keeper!") == "the-lighthouse-keeper"
    assert slugify("***") == "story"
