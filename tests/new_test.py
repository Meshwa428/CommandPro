# import re
# from collections import namedtuple

# # Define Token structure
# Token = namedtuple("Token", ["type", "value", "line", "column"])


# class Lexer:
#     def __init__(self, code):
#         self.code = code
#         self.tokens = []
#         self.line = 1
#         self.column = 1

#         # Define token patterns
#         self.token_specification = [
#             (
#                 "KEYWORD",
#                 r"\b(MOVE|SET|PRINT|WAIT|IF|END IF|DEFFUN|REPEAT)\b",
#             ),  # Keywords must appear first
#             (
#                 "IDENTIFIER",
#                 r"[a-zA-Z_][a-zA-Z0-9_]*",
#             ),  # Variables and other identifiers
#             ("NUMBER", r"\d+"),
#             ("STRING", r'"(?:\\.|[^"\\])*"'),
#             ("OPERATOR", r"[+\-*/=]"),
#             ("DELIMITER", r"[;]"),
#             ("PAREN", r"[()]"),
#             ("COMMA", r","),
#             ("WHITESPACE", r"[ \t]+"),
#             ("NEWLINE", r"\n"),
#             ("MISMATCH", r"."),
#         ]

#         self.token_regex = "|".join(
#             f"(?P<{name}>{pattern})" for name, pattern in self.token_specification
#         )

#     def tokenize(self):
#         for match in re.finditer(self.token_regex, self.code):
#             kind = match.lastgroup
#             value = match.group(kind)
#             column_start = self.column
#             self.column += len(value)

#             if kind == "NEWLINE":
#                 self.line += 1
#                 self.column = 1
#                 continue
#             elif kind == "WHITESPACE":
#                 continue
#             elif kind == "MISMATCH":
#                 raise SyntaxError(
#                     f"Unexpected character: {value} at {self.line}:{self.column}"
#                 )

#             token = Token(kind, value, self.line, column_start)
#             self.tokens.append(token)
#         return self.tokens


# class Parser:
#     def __init__(self, tokens):
#         self.tokens = tokens
#         self.current_token_index = 0
#         self.current_token = self.tokens[self.current_token_index]

#     def advance(self):
#         self.current_token_index += 1
#         if self.current_token_index < len(self.tokens):
#             self.current_token = self.tokens[self.current_token_index]

#     def consume(self, expected_type):
#         if self.current_token.type == expected_type:
#             self.advance()
#         else:
#             raise SyntaxError(
#                 f"Expected {expected_type}, found {self.current_token.type}"
#             )

#     def parse(self):
#         return self.program()

#     def program(self):
#         statements = []
#         while self.current_token.type != "EOF":
#             if self.current_token.type == "DELIMITER":
#                 self.advance()
#             statements.append(self.statement())
#         return statements

#     def statement(self):
#         if self.current_token.type == "KEYWORD" and self.current_token.value == "SET":
#             return self.set_statement()
#         elif (
#             self.current_token.type == "KEYWORD"
#             and self.current_token.value == "PRINTLN"
#         ):
#             return self.print_statement()
#         else:
#             raise SyntaxError(f"Unexpected token: {self.current_token}")

#     def set_statement(self):
#         self.consume("KEYWORD")
#         var_name = self.current_token.value
#         self.consume("IDENTIFIER")
#         self.consume("OPERATOR")
#         value = self.expression()
#         return {"type": "SET", "var_name": var_name, "value": value}

#     def print_statement(self):
#         self.consume("KEYWORD")
#         expr = self.expression()
#         return {"type": "PRINTLN", "expression": expr}

#     def expression(self):
#         if self.current_token.type == "NUMBER":
#             value = int(self.current_token.value)
#             self.advance()
#             return {"type": "NUMBER", "value": value}
#         elif self.current_token.type == "STRING":
#             value = self.current_token.value.strip('"')
#             self.advance()
#             return {"type": "STRING", "value": value}
#         elif self.current_token.type == "IDENTIFIER":
#             value = self.current_token.value
#             self.advance()
#             return {"type": "VARIABLE", "name": value}
#         else:
#             raise SyntaxError(f"Invalid expression starting with {self.current_token}")


# class Interpreter:
#     def __init__(self):
#         self.variables = {}

