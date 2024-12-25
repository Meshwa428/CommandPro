import json
import logging
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

    executor = Executor()

    executor.window_manager.create_window("Calculator", 800, 600)
    executor.window_manager.create_window("Notepad", 800, 600)
    executor.window_manager.create_window("Word", 800, 600)

    parser = Parser(tokens, executor.global_scope, executor.functions)  # Initialize i in global scope
    ast = parser.parse()
    with open("ast.json", "w") as file:
        json.dump(ast.to_dict(), file, indent=4)
    executor.execute(ast)

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

def test_function_with_pass():
    code = """
    DEFUN add(a, b) {
        PASS;
    }
    PRINTLN add(5, 3);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    executor = Executor()
    parser = Parser(tokens, executor.global_scope, executor.functions)
    ast = parser.parse()


    executor.execute(ast)


if __name__ == "__main__":
    # setup_logging(enable_logging=True)
    test_increment_decrement()
