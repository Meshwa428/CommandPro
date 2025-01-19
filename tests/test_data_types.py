# test_data_types.py
import pytest
from lexer import Lexer
from parser import Parser
from executor import Executor
import datetime


def test_contains_command(capsys, parser, executor):
    code = """
    SET test = "This is string";
    PRINT test CONTAINS "string";
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "True"
    assert captured.out.strip() == expected_output

def test_in_command(capsys, parser, executor):
    code = """
    SET test = "This is string";
    PRINT "string" IN test;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "True"
    assert captured.out.strip() == expected_output
    
def test_point_constructor_output(capsys, parser, executor):
    code = """
    PRINT POINT(123, 123);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "(123, 123)"
    assert captured.out.strip() == expected_output
    
def test_color_constructor_rgb(capsys, parser, executor):
     code = """
     PRINT COLOR(255, 0, 0);
     """
     lexer = Lexer(code)
     tokens = lexer.tokenize()
     ast = parser.parse(tokens)
     executor.execute(ast)
     captured = capsys.readouterr()
     expected_output = "(255, 0, 0)"
     assert captured.out.strip() == expected_output
     
def test_color_constructor_hex(capsys, parser, executor):
    code = """
    PRINT COLOR("#FF5733");
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "(255, 87, 51)"
    assert captured.out.strip() == expected_output
    
def test_list_constructor(capsys, parser, executor):
     code = """
     PRINT LIST(1, 2, 3, 4, 5);
     """
     lexer = Lexer(code)
     tokens = lexer.tokenize()
     ast = parser.parse(tokens)
     executor.execute(ast)
     captured = capsys.readouterr()
     expected_output = "[1, 2, 3, 4, 5]"
     assert captured.out.strip() == expected_output

def test_filepath_constructor(capsys, parser, executor):
    code = """
    PRINT FILEPATH("path/to/file");
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "path/to/file"
    assert captured.out.strip() == expected_output

def test_date_constructor_yyyy_mm_dd(capsys, parser, executor):
    code = """
    PRINT DATE("2024-11-27");
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "2024-11-27"
    assert captured.out.strip() == expected_output
    
def test_time_constructor_24hr(capsys, parser, executor):
    code = """
    PRINT TIME("14:30:00");
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "14:30:00"
    assert captured.out.strip() == expected_output

def test_time_constructor_12hr(capsys, parser, executor):
    code = """
    PRINT TIME("12:11:00 AM");
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "00:11:00"
    assert captured.out.strip() == expected_output

def test_enum_constructor(capsys, parser, executor):
     code = """
     PRINT ENUM("LEFT", "CENTER", "RIGHT");
     """
     lexer = Lexer(code)
     tokens = lexer.tokenize()
     ast = parser.parse(tokens)
     executor.execute(ast)
     captured = capsys.readouterr()
     expected_output = "['LEFT', 'CENTER', 'RIGHT']"
     assert captured.out.strip() == expected_output
     
def test_explicit_datatype_int(capsys, parser, executor):
    code = """
    INT integerValue = 10;
    PRINT integerValue;
    PRINT TYPE integerValue;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "10\n<class 'int'>"
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_float(capsys, parser, executor):
    code = """
    FLOAT pi = 3.14159;
    PRINT pi;
    PRINT TYPE pi;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "3.14159\n<class 'float'>"
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_double(capsys, parser, executor):
    code = """
    DOUBLE largeNumber = 1.23456789012;
    PRINT largeNumber;
    PRINT TYPE largeNumber;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "1.23456789012\n<class 'float'>" # double is handled as float
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_long(capsys, parser, executor):
     code = """
     LONG bigValue = 3287467823462738167826;
     PRINT bigValue;
     PRINT TYPE bigValue;
     """
     lexer = Lexer(code)
     tokens = lexer.tokenize()
     ast = parser.parse(tokens)
     executor.execute(ast)
     captured = capsys.readouterr()
     expected_output = "3287467823462738167826\n<class 'int'>"
     assert captured.out.strip() == expected_output

def test_explicit_datatype_short(capsys, parser, executor):
    code = """
    SHORT smallValue = 32767;
    PRINT smallValue;
    PRINT TYPE smallValue;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "32767\n<class 'int'>"
    assert captured.out.strip() == expected_output

def test_explicit_datatype_byte(capsys, parser, executor):
    code = """
    BYTE flag = 255;
    PRINT flag;
    PRINT TYPE flag;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "255\n<class 'int'>"
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_boolean(capsys, parser, executor):
    code = """
    BOOLEAN isActive = FALSE;
    PRINT isActive;
    PRINT TYPE isActive;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "False\n<class 'bool'>"
    assert captured.out.strip() == expected_output

