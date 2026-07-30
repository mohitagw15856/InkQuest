from pathlib import Path

from inkquest.cli import main
from inkquest.iqs import read_iqs

_GOOD = """:: StoryTitle
Tiny Tale

:: StoryData
{ "ifid": "CLI-TEST-1", "start": "Start" }

:: Start
Begin the tale. [[Onward->Fin]]

:: Fin
The end. [[The End->END]]
"""

_BROKEN = """:: Start
Nowhere to go, and stuck.
"""


def test_compile_writes_iqs(tmp_path: Path, capsys):
    src = tmp_path / "tale.twee"
    src.write_text(_GOOD, encoding="utf-8")
    out = tmp_path / "tale.iqs"
    rc = main(["compile", str(src), "-o", str(out)])
    assert rc == 0
    assert out.exists()
    story = read_iqs(out.read_bytes())
    assert story.title == "Tiny Tale"
    assert story.uid == "cli-test-1"
    assert story.passage_count == 2


def test_compile_default_output_path(tmp_path: Path):
    src = tmp_path / "tale.twee"
    src.write_text(_GOOD, encoding="utf-8")
    rc = main(["compile", str(src)])
    assert rc == 0
    assert (tmp_path / "tale.iqs").exists()


def test_validate_command_on_iqs(tmp_path: Path):
    src = tmp_path / "tale.twee"
    src.write_text(_GOOD, encoding="utf-8")
    out = tmp_path / "tale.iqs"
    main(["compile", str(src), "-o", str(out)])
    assert main(["validate", str(out)]) == 0


def test_compile_rejects_broken_story_without_force(tmp_path: Path, capsys):
    src = tmp_path / "broken.twee"
    src.write_text(_BROKEN, encoding="utf-8")
    out = tmp_path / "broken.iqs"
    rc = main(["compile", str(src), "-o", str(out)])
    assert rc == 1
    assert not out.exists()
    err = capsys.readouterr().err
    assert "validation" in err.lower()


def test_force_compiles_broken_story(tmp_path: Path):
    src = tmp_path / "broken.twee"
    src.write_text(_BROKEN, encoding="utf-8")
    out = tmp_path / "broken.iqs"
    rc = main(["compile", str(src), "-o", str(out), "--force"])
    assert rc == 0
    assert out.exists()


def test_missing_input_returns_error(tmp_path: Path):
    assert main(["compile", str(tmp_path / "nope.twee")]) == 2
