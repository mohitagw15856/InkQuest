import pytest

from inkquest.errors import CompileError
from inkquest.expr import compile_expr, parse_expression, variables_in
from inkquest.ir import Binary, Num, Unary, Var


def test_parse_arithmetic_precedence():
    node = parse_expression("1 + 2 * 3")
    assert node == Binary("+", Num(1), Binary("*", Num(2), Num(3)))


def test_parse_harlowe_is_and_is_not():
    assert parse_expression("$hp is 3") == Binary("==", Var("hp"), Num(3))
    assert parse_expression("$hp is not 3") == Binary("!=", Var("hp"), Num(3))


def test_parse_boolean_literals():
    assert parse_expression("true") == Num(1)
    assert parse_expression("$flag is false") == Binary("==", Var("flag"), Num(0))


def test_parse_and_or_not_symbols_and_words():
    a = parse_expression("$a and not $b")
    b = parse_expression("$a && !$b")
    assert a == b == Binary("and", Var("a"), Unary("not", Var("b")))
    assert parse_expression("$a or $b") == Binary("or", Var("a"), Var("b"))


def test_parenthesised_expression():
    node = parse_expression("($a + 1) * 2")
    assert node == Binary("*", Binary("+", Var("a"), Num(1)), Num(2))


def test_variables_in():
    node = parse_expression("$gold >= 5 and $hasKey")
    assert variables_in(node) == {"gold", "hasKey"}


def test_float_is_rejected():
    with pytest.raises(CompileError):
        parse_expression("$x is 1.5")


def test_unknown_variable_in_compile_is_rejected():
    node = parse_expression("$missing")
    with pytest.raises(CompileError):
        compile_expr(node, {"present": 0})


def test_compiled_bytecode_shape():
    # $gold >= 5  ->  PUSH_VAR 0, PUSH_I32 5, GE
    code = compile_expr(parse_expression("$gold >= 5"), {"gold": 0})
    assert code[0] == 0x02  # OP_PUSH_VAR
    assert code[1:3] == b"\x00\x00"  # var index 0
    assert code[3] == 0x01  # OP_PUSH_I32
    assert code[4:8] == (5).to_bytes(4, "little")
    assert code[8] == 0x25  # OP_GE


def test_empty_expression_errors():
    with pytest.raises(CompileError):
        parse_expression("   ")
