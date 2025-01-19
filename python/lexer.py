import re
import logging
from .errors import InvalidNumberError, SyntaxError
from .ast_nodes import Token

# Configure logger for this module
logger = logging.getLogger(__name__)

class Lexer:
    def __init__(self, source_code):
        # Core language keywords
        self.keywords = {
            "SET",
            "DEFUN",
            "LAMBDA",
            "IF",
            "THEN", 
            "ELSE",
            "ELSEIF",
            "ENDIF",
            "TIMES",
            "RETURN",
            "BREAK", 
            "CONTINUE",
            "YIELD",
            "PASS",
            "POINT"
        }

        self.loop_keywords = {
            "REPEAT",
            "WHILE",
        }

        # I/O and interaction keywords
        self.io_keywords = {
            "PRINTLN",
            "PRINT", 
            "INPUT",
            "OPEN",
            "WRITE",
            "RUN",
        }

        # Mouse and keyboard control keywords
        self.input_control_keywords = {
            "MOVE",
            "MOUSE",
            "WAIT",
            "RELEASE",
            "HOLD",
            "PRESS",
            "FOCUS",
            "SCROLL",
        }

        # Error handling keywords
        self.error_keywords = {
            "TRY",
            "EXCEPT", 
            "FINALLY",
            "ERROR",
        }

        # Control flow keywords
        self.control_keywords = {
            "EXIT",
            "CONTINUE", 
            "BREAK",
            "RAISE",
        }

        # Generator keywords
        self.generator_keywords = {
            "YIELD",
            "FROM",
        }

        # Type keywords
        self.type_keywords = {
            "APP",
            "WINDOW",
            "KEY",
            "BUTTON",
        }

        # Target/direction keywords
        self.target_keywords = {
            "TO",
        }

        # Assertion/comparison keywords
        self.assertion_keywords = {
            "EXISTS",
            "IS",
            "IN",
            "AT",
        }

        # Keyboard key definitions
        self.keyboard_keys = {
            "FN",
            "BACKSPACE",
            "ENTER", 
            "SPACE",
            "TAB",
            "(?:L|R|)CTRL",
            "(?:L|R|)ALT",
            "(?:L|R|)SHIFT",
            "(?:L|R|)WIN",
            "DEL",
            "DELETE",
            "END",
            "HOME",
            "INSERT",
            "PG_(?:U|DOWN)",
            "ARROW_(?:LEFT|RIGHT|UP|DOWN)",
            "ESC",
            "CAPS_LOCK",
            "[a-zA-Z]",
            "F(?:1[0-2]|[1-9])",
        }

        # Mouse button definitions
        self.mouse_keys = {
            "LEFT",
            "RIGHT", 
            "MIDDLE",
            "WHEEL_UP",
            "WHEEL_DOWN",
            "SCROLL_UP",  # how many times to scroll up (extension of WHEEL_UP)
            "SCROLL_DOWN",  # how many times to scroll down (extension of WHEEL_DOWN)
        }

        # Boolean literals
        self.boolean_values = {
            "TRUE",
            "FALSE"
        }

        self.source_code = source_code
        self.tokens = []
        self.current_pos = 0  # Initialize current_pos
        self.line_num = 1  # Initialize line number

        # Define token patterns as a dictionary for more efficient lookup
        self.token_patterns = {
            # Comments - both single line (#) and multi-line (#* *#)
            "COMMENT": r"#\*[\s\S]*?\*#|#.*",

            # Keywords (only pure uppercase or lowercase)
            "TYPE_KEYWORD": r"\b(?:" + "|".join([f"({k}|{k.lower()})" for k in self.type_keywords]) + r")\b",
            "KEYWORD": r"\b(?:" + "|".join([f"({k}|{k.lower()})" for k in (self.keywords | self.io_keywords | self.input_control_keywords | self.error_keywords)]) + r")\b",
            "KEYWORD_ASSERTION": r"\b(?:" + "|".join([f"({k}|{k.lower()})" for k in self.assertion_keywords]) + r")\b",
            "KEYWORD_TARGET": r"\b(?:" + "|".join([f"({k}|{k.lower()})" for k in self.target_keywords]) + r")\b",
            
            # Special keys (case-sensitive)
            "KEYBOARD_KEY": r"\b(?:" + "|".join(self.keyboard_keys) + r")\b",
            "MOUSE_KEY": r"\b(?:" + "|".join(self.mouse_keys) + r")\b",

            # Loop keywords
            "LOOP": r"\b(?:" + "|".join([f"({k}|{k.lower()})" for k in self.loop_keywords]) + r")\b",
            
            # Literals
            "FLOAT": r"\b\d+\.\d+\b",
            "INT": r"\b\d+\b",
            "TIME": r"\b\d+(?:\.\d+)?[smh]\b|\b\d+ms\b", 
            "STR": r'(["\'])(?:\\.|[^\\\1])*?\1',
            "BOOL": r"\b(?:TRUE|FALSE|true|false)\b",

            # Increment/decrement operators
            "INCREMENT": r"\+\+",
            "DECREMENT": r"--",

            # Identifiers
            "ID": r"\b[a-zA-Z_][a-zA-Z0-9_]*\b",

            # Operators (order matters - longer patterns first)
            "COMP_OP": r"===|!==|==|!=|<=|>=|<|>|\|>",
            "OP_ASSIGN": r"\+=|-=|\*=|/=|%=|&=|\^=|<<=|>>=|=",
            "BITWISE_OP": r"\||&|\^|~|<<|>>",
            "OP": r"\*\*|//|[+\-*/]",  # Added ** and // as single operators
            "TERMINATOR": r";",

            # Parentheses
            "L_PAREN": r"\(",
            "R_PAREN": r"\)",

            # Braces
            "L_BRACE": r"\{",
            "R_BRACE": r"\}",

            # Commas
            "COMMA": r",",

            # Type Hint Token
            "TYPE_HINT": r":",

            # Newlines and whitespace
            "NEWLINE": r"\n",
            "SKIP": r"[ \t]+",
            "MISMATCH": r"."
        }

        # Update the time unit pattern in token_patterns
        self.token_patterns['TIME'] = r'(\d+(?:\.\d+)?)(ms|s|m|h)'
        
        # Compile the regex pattern with named groups
        self.token_regex = "|".join(f"(?P<{kind}>{pattern})" for kind, pattern in self.token_patterns.items())

        logger.debug("Lexer initialized with source code length %d.", len(source_code))

    def tokenize(self):
        logger.info("Starting tokenization process.")
        previous_token = None

        for match in re.finditer(self.token_regex, self.source_code):
            kind = match.lastgroup
            value = match.group() 

            # Start of Selection
            logger.debug(
                "Matched token: %s with value '%s' at line %d.",
                kind,
                value.encode('unicode_escape').decode(),
                self.line_num
            )

            if kind == "NEWLINE":
                self.line_num += 1
                logger.debug("Encountered NEWLINE. Incremented line number to %d.", self.line_num)
                continue
            elif kind in ["COMMENT", "SKIP"]:
                logger.debug("Skipping token of kind '%s'.", kind)
                continue

            if kind == "FLOAT":
                try:
                    value = float(value)
                except ValueError:
                    logger.error("Invalid FLOAT value '%s' at line %d.", value, self.line_num)
                    raise InvalidNumberError(f"Invalid FLOAT value '{value}' at line {self.line_num}")
            elif kind == "INT":
                value = int(value)
            elif kind == "TIME":
                value = self.process_time_with_unit(value)
                kind = "TIME"
            elif kind == "BOOL":
                value = value.upper() == "TRUE"  # Convert to Python boolean, case-insensitive
            elif kind == "MISMATCH":
                error_msg = f"Unexpected character at line {self.line_num}: {value}"
                logger.error(error_msg)
                raise SyntaxError(error_msg)

            # Convert keyword values to uppercase for consistency
            if kind in ["KEYWORD", "TYPE_KEYWORD", "KEYWORD_ASSERTION", "KEYWORD_TARGET", "LOOP"]:
                value = value.upper()

            # Handle KEYBOARD_KEY based on previous token
            if kind == "KEYBOARD_KEY":
                if previous_token and previous_token.kind == "TYPE_KEYWORD" and previous_token.value == "KEY":
                    logger.debug("Valid KEYBOARD_KEY '%s' found.", value)
                    pass  # Valid KEYBOARD_KEY
                else:
                    logger.debug("Invalid KEYBOARD_KEY context for '%s'. Treating as ID.", value)
                    kind = "ID"  # Treat as ID if not valid in context

            # Create Token with line number
            current_token = Token(kind, value, self.line_num, previous_token)
            if previous_token:
                previous_token.next_token = current_token
            self.tokens.append(current_token)
            logger.debug("Appended token: %s, line: %d", current_token, self.line_num)
            previous_token = current_token

        # Add EOF token with current line number
        eof_token = Token("EOF", None, self.line_num, previous_token)
        if previous_token:
            previous_token.next_token = eof_token
        self.tokens.append(eof_token)
        logger.debug("Appended EOF token at line %d.", self.line_num)

        logger.info("Tokenization process completed successfully.")
        return self.connect_tokens()

    def process_time_with_unit(self, match_str):
        logger.debug("Processing TIME token '%s'.", match_str)
        time_match = re.match(r'(\d+(?:\.\d+)?)(ms|s|m|h)', match_str)
        value = float(time_match.group(1))
        unit = time_match.group(2)

        if unit == 'ms':
            value = value / 1000  # milliseconds to seconds
        elif unit == 'm':
            value = value * 60    # minutes to seconds
        elif unit == 'h':
            value = value * 3600  # hours to seconds
        # 's' doesn't need conversion

        logger.debug("Converted TIME token '%s' to (%f, '%s').", match_str, value, 's')
        return (value, 's')

    def connect_tokens(self):
        # This method ensures that each token has correct previous and next references
        logger.debug("Connecting tokens for bidirectional traversal.")
        for i in range(len(self.tokens)):
            if i > 0:
                self.tokens[i].previous_token = self.tokens[i - 1]
            if i < len(self.tokens) - 1:
                self.tokens[i].next_token = self.tokens[i + 1]
        logger.debug("Tokens connected successfully.")
        return self.tokens

    def parse_numbers(self, number_str):
        # Check if the number is an integer
        try:
            int_value = int(number_str)
            logger.debug("Parsed integer number: %d", int_value)
            return int_value
        except ValueError:
            pass

        # Check if the number is a float
        try:
            float_value = float(number_str)
            logger.debug("Parsed float number: %f", float_value)
            return float_value
        except ValueError:
            pass

        error_msg = f"'{number_str}' is not a valid number."
        logger.error(error_msg)
        raise InvalidNumberError(error_msg)

    def get_next_token(self):
        if not self.tokens:
            logger.debug("Token list empty. Starting tokenization.")
            self.tokenize()

        if self.current_pos < len(self.tokens):
            token = self.tokens[self.current_pos]
            self.current_pos += 1
            logger.debug("Retrieved next token: %s", token)
            return token
        else:
            logger.debug("No more tokens. Returning EOF.")
            return Token("EOF", None, self.line_num)