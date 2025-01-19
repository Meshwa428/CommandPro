# test_system_interaction.py
import pytest
from lexer import Lexer
from parser import Parser
from executor import Executor
import time
import os

def test_mouse_movement(parser, executor):
    code = """
    MOVE MOUSE TO (100, 200);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    assert executor.mouse_manager.current_position == (100, 200)


def test_mouse_click(capsys, parser, executor):
    code = """
    CLICK LEFT (50, 50);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    assert executor.mouse_manager.current_position == (50, 50)
    assert executor.mouse_manager.buttons_pressed["left"] == True

def test_mouse_drag(parser, executor):
    code = """
    DRAG MOUSE FROM (10, 10) TO (30, 30);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    assert executor.mouse_manager.current_position == (30, 30)

def test_mouse_hold_release(capsys, parser, executor):
    code = """
    HOLD BUTTON LEFT;
    PRINTLN executor.mouse_manager.buttons_pressed;
    RELEASE BUTTON LEFT;
    PRINTLN executor.mouse_manager.buttons_pressed;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "{'left': True}\n{'left': False}"
    assert captured.out.strip() == expected_output

def test_keyboard_press_release(capsys, parser, executor):
    code = """
    PRESS KEY "A";
    PRINTLN executor.keyboard_manager.keys_pressed;
    PRESS KEY ENTER;
    PRINTLN executor.keyboard_manager.keys_pressed;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "{'A': True}\n{'A': False, 'enter': True}"
    assert captured.out.strip() == expected_output

def test_keyboard_hold_release(capsys, parser, executor):
     code = """
     HOLD KEY SHIFT;
     PRINTLN executor.keyboard_manager.keys_pressed;
     RELEASE KEY SHIFT;
     PRINTLN executor.keyboard_manager.keys_pressed;
     """
     lexer = Lexer(code)
     tokens = lexer.tokenize()
     ast = parser.parse(tokens)
     executor.execute(ast)
     captured = capsys.readouterr()
     expected_output = "{'shift': True}\n{'shift': False}"
     assert captured.out.strip() == expected_output

def test_keyboard_write(capsys, parser, executor):
    code = """
    WRITE "Hello, World!";
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    assert executor.keyboard_manager.text_written == "Hello, World!"

def test_open_close_window(capsys, parser, executor):
    code = """
    OPEN APP "Notepad";
    PRINTLN executor.window_manager.windows;
    CLOSE WINDOW "Notepad";
    PRINTLN executor.window_manager.windows;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "{'Notepad': {'x': 0, 'y': 0, 'width': 800, 'height': 600}}\n{}"
    assert captured.out.strip() == expected_output

def test_focus_window(capsys, parser, executor):
    code = """
    OPEN APP "Notepad";
    OPEN APP "Calculator";
    FOCUS WINDOW "Calculator";
    PRINTLN executor.window_manager.focused_window;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    assert captured.out.strip() == 'Calculator'

def test_move_window(parser, executor):
    code = """
    OPEN APP "Notepad";
    MOVE WINDOW "Notepad" TO (100, 150);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    assert executor.window_manager.windows["Notepad"]["x"] == 100
    assert executor.window_manager.windows["Notepad"]["y"] == 150

def test_drag_window(parser, executor):
    code = """
    OPEN APP "Notepad";
    DRAG WINDOW "Notepad" TO (50, 50);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    assert executor.window_manager.windows["Notepad"]["x"] == 50
    assert executor.window_manager.windows["Notepad"]["y"] == 50
    
def test_resize_window(parser, executor):
    code = """
    OPEN APP "Notepad";
    RESIZE WINDOW "Notepad" TO (400, 300);
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    assert executor.window_manager.windows["Notepad"]["width"] == 400
    assert executor.window_manager.windows["Notepad"]["height"] == 300

def test_window_minimize_maximize_restore(capsys, parser, executor):
    code = """
    OPEN APP "Notepad";
    MINIMIZE WINDOW "Notepad";
    PRINTLN executor.window_manager.windows["Notepad"]["state"];
    MAXIMIZE WINDOW "Notepad";
    PRINTLN executor.window_manager.windows["Notepad"]["state"];
    RESTORE WINDOW "Notepad";
    PRINTLN executor.window_manager.windows["Notepad"]["state"];
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "'minimized'\n'maximized'\n'normal'"
    assert captured.out.strip() == expected_output

def test_input_output(capsys, parser, executor):
     code = """
     INPUT "Enter your name: " INTO name AS STR;
     INPUT "Enter your age: " INTO age AS INT;
     PRINTLN "Name: " + name;
     PRINTLN "Age: " + age;
     """
     lexer = Lexer(code)
     tokens = lexer.tokenize()
     ast = parser.parse(tokens)
     executor.user_input = ["John Doe", "30"]
     executor.execute(ast)
     captured = capsys.readouterr()
     expected_output = "Name: John Doe\nAge: 30"
     assert captured.out.strip() == expected_output

def test_screen_capture(parser, executor):
    code = """
    CAPTURE SCREEN INTO "test_screenshot.png";
    CAPTURE SCREEN FROM (10, 10) TO (100, 100) INTO "test_partial.png";
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    assert os.path.exists("test_screenshot.png") == True
    assert os.path.exists("test_partial.png") == True
    os.remove("test_screenshot.png")
    os.remove("test_partial.png")

def test_wait_command(capsys, parser, executor):
    code = """
    SET start_time = executor.time_manager.current_time;
    WAIT 2s;
    SET end_time = executor.time_manager.current_time;
    PRINTLN (end_time - start_time) > 1;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    assert captured.out.strip() == "True"

def test_run_at_command(capsys, parser, executor):
     code = """
     RUN AT "12:00 AM" {
         PRINTLN "Scheduled Task";
     };
     SET current_time = TIME("12:00 AM");
     PRINTLN executor.time_manager.schedule;
     executor.time_manager.run_scheduled_tasks(current_time)
     """
     lexer = Lexer(code)
     tokens = lexer.tokenize()
     ast = parser.parse(tokens)
     executor.execute(ast)
     captured = capsys.readouterr()
     expected_output = "{'12:00 AM': [<executor.ASTNode object at 0x00000207446C6E90>]}\nScheduled Task"
     assert captured.out.strip() == expected_output

def test_interval_command(capsys, parser, executor):
    code = """
    SET counter = 0;
    INTERVAL 1s {
        counter++;
        IF counter == 3 {
            PRINTLN "Interval Task Executed 3 Times";
        }
    };
    SET current_time = TIME("12:00:00 AM");
    executor.time_manager.run_scheduled_tasks(current_time);
    WAIT 1s;
    SET current_time = TIME("12:00:01 AM");
    executor.time_manager.run_scheduled_tasks(current_time);
    WAIT 1s;
    SET current_time = TIME("12:00:02 AM");
    executor.time_manager.run_scheduled_tasks(current_time);
    WAIT 1s;
    """
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    ast = parser.parse(tokens)
    executor.execute(ast)
    captured = capsys.readouterr()
    expected_output = "Interval Task Executed 3 Times"
    assert captured.out.strip() == expected_output