def test_explicit_datatype_complex(capsys, parser, executor):
    code = """
    COMPLEX complex_value = 3 + 4i;
    PRINT complex_value;
    PRINT TYPE complex_value;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "(3+4j)\n<class 'complex'>"
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_constant_int(capsys, parser, executor):
    code = """
    FIXED INT constantInteger = 123;
    PRINT constantInteger;
    PRINT TYPE constantInteger;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "123\n<class 'int'>"
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_point(capsys, parser, executor):
    code = """
    POINT cordinate = (123, 456);
    PRINT cordinate;
    PRINT TYPE cordinate;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "(123, 456)\n<class 'tuple'>"
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_unsigned_int(capsys, parser, executor):
    code = """
    UNSIGNED INT var = 10;
    PRINT var;
    PRINT TYPE var;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "10\n<class 'int'>"
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_unsigned_float(capsys, parser, executor):
    code = """
    UNSIGNED FLOAT pi = 3.14159;
    PRINT pi;
    PRINT TYPE pi;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "3.14159\n<class 'float'>"
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_unsigned_double(capsys, parser, executor):
    code = """
    UNSIGNED DOUBLE largeNumber = 1.23456789012;
    PRINT largeNumber;
    PRINT TYPE largeNumber;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "1.23456789012\n<class 'float'>"
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_unsigned_long(capsys, parser, executor):
    code = """
    UNSIGNED LONG bigValue = 3287467823462738167826;
    PRINT bigValue;
    PRINT TYPE bigValue;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "3287467823462738167826\n<class 'int'>"
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_unsigned_short(capsys, parser, executor):
    code = """
    UNSIGNED SHORT smallValue = 32767;
    PRINT smallValue;
    PRINT TYPE smallValue;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "32767\n<class 'int'>"
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_unsigned_byte(capsys, parser, executor):
    code = """
    UNSIGNED BYTE flag = 255;
    PRINT flag;
    PRINT TYPE flag;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "255\n<class 'int'>"
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_signed_int(capsys, parser, executor):
    code = """
    SIGNED INT var = 10;
    PRINT var;
    PRINT TYPE var;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "10\n<class 'int'>"
    assert captured.out.strip() == expected_output

def test_explicit_datatype_signed_float(capsys, parser, executor):
    code = """
    SIGNED FLOAT pi = 3.14159;
    PRINT pi;
    PRINT TYPE pi;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "3.14159\n<class 'float'>"
    assert captured.out.strip() == expected_output
    
def test_explicit_datatype_signed_double(capsys, parser, executor):
    code = """
    SIGNED DOUBLE largeNumber = 1.23456789012;
    PRINT largeNumber;
    PRINT TYPE largeNumber;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "1.23456789012\n<class 'float'>"
    assert captured.out.strip() == expected_output

def test_explicit_datatype_signed_long(capsys, parser, executor):
     code = """
     SIGNED LONG bigValue = 3287467823462738167826;
     PRINT bigValue;
     PRINT TYPE bigValue;
     """
     lexer = Lexer(code)
     tokens = lexer.tokenize()
     ast = parser.parse(tokens)
     executor.execute(ast)
     captured = capsys.readouterr()
     expected_output = "3287467823462738167826\n<class 'int'>"
     assert captured.out.strip() == expected_output

def test_explicit_datatype_signed_short(capsys, parser, executor):
    code = """
    SIGNED SHORT smallValue = 32767;
    PRINT smallValue;
    PRINT TYPE smallValue;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "32767\n<class 'int'>"
    assert captured.out.strip() == expected_output

def test_explicit_datatype_signed_byte(capsys, parser, executor):
    code = """
    SIGNED BYTE flag = 255;
    PRINT flag;
    PRINT TYPE flag;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "255\n<class 'int'>"
    assert captured.out.strip() == expected_output

def test_get_datatype(capsys, parser, executor):
    code = """
    INT integerValue = 10;
    FLOAT pi = 3.14159;
    PRINT TYPE integerValue;
    PRINT TYPE pi;
    PRINT TYPE (111, 100);
    PRINT TYPE "this is string";
    PRINT TYPE 45m;
    PRINT TYPE 123;
    PRINT TYPE 123.4;
    PRINT TYPE COMPLEX();
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "<class 'int'>\n<class 'float'>\n<class 'tuple'>\n<class 'str'>\n<class 'str'>\n<class 'int'>\n<class 'float'>\n<class 'complex'>"
    assert captured.out.strip() == expected_output