# test_advanced_features.py
import pytest
from lexer import Lexer
from parser import Parser
from executor import Executor

def test_class_creation_and_instance(capsys, parser, executor):
    code = """
    CLASS MyClass {
        SET x = 10;
        DEFUN get_x(self) {
            RETURN self.x;
        }
    };
    INSTANTIATE MyClass AS obj;
    PRINTLN obj.get_x();
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    assert captured.out.strip() == "10"

def test_class_with_constructor(capsys, parser, executor):
    code = """
    CLASS MyClass {
        CONSTRUCTOR(self, val) {
            self.x = val;
        }
        DEFUN get_x(self) {
            RETURN self.x;
        }
    };
    INSTANTIATE MyClass WITH (20) AS obj;
    PRINTLN obj.get_x();
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    assert captured.out.strip() == "20"

def test_class_with_destructor(capsys, parser, executor):
    code = """
        CLASS MyClass {
        CONSTRUCTOR(self, val) {
            self.x = val;
        }
        DESTRUCTOR {
            PRINTLN "Destructor called";
        }
        DEFUN get_x(self) {
            RETURN self.x;
        }
    };
    INSTANTIATE MyClass WITH (20) AS obj;
    PRINTLN obj.get_x();
    DESTROY obj;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "20\nDestructor called"
    assert captured.out.strip() == expected_output

def test_try_except_finally(capsys, parser, executor):
    code = """
    TRY {
        PRINTLN "Inside Try";
        RAISE ERROR("Test Error");
    } EXCEPT ERROR AS e{
        PRINTLN "Caught Exception: " + e;
    } FINALLY {
        PRINTLN "Finally Block";
    }
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "Inside Try\nCaught Exception: Test Error\nFinally Block"
    assert captured.out.strip() == expected_output
    
def test_try_except_no_error(capsys, parser, executor):
    code = """
    TRY {
        PRINTLN "Inside Try";
        SET x = 10;
    } EXCEPT ERROR AS e{
        PRINTLN "Caught Exception: " + e;
    } FINALLY {
        PRINTLN "Finally Block";
    }
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "Inside Try\nFinally Block"
    assert captured.out.strip() == expected_output
    
def test_try_except_no_catch(capsys, parser, executor):
    code = """
    TRY {
        PRINTLN "Inside Try";
        RAISE ERROR("Test Error");
    } FINALLY {
        PRINTLN "Finally Block";
    }
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    with pytest.raises(Exception):
         executor.execute(ast)

def test_ocr_read_screen(capsys, parser, executor):
     code = """
     SET ocr_test = READ SCREEN FROM (10, 10) TO (100, 100);
     IF ocr_test CONTAINS "text" THEN {
         PRINTLN "text found";
     } ELSE IF ocr_test CONTAINS "abc" THEN {
        PRINTLN "abc found";
     } ELSE {
        PRINTLN "Nothing found";
     }
     """
     lexer = Lexer(code)
     tokens = lexer.tokenize()
     ast = parser.parse(tokens)
     executor.screen_manager.add_text_on_screen("text", 10, 10, 100, 100)
     executor.execute(ast)
     captured = capsys.readouterr()
     expected_output = "text found"
     assert captured.out.strip() == expected_output

def test_ocr_detect_object(capsys, parser, executor):
     code = """
     OPEN APP "Calculator";
     SET cords = DETECT OBJECT "7";
     PRINTLN cords;
     """
     lexer = Lexer(code)
     tokens = lexer.tokenize()
     ast = parser.parse(tokens)
     executor.object_detector.add_object("7", 50, 50, 20, 20)
     executor.execute(ast)
     captured = capsys.readouterr()
     expected_output = "(50, 50)"
     assert captured.out.strip() == expected_output