import json
import logging
import pytest
from lexer import Lexer
from parser import Parser
from executor import Executor

def setup_logging(enable_logging=False, log_level=logging.DEBUG, log_file="app.log"):
    """Configure logging for the application."""
    if not enable_logging:
        logging.disable(logging.CRITICAL)
        return
    
    logging.basicConfig(
        level=log_level,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        handlers=[
            logging.FileHandler(log_file),
            logging.StreamHandler()
        ]
    )

@pytest.fixture
def executor():
    return Executor()

@pytest.fixture
def parser(executor):
    return Parser(executor.global_scope, executor.functions)

def test_while_loop_with_break(capsys, parser, executor):
    code = """
    SET i = 10;
    DEFUN increment(x) {
        x++;
        RETURN x;
    };
    WHILE (i > 1) {
        i--;
        IF (i == 5) {
            REPEAT i TIMES {
                PRINTLN "Hello";
            };
        };
        PRINTLN i;
    };
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "9\n8\n7\n6\nHello\nHello\nHello\nHello\nHello\n5\n4\n3\n2\n1"
    assert captured.out.strip() == expected_output

def test_function_with_return(capsys, parser, executor):
    code = """
    DEFUN add(a, b) {
        SET c = a + b;
        RETURN c;
    }
    PRINTLN add(5, 3);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "8"
    assert captured.out.strip() == expected_output

def test_nested_loops_with_control(capsys, parser, executor):
    code = """
    REPEAT 3 TIMES {
        SET j = 0;
        WHILE (j < 3) {
            IF (j == 1) {
                j++;
                CONTINUE;
            };
            PRINTLN j;
            j++;
        };
    };
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "0\n2\n0\n2\n0\n2"
    assert captured.out.strip() == expected_output

def test_increment_decrement(capsys, parser, executor):
    code = """
    #* This is a multi-line comment
       testing the increment and decrement
       operators *#
    SET i = 5;
    i++;  # Single line comment: increment i
    PRINTLN i;  # Should print 6
    i--;  # Single line comment: decrement i
    PRINTLN i;  # Should print 5

    # Test Case 4: Conditional Logic
    DEFUN test_case_4() {
        PRINTLN "Test Case 4: Conditional Logic";
        IF (WINDOW "Calculator" EXISTS) THEN {
            FOCUS WINDOW "Calculator";
            MOVE WINDOW "Calculator" TO (150, 150);
            SET a = 10;
        }
        PRINTLN a;
    };
    test_case_4();
    PRINTLN i;
    PRINTLN ++i;
    PRINTLN --i;
    PRINT "\n\n\n\n";
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    executor.window_manager.create_window("Calculator", 800, 600)
    executor.window_manager.create_window("Notepad", 800, 600)
    executor.window_manager.create_window("Word", 800, 600)
    ast = parser.parse(tokens)
    with open("ast.json", "w") as file:
        json.dump(ast.to_dict(), file, indent=4)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = (
        "6\n"
        "5\n"
        "Test Case 4: Conditional Logic\n"
        "10\n"
        "5\n"
        "6\n"
        "5\n"
        "\n\n\n\n"
    )
    assert captured.out == expected_output

def test_function_with_pass(capsys, parser, executor):
    code = """
    DEFUN add(a, b) {
        PASS;
    }
    PRINTLN add(5, 3);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "None"
    assert captured.out.strip() == expected_output