#     def interpret(self, ast):
#         for statement in ast:
#             if statement["type"] == "SET":
#                 self.variables[statement["var_name"]] = self.evaluate(
#                     statement["value"]
#                 )
#             elif statement["type"] == "PRINTLN":
#                 print(self.evaluate(statement["expression"]))

#     def evaluate(self, expr):
#         if expr["type"] == "NUMBER":
#             return expr["value"]
#         elif expr["type"] == "STRING":
#             return expr["value"]
#         elif expr["type"] == "VARIABLE":
#             var_name = expr["name"]
#             if var_name in self.variables:
#                 return self.variables[var_name]
#             else:
#                 raise ValueError(f"Undefined variable: {var_name}")


# # Example Usage
# if __name__ == "__main__":
#     code = """\
# SET x = 10;
# SET y = "Hello, world!";
# PRINTLN x;
# PRINTLN y;
#     """

#     lexer = Lexer(code)
#     tokens = lexer.tokenize()
#     print(tokens)
#     parser = Parser(tokens)
#     ast = parser.parse()
#     interpreter = Interpreter()
#     interpreter.interpret(ast)

#     lexer = Lexer(code)
#     tokens = lexer.tokenize()
#     for token in tokens:
#         print(token)

import re
from collections import namedtuple

# Define Token structure
Token = namedtuple("Token", ["type", "value", "line", "column"])


class Lexer:
    def __init__(self, code):
        self.code = code
        self.tokens = []
        self.line = 1
        self.column = 1

        # Define token patterns
        self.token_specification = [
            ("COMMENT_SINGLE", r"--[^\n]*"),  # Single-line comments
            ("COMMENT_MULTI", r"-\*.*?\*-"),  # Multi-line comments
            (
                "KEYWORD",
                r"\b(MOVE|SET|PRINT|PRINTLN|WAIT|IF|ELSE IF|ELSE|DEFFUN|REPEAT)\b",
            ),  # Keywords
            (
                "IDENTIFIER",
                r"[a-zA-Z_][a-zA-Z0-9_]*",
            ),  # Variables and other identifiers
            ("NUMBER", r"\d+"),  # Integer numbers
            (
                "FLOAT",
                r"\d+\.\d+(?=\s*(h|m|s|ms|$))",
            ),  # Float numbers followed by time units or end (modified)
            ("TIME", r"\d+\.\d*(h|m|s|ms)"),  # Time units (modified)
            ("PERIOD", r"[.]"),
            ("STRING", r'"(?:\\.|[^"\\])*"'),  # Strings
            ("OPERATOR", r"[+\-*/=<>!]=?|&&|\|\|"),  # Operators
            ("DELIMITER", r"[;]"),  # Delimiters
            ("PAREN", r"[()]"),  # Parentheses
            ("COMMA", r","),  # Comma
            ("CURLY_PAREN", r"[{}]"),  # Curly Parentheses
            ("WHITESPACE", r"[ \t]+"),  # Ignore spaces and tabs
            ("NEWLINE", r"\n"),  # Newlines
            ("MISMATCH", r"."),  # Any other character
        ]

        # Compile token patterns, applying re.DOTALL for multi-line comments
        self.token_regex = "|".join(
            f"(?P<{name}>{pattern})" for name, pattern in self.token_specification
        )

    def tokenize(self):
        """Generate tokens by matching regex patterns."""
        for match in re.finditer(self.token_regex, self.code, re.DOTALL):
            kind = match.lastgroup
            value = match.group(kind)
            column_start = self.column
            self.column += len(value)

            if kind == "NEWLINE":
                self.line += 1
                self.column = 1
                continue
            elif kind in ("WHITESPACE", "COMMENT_SINGLE", "COMMENT_MULTI"):
                # Skip whitespace and comments
                continue
            elif kind == "MISMATCH":
                raise SyntaxError(
                    f"Unexpected character: {value} at {self.line}:{self.column}"
                )

            token = Token(kind, value, self.line, column_start)
            self.tokens.append(token)
        return self.tokens


