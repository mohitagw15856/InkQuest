"""Round-trip the writer through the reference reader / runtime."""

from inkquest.ir import Binary, BodyPart, Choice, Num, Passage, Setter, Story, Var, VarDecl
from inkquest.iqs import NO_PASSAGE, read_iqs, write_iqs


def _sample_story() -> Story:
    story = Story(title="The Gate", author="Tests", uid="the-gate", start="gate")
    story.variables = [VarDecl("gold", 0, "int"), VarDecl("hasKey", 0, "bool")]
    gate = Passage(
        name="gate",
        body=[BodyPart("You stand at the gate.")],
        choices=[
            Choice(
                "Take the key",
                "hall",
                effects=[Setter("hasKey", "=", Num(1)), Setter("gold", "+=", Num(5))],
            ),
            Choice("Leave", "outside"),
        ],
    )
    hall = Passage(
        name="hall",
        body=[
            BodyPart("The hall is cold."),
            BodyPart("The key gleams.", cond=Binary("==", Var("hasKey"), Num(1))),
        ],
        choices=[
            Choice("Open the vault", "vault", cond=Binary("==", Var("hasKey"), Num(1))),
            Choice("Search the floor", "outside", cond=Binary("==", Var("hasKey"), Num(0))),
        ],
    )
    outside = Passage("outside", body=[BodyPart("You walk into the rain.")],
                      choices=[Choice("The End", None)])
    vault = Passage(
        "vault",
        body=[BodyPart("Gold spills out.", cond=Binary(">=", Var("gold"), Num(5)))],
        choices=[Choice("The End", None)],
    )
    story.passages = [gate, hall, outside, vault]
    return story


def test_header_and_meta_roundtrip():
    data = write_iqs(_sample_story())
    story = read_iqs(data)
    assert story.title == "The Gate"
    assert story.uid == "the-gate"
    assert story.passage_count == 4
    assert story.var_names == ["gold", "hasKey"]
    assert story.start == 0


def test_runtime_key_path_matches_cpp_semantics():
    data = write_iqs(_sample_story())
    story = read_iqs(data)
    rt = story.new_state()

    p = rt.enter(story.start)
    assert rt.visible_text(p) == "You stand at the gate."
    # Take the key.
    p = rt.choose(p.choices[0])
    assert rt.values[0] == 5  # gold
    assert rt.values[1] == 1  # hasKey
    assert rt.visible_text(p) == "The hall is cold.The key gleams."
    assert rt.choice_visible(p.choices[0]) is True
    assert rt.choice_visible(p.choices[1]) is False
    # Open the vault, reach the ending.
    p = rt.choose(p.choices[0])
    assert rt.visible_text(p) == "Gold spills out."
    assert rt.choose(p.choices[0]) is None
    assert rt.current == NO_PASSAGE


def test_runtime_no_key_path():
    data = write_iqs(_sample_story())
    rt = read_iqs(data).new_state()
    p = rt.enter(rt.current)
    p = rt.choose(p.choices[1])  # Leave
    assert rt.values[1] == 0
    assert rt.visible_text(p) == "You walk into the rain."


def test_conditional_hidden_without_key():
    data = write_iqs(_sample_story())
    story = read_iqs(data)
    rt = story.new_state()
    # Jump straight to the hall without a key.
    hall_index = [p.name for p in story.passages()].index("hall")
    p = rt.enter(hall_index)
    assert rt.visible_text(p) == "The hall is cold."
    assert rt.choice_visible(p.choices[0]) is False
    assert rt.choice_visible(p.choices[1]) is True
