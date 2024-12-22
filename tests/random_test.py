import re
from pprint import pprint


class Lexer:
    def __init__(self, source_code):
        self.keywords = [
            "MOVE",
            "PRINTLN",
            "PRINT",
            "TO",
            "MOUSE",
            "WAIT",
            "RELEASE",
            "KEY",
            "HOLD",
            "PRESS",
            "BUTTON",
        ]

        self.keyboard_keys = [
            "[a-zA-Z0-9]",
            "F(?:1[0-2]|[1-9])",
            "FN",
            "BACKSPACE",
            "ENTER",
            "SPACE",
            "TAB",
            "(?:L|R|)CTRL",
            "(?:L|R|)ALT",
            "(?:L|R|)SHIFT",
            "DEL",
            "DELETE",
            "END",
            "HOME",
            "INSERT",
            "PG_(?:U|DOWN)",
            "ARROW_(?:LEFT|RIGHT|UP|DOWN)",
            "ESC",
            "GRAVE",
            "`",
            "CAPS_LOCK",
            "(?:L|R|)WIN",
        ]

        self.mouse_keys = ["LEFT", "RIGHT"]

        self.source_code = source_code
        self.tokens = []

        # Updated token specs to handle comments more robustly
        self.token_specs = [
            ("COMMENT", r"--.*"),  # Capture entire comment line
            ("NUMBER_WITH_UNIT", r"\b\d+[smh]\b"),
            ("NUMBER", r"\b\d+\b"),
            ("STRING", r'"[^\"]*"'),
            ("KEYWORD", r"\b(?:" + "|".join(self.keywords) + r")\b"),
            ("KEYBOARD_KEY", r"\b(?:" + "|".join(self.keyboard_keys) + r")\b"),
            ("MOUSE_KEY", r"\b(?:" + "|".join(self.mouse_keys) + r")\b"),
            ("ID", r"\b[a-zA-Z_][a-zA-Z0-9_]*\b"),
            ("ASSIGN", r"="),
            ("END", r";"),
            ("OP", r"[+\-*/]"),
            ("LPAREN", r"\("),
            ("RPAREN", r"\)"),
            ("COMMA", r","),
            ("NEWLINE", r"\n"),
            ("SKIP", r"[ \t]+"),
            ("MISMATCH", r"."),
        ]

    def tokenize(self):
        token_regex = "|".join(f"(?P<{pair[0]}>{pair[1]})" for pair in self.token_specs)
        for match in re.finditer(token_regex, self.source_code):
            kind = match.lastgroup
            value = match.group()

            if kind in ["COMMENT", "SKIP", "NEWLINE"]:
                continue

            if kind == "NUMBER":
                value = int(value)
            elif kind == "NUMBER_WITH_UNIT":
                unit = value[-1]
                number = int(value[:-1])
                value = (number, unit)
            elif kind == "MISMATCH":
                raise SyntaxError(f"Unexpected character: {value}")

            self.tokens.append((kind, value))

        self.tokens.append(("EOF", None))
        return self.tokens


