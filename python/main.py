import pytest
from lexer import Lexer
from parser import Parser
from executor import Executor

def test_while_loop_with_break():
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
    executor = Executor()
    parser = Parser(tokens, executor.global_scope, executor.functions)
    ast = parser.parse()
    executor.execute(ast)
    # Should print 1, 2, 3, 4 only

def test_function_with_return():
    code = """
    DEFUN add(a, b) {
        SET c = a + b;
        RETURN c;
    }
    PRINTLN add(5, 3);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    executor = Executor()
    parser = Parser(tokens, executor.global_scope, executor.functions)
    ast = parser.parse()
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
    executor = Executor()
    parser = Parser(tokens, executor.global_scope, executor.functions)
    ast = parser.parse()
    
    executor.execute(ast)
    # Should print 0, 2 three times

def test_increment_decrement():
    code = """
    #* This is a multi-line comment
       testing the increment and decrement
       operators *#
    SET i = 5;
    i++;  # Single line comment: increment i
    PRINTLN i;  # Should print 6
    i--;  # Single line comment: decrement i
    PRINTLN i;  # Should print 5
    """

    lexer = Lexer(code)
    tokens = lexer.tokenize()

    executor = Executor()
    parser = Parser(tokens, executor.global_scope, executor.functions)  # Initialize i in global scope
    ast = parser.parse()
    executor.execute(ast)

if __name__ == "__main__":
    test_increment_decrement()
