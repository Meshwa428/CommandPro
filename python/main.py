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
def parser():
    return Parser()

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
    }
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

def test_lambda_and_named_args(capsys, parser, executor):
    code = """
    DEFUN apply_twice(f, x) {
        RETURN f(f(x));
    }

    # LAMBDA (x) {
    #     RETURN x * 2;
    # };

    SET double = LAMBDA (x) {
        RETURN x * 2;
    };

    PRINTLN apply_twice(double, 3);  # Should print 12

    # Test named arguments
    DEFUN greet(first_name, last_name) {
        PRINTLN "Hello " + first_name + " " + last_name;
    }

    greet(last_name="Doe", first_name="John");  # Should print "Hello John Doe"
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    with open("ast1.json", "w") as file:
        json.dump(ast.to_dict(), file, indent=4)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "12\nHello John Doe"
    assert captured.out.strip() == expected_output

def test_point_constructor(capsys, parser, executor):
    code = """
    SET p = POINT(100, 200);
    MOVE MOUSE TO p;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    with open("ast.json", "w") as file:
        json.dump(ast.to_dict(), file, indent=4)
    executor.execute(ast)
    captured = capsys.readouterr()
    assert executor.mouse_manager.current_position == (100, 200)

def test_function_composition(capsys, parser, executor):
    code = """
    DEFUN add_one(x) {
        RETURN x + 1;
    }

    DEFUN double(x) {
        RETURN x * 2;
    }

    # Function composition using |> operator
    SET result = 5 |> add_one |> double;
    PRINTLN result;  # Should print 12 (first add 1 to 5, then double 6)
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "12"
    assert captured.out.strip() == expected_output

def test_prefix_postfix_operations(capsys, parser, executor):
    code = """
    SET i = 5;
    SET j = 10;
    
    # Test postfix increment/decrement
    PRINTLN i++;  # Should print 5, then i becomes 6
    PRINTLN i;    # Should print 6
    PRINTLN j--;  # Should print 10, then j becomes 9
    PRINTLN j;    # Should print 9
    
    # Test prefix increment/decrement
    PRINTLN ++i;  # Should print 7 (increment first, then print)
    PRINTLN --j;  # Should print 8 (decrement first, then print)
    
    # Test in expressions
    SET k = i++ + ++j;  # i is 7, becomes 8; j is 8, becomes 9; k should be 7 + 9 = 16
    PRINTLN k;    # Should print 16
    PRINTLN i;    # Should print 8
    PRINTLN j;    # Should print 9
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "5\n6\n10\n9\n7\n8\n16\n8\n9"
    assert captured.out.strip() == expected_output

def test_increment_decrement_in_expressions(capsys, parser, executor):
    code = """
    SET i = 5;
    SET j = 10;
    
    # Test increment/decrement in print statements
    PRINTLN i++;  # Should print 5, then i becomes 6
    PRINTLN ++j;  # Should print 11 (j is incremented first)
    
    # Test increment/decrement in assignments
    SET a = i++;  # a gets 6, then i becomes 7
    SET b = ++j;  # j becomes 12, then b gets 12
    
    # Print final values
    PRINTLN "i=" + i;  # Should print i=7
    PRINTLN "j=" + j;  # Should print j=12
    PRINTLN "a=" + a;  # Should print a=6
    PRINTLN "b=" + b;  # Should print b=12
    
    # Test in complex expressions
    SET c = (i++ + ++j) * 2;  # i is 7->8, j is 12->13, so (7 + 13) * 2 = 40
    PRINTLN "c=" + c;  # Should print c=40
    PRINTLN "i=" + i;  # Should print i=8
    PRINTLN "j=" + j;  # Should print j=13
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "5\n11\ni=7\nj=12\na=6\nb=12\nc=40\ni=8\nj=13"
    assert captured.out.strip() == expected_output
