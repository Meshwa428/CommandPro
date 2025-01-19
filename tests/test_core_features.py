# test_core_features.py
import pytest
import json
from lexer import Lexer
from parser import Parser
from executor import Executor

def test_variable_assignment(capsys, parser, executor):
    code = """
    SET x = 10;
    SET y = 20;
    SET z = x + y;
    PRINTLN z;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    assert captured.out.strip() == "30"

def test_string_concatenation(capsys, parser, executor):
    code = """
    SET name = "Alice";
    SET greeting = "Hello, " + name + "!";
    PRINTLN greeting;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    assert captured.out.strip() == "Hello, Alice!"

def test_arithmetic_operations(capsys, parser, executor):
    code = """
    SET a = 10;
    SET b = 3;
    SET sum = a + b;
    SET difference = a - b;
    SET product = a * b;
    SET quotient = a / b;
    SET int_quotient = a // b;
    SET remainder = a % b;
    SET power = a ** b;

    PRINTLN sum;
    PRINTLN difference;
    PRINTLN product;
    PRINTLN quotient;
    PRINTLN int_quotient;
    PRINTLN remainder;
    PRINTLN power;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "13\n7\n30\n3.3333333333333335\n3\n1\n1000"
    assert captured.out.strip() == expected_output

def test_boolean_operations(capsys, parser, executor):
    code = """
    SET a = TRUE;
    SET b = FALSE;
    SET result1 = a AND b;
    SET result2 = a OR b;
    SET result3 = NOT a;
    PRINTLN result1;
    PRINTLN result2;
    PRINTLN result3;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "False\nTrue\nFalse"
    assert captured.out.strip() == expected_output

def test_complex_expression(capsys, parser, executor):
    code = """
        SET result = ((15 + 5) * 2 ** 3) / (10 - (3 + 2)) + 4 * (7 - 3);
        PRINTLN result;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    assert captured.out.strip() == "48.0"

def test_conditional_if(capsys, parser, executor):
    code = """
    SET x = 10;
    IF (x > 5) THEN {
        PRINTLN "x is greater than 5";
    }
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    assert captured.out.strip() == "x is greater than 5"

def test_conditional_if_else(capsys, parser, executor):
    code = """
    SET x = 3;
    IF (x > 5) THEN {
        PRINTLN "x is greater than 5";
    } ELSE {
        PRINTLN "x is not greater than 5";
    }
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    assert captured.out.strip() == "x is not greater than 5"

def test_conditional_if_elif_else(capsys, parser, executor):
    code = """
    SET x = 5;
    IF (x > 5) THEN {
        PRINTLN "x is greater than 5";
    } ELSE IF (x < 5) {
        PRINTLN "x is less than 5";
    } ELSE {
        PRINTLN "x is equal to 5";
    }
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    assert captured.out.strip() == "x is equal to 5"

def test_repeat_loop(capsys, parser, executor):
    code = """
    REPEAT 3 TIMES {
        PRINTLN "Looping";
    }
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "Looping\nLooping\nLooping"
    assert captured.out.strip() == expected_output

def test_while_loop(capsys, parser, executor):
    code = """
    SET i = 0;
    WHILE (i < 3) {
        PRINTLN i;
        i++;
    }
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "0\n1\n2"
    assert captured.out.strip() == expected_output

def test_function_definition_and_call(capsys, parser, executor):
    code = """
    DEFUN add(a, b) {
        RETURN a + b;
    }
    SET result = add(5, 7);
    PRINTLN result;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    assert captured.out.strip() == "12"

def test_function_with_multiple_returns(capsys, parser, executor):
    code = """
    DEFUN check_value(x) {
        IF (x > 10) {
            RETURN "Greater";
        } ELSE IF (x < 0){
            RETURN "Negative";
        } ELSE {
            RETURN "Within Range";
        }
    }
    PRINTLN check_value(12);
    PRINTLN check_value(2);
    PRINTLN check_value(-1);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "Greater\nWithin Range\nNegative"
    assert captured.out.strip() == expected_output
    
def test_for_loop(capsys, parser, executor):
    code = """
    SET numbers = [1, 2, 3, 4, 5];
    FOR num IN numbers {
        PRINTLN num;
    }
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "1\n2\n3\n4\n5"
    assert captured.out.strip() == expected_output
    
def test_range(capsys, parser, executor):
    code = """
    FOR num IN RANGE(1, 5) {
        PRINTLN num;
    }
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "1\n2\n3\n4"
    assert captured.out.strip() == expected_output
    
def test_range_with_step(capsys, parser, executor):
    code = """
    FOR num IN RANGE(1, 10, 2) {
        PRINTLN num;
    }
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "1\n3\n5\n7\n9"
    assert captured.out.strip() == expected_output
    
def test_list_operations(capsys, parser, executor):
    code = """
    SET my_list = [1, 2, 3];
    PRINTLN my_list;
    SET val = my_list[0];
    PRINTLN val;
    SET val_2 = my_list[2];
    PRINTLN val_2;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "[1, 2, 3]\n1\n3"
    assert captured.out.strip() == expected_output
    
def test_list_comprehension(capsys, parser, executor):
    code = """
    SET numbers = [1, 2, 3, 4, 5];
    SET squares = [num * num FOR num IN numbers];
    PRINTLN squares;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "[1, 4, 9, 16, 25]"
    assert captured.out.strip() == expected_output

def test_lambda_function(capsys, parser, executor):
    code = """
    SET add = LAMBDA (a, b) {
        RETURN a + b;
    };

    PRINTLN add(3, 5);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "8"
    assert captured.out.strip() == expected_output

def test_nested_conditions_with_break_and_continue(capsys, parser, executor):
    code = """
        SET i = 0;
        WHILE (i < 5) {
            i++;
            IF (i == 2) {
               CONTINUE;
            };
            IF (i == 4) {
                BREAK;
            };
            PRINTLN i;
        };
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "1\n3"
    assert captured.out.strip() == expected_output