class Parser:
    def __init__(self, tokens):
        self.tokens = tokens
        self.current_token_index = 0
        self.current_token = self.tokens[self.current_token_index]

    def advance(self):
        self.current_token_index += 1
        if self.current_token_index < len(self.tokens):
            self.current_token = self.tokens[self.current_token_index]

    def consume(self, expected_type):
        if self.current_token.type == expected_type:
            self.advance()
        else:
            raise SyntaxError(
                f"Expected {expected_type}, found {self.current_token.type} at line {self.current_token.line}"
            )

    def parse(self):
        return self.program()

    def program(self):
        statements = []
        while self.current_token.type != "EOF":
            if self.current_token.type == "DELIMITER":
                self.advance()
            statements.append(self.statement())
        return statements

    def statement(self):
        if self.current_token.type == "KEYWORD" and self.current_token.value == "WAIT":
            return self.wait_statement()
        elif self.current_token.type == "KEYWORD" and self.current_token.value == "SET":
            return self.set_statement()
        elif (
            self.current_token.type == "KEYWORD"
            and self.current_token.value == "PRINTLN"
        ):
            return self.print_statement()
        else:
            raise SyntaxError(f"Unexpected token: {self.current_token}")

    def wait_statement(self):
        self.consume("KEYWORD")  # Consume the 'WAIT' keyword
        time_value = self.time_expression()
        self.consume("DELIMITER")  # Consume the semicolon
        return {"type": "WAIT", "time": time_value}

    def time_expression(self):
        if self.current_token.type == "TIME":
            time_value = self.current_token.value
            self.advance()
            return {"type": "TIME", "value": time_value}
        elif self.current_token.type == "FLOAT":
            # If a float number is found without a time unit, we can assume it's in seconds.
            float_value = self.current_token.value
            self.advance()
            return {
                "type": "TIME",
                "value": f"{float_value}s",
            }  # Assume seconds if no time unit
        else:
            raise SyntaxError(f"Invalid time expression: {self.current_token}")

    def set_statement(self):
        """Parse a SET statement."""
        self.consume("KEYWORD")
        var_name = self.current_token.value
        self.consume("IDENTIFIER")
        self.consume("OPERATOR")
        value = self.expression()
        return {"type": "SET", "var_name": var_name, "value": value}

    def print_statement(self):
        """Parse a PRINTLN statement."""
        self.consume("KEYWORD")
        expr = self.expression()
        return {"type": "PRINTLN", "expression": expr}

    def expression(self, precedence=0):
        """Parse expressions with operator precedence."""
        # Parse the first part of the expression
        left = self.primary()

        while (
            self.current_token.type == "OPERATOR"
            and self.get_precedence(self.current_token.value) >= precedence
        ):
            op = self.current_token.value
            op_precedence = self.get_precedence(op)
            self.advance()
            right = self.expression(op_precedence + 1)
            left = {"type": "BINARY_OP", "operator": op, "left": left, "right": right}

        return left

    def primary(self):
        """Parse primary expressions (numbers, strings, variables, or grouped expressions)."""
        if self.current_token.type == "NUMBER":
            value = int(self.current_token.value)
            self.advance()
            return {"type": "NUMBER", "value": value}
        elif self.current_token.type == "STRING":
            value = self.current_token.value.strip('"')
            self.advance()
            return {"type": "STRING", "value": value}
        elif self.current_token.type == "IDENTIFIER":
            value = self.current_token.value
            self.advance()
            return {"type": "VARIABLE", "name": value}
        elif self.current_token.type == "PAREN" and self.current_token.value == "(":
            self.advance()
            expr = self.expression()
            self.consume("PAREN")  # Consume closing parenthesis
            return expr
        else:
            raise SyntaxError(
                f"Invalid expression starting with {self.current_token.value} at line {self.current_token.line}, column {self.current_token.column}"
            )

    def get_precedence(self, operator):
        """Return the precedence level of an operator."""
        precedence = {
            "=": 1,
            "||": 2,
            "&&": 3,
            "==": 4,
            "!=": 4,
            "<": 5,
            ">": 5,
            "+": 6,
            "-": 6,
            "*": 7,
            "/": 7,
        }
        return precedence.get(operator, 0)


# Example usage
code = """
SET x = 5 + 3 * 2;
PRINTLN x;
"""

with open("../test.csc", "r") as file:
    code = file.read()

lexer = Lexer(code)
tokens = lexer.tokenize()

parser = Parser(tokens)
ast = parser.parse()

print(ast)
