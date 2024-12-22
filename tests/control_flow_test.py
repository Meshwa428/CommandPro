import pytest
import sys
import os
os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from python.lexer import Lexer
from python.parser import Parser
from python.executor import Executor

def test_while_loop_with_break():
    code = """
    SET i = 0;
    WHILE (i < 10) {
        i++;
        IF (i == 5) {
            BREAK;
        };
        PRINTLN i;
    };
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    parser = Parser(tokens, {}, {})
    ast = parser.parse()
    executor = Executor()
    executor.execute(ast)
    # Should print 1, 2, 3, 4 only

def test_function_with_return():
    code = """
    DEFUN max(a, b) {
        IF (a > b) {
            RETURN a;
        };
        RETURN b;
    };
    PRINTLN max(5, 3);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    parser = Parser(tokens, {}, {})
    ast = parser.parse()
    executor = Executor()
    executor.execute(ast)
    # Should print 5

def test_nested_loops_with_control():
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
    parser = Parser(tokens, {}, {})
    ast = parser.parse()
    executor = Executor()
    executor.execute(ast)
    # Should print 0, 2 three times

def test_increment_decrement():
    code = """
    SET i = 5;
    i++;
    PRINTLN i;  -- Should print 6
    i--;
    PRINTLN i;  -- Should print 5
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    executor = Executor()
    executor.global_scope["i"] = 5  # Initialize i in global scope
    parser = Parser(tokens, executor.global_scope, {})
    ast = parser.parse()
    executor.execute(ast)