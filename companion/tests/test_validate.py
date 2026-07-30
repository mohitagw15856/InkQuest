from pathlib import Path

from inkquest.compilers.twine import compile_twine
from inkquest.iqs import read_iqs
from inkquest.validate import validate_iqs, validate_story

_DEMO = Path(__file__).parents[2] / "stories" / "the-lighthouse-keeper" / "the-lighthouse-keeper.iqs"

_STORY = """:: Start
Go [[On->Middle]].

:: Middle
Nearly there. [[Finish->End1]] or [[Look->Corner]]

:: End1
Done. [[The End->END]]

:: Corner
Nothing here at all.

:: Trap
You are stuck forever.
"""


def test_detects_unreachable_and_dead_ends():
    story = compile_twine(_STORY)
    report = validate_story(story)
    assert "Trap" in report.unreachable
    assert "Corner" in report.dead_ends
    assert "Trap" in report.dead_ends
    assert "End1" in report.endings
    assert not report.ok


def test_clean_story_reports_ok():
    src = """:: Start
Begin. [[Go->Fin]]
:: Fin
Done. [[The End->END]]
"""
    report = validate_story(compile_twine(src))
    assert report.ok
    assert report.unreachable == []
    assert report.dead_ends == []


def test_demo_story_validates_clean():
    assert _DEMO.exists(), "demo .iqs must be compiled and committed"
    report = validate_iqs(read_iqs(_DEMO.read_bytes()))
    assert report.ok, report.format()
    assert report.passage_count == 28
    assert len(report.endings) == 4


def test_report_format_mentions_counts():
    report = validate_story(compile_twine(_STORY))
    text = report.format()
    assert "unreachable" in text
    assert "dead-end" in text