class Parser:
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0

    def peek(self):
        return self.tokens[self.pos]

    def advance(self):
        self.pos += 1

    def parse(self):
        """Parse the tokens and return an abstract syntax tree (AST)."""
        ast = []
        while self.peek()[0] != "EOF":
            ast.append(self.parse_statement())
        return ast

    def parse_statement(self):
        """Parse a single statement."""
        token = self.peek()

        # Handle print statements
        if token[0] == "KEYWORD" and token[1] == "PRINTLN":
            self.consume("KEYWORD")
            return self.parse_print_statement()

        # Handle wait statements
        elif token[0] == "KEYWORD" and token[1] == "WAIT":
            self.consume("KEYWORD")
            return self.parse_wait_statement()

        # Handle mouse movement
        elif token[0] == "KEYWORD" and token[1] == "MOVE":
            self.consume("KEYWORD")
            return self.parse_move_statement()

        # Handle key operations
        elif token[0] == "KEYWORD" and token[1] in ["HOLD", "RELEASE", "PRESS"]:
            return self.parse_key_operation()

        # Handle button operations
        elif (
            token[0] == "KEYWORD"
            and token[1] == "PRESS"
            and self.tokens[self.pos + 1][1] == "BUTTON"
        ):
            return self.parse_button_operation()

        else:
            raise SyntaxError(f"Unexpected token: {token}")

    def parse_print_statement(self):
        """Parse a print statement."""
        string_value = self.consume("STRING")[1]
        self.consume("END")
        return ("print", string_value)

    def parse_wait_statement(self):
        """Parse a wait statement with a time unit."""
        time_value, unit = self.consume("NUMBER_WITH_UNIT")[1]
        self.consume("END")
        return ("wait", time_value, unit)

    def parse_move_statement(self):
        """Parse a mouse movement statement."""
        # Expect MOUSE and TO keywords
        self.consume("KEYWORD")  # MOUSE
        self.consume("KEYWORD")  # TO

        # Parse coordinates
        self.consume("LPAREN")
        x = self.consume("NUMBER")[1]
        self.consume("COMMA")
        y = self.consume("NUMBER")[1]
        self.consume("RPAREN")
        self.consume("END")

        return ("move_mouse", x, y)

    def parse_key_operation(self):
        """Parse key operations like HOLD, RELEASE, PRESS KEY"""
        operation = self.consume("KEYWORD")[1]
        self.consume("KEYWORD")  # KEY
        try:
            key = self.consume("KEYBOARD_KEY")[1]
            operation_type = "key_operation"
            self.consume("END")
        except:
            key = self.consume("MOUSE_KEY")[1]
            operation_type = "mouse_operation"
            self.consume("END")

        return (operation_type, operation, key)

    def parse_button_operation(self):
        """Parse button operations like PRESS BUTTON LEFT"""
        self.consume("KEYWORD")  # PRESS
        self.consume("KEYWORD")  # BUTTON
        button = self.consume("KEYWORD")[1]
        self.consume("END")

        return ("button_operation", button)

    def consume(self, expected_type):
        token = self.peek()
        if token[0] == expected_type:
            self.advance()
            return token
        else:
            raise SyntaxError(f"Expected {expected_type}, got {token}")


# 3. Executor Class
class Executor:
    def __init__(self, ast):
        self.ast = ast

    def execute(self):
        """Execute the AST by walking through it."""
        for statement in self.ast:
            self.execute_statement(statement)

    def execute_statement(self, statement):
        """Execute a single statement."""
        if statement[0] == "assign":
            _, var_name, expr = statement
            value = self.evaluate_expression(expr)
            print(f"Assigning {value} to variable '{var_name}'")
        elif statement[0] == "print":
            _, string_value = statement
            print(f"PRINTING: {string_value}")
        elif statement[0] == "wait":
            _, time_value, unit = statement
            print(f"Waiting for {time_value}{unit}")

        else:
            raise RuntimeError(f"Unknown statement: {statement}")

    def evaluate_expression(self, expr):
        """Evaluate an expression and return its value."""
        if expr[0] == "number":
            return expr[1]
        elif expr[0] == "binop":
            _, op, left, right = expr
            left_val = self.evaluate_expression(left)
            right_val = self.evaluate_expression(right)
            if op == "+":
                return left_val + right_val
            elif op == "-":
                return left_val - right_val
            elif op == "*":
                return left_val * right_val
            elif op == "/":
                return left_val / right_val
            else:
                raise RuntimeError(f"Unknown operator: {op}")
        else:
            raise RuntimeError(f"Unknown expression: {expr}")


# Example usage
if __name__ == "__main__":
    # Load the source code from the file
    source_code = """PRINTLN "Test Case 1: Basic Mouse and Key Operations";
HOLD KEY SHIFT;
MOVE MOUSE TO (300, 400);
PRESS BUTTON LEFT;
WAIT 2s;
MOVE MOUSE TO (500, 600);
WAIT 3m;
RELEASE KEY SHIFT; -- Release SHIFT key"""

    # Tokenize
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()
    print("Tokens:", tokens)

    # Parse
    parser = Parser(tokens)
    ast = parser.parse()
    print("\n\nAST:", end="")
    pprint(ast)

    # Execute
    executor = Executor(ast)
    executor.execute()
