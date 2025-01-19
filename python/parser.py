import logging
from .ast_nodes import ( Token,
    Program, FunctionDefinition, FunctionCall, Assignment, PrintStatement,
    WaitStatement, MoveMouse, KeyOperation, ButtonOperation,
    BinaryOperation, Identifier, Integer, Time, String, Boolean, Float,
    ASTNode, WhileLoop, RepeatLoop, ControlStatement, IncrementDecrement,
    IfStatement, MoveWindow, FocusWindow, WindowExists, LambdaFunction, Point,
    FunctionComposition, NamedArgument
)
from typing import List, Optional, Dict, Any
from .errors import SyntaxError

# Configure logger for this module
logger = logging.getLogger(__name__)

class Parser:
    def __init__(self):
        self.pos = 0
        self.precedence = {
            'OR': 1, 
            'AND': 2,
            '==': 3, '!=': 3, '<': 3, '>': 3, '<=': 3, '>=': 3, '===': 3,
            '|': 4,  # Bitwise OR
            '^': 5,  # Bitwise XOR
            '&': 6,  # Bitwise AND
            '<<': 7, '>>': 7,  # Bit shifts
            '+': 8, '-': 8,
            '*': 9, '/': 9, '//': 9, '%': 9,
            '**': 10,  # Exponentiation
            'NOT': 11, '~': 11,  # Unary operators
            'IS': 12, 'IN': 12
        }
        self.scope_stack = [{
            'variables': {},
            'functions': {}
        }]  # Initialize with global scope
        self.data_types = {
            'INT': int,
            'FLOAT': float,
            'STR': str,
            'TIME': 'time',  # Special handling for time values
            'BOOL': bool,
            'POINT': 'point'
        }
        self.global_scope = dict()
        self.functions = dict()
        self.current_context = []  # Stack to track current context (LOOP, FUNCTION)
        self.defined_functions = set()  # Initialize a set to track defined functions
        

    def peek(self):
        if self.pos < len(self.tokens):
            return self.tokens[self.pos]
        else:
            return self.tokens[-1]  # Return EOF token

    def advance(self):
        logger.debug("Advancing from token: %s", self.peek())
        self.pos += 1

    def consume(self, expected_kind):
        """Consume a token of the expected kind."""
        if self.pos >= len(self.tokens):
            logger.error("Unexpected end of input. Expected %s at end of file.", expected_kind)
            raise SyntaxError(f"Unexpected end of input. Expected {expected_kind} at end of file.")
        
        token = self.peek()
        if token.kind == expected_kind:
            self.advance()
            logger.debug("Consumed token: %s", token)
            return token
        else:
            error_msg = f"Expected {expected_kind}, got {token.kind} with value '{token.value}' at line {token.line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)

    def parse(self, tokens: List[Token]) -> Program:
        """Parse the tokens and return an abstract syntax tree (AST)."""
        self.tokens = tokens
        self.pos = 0
        logger.debug("Parser initialized with %d tokens.", len(tokens))
        logger.info("Starting parse process.")
        statements = []
        while self.peek().kind != "EOF":
            # Check if the next token is just a TERMINATOR
            if self.peek().kind == "TERMINATOR":
                logger.debug("Skipping TERMINATOR token at line %d.", self.peek().line)
                self.advance()  # Skip the terminator
                continue  # Avoid adding an EmptyStatement
            stmt = self.parse_statement()
            if stmt is not None:  # Only append non-None statements
                statements.append(stmt)
        logger.info("Parse process completed successfully.")
        return Program(statements)

    def parse_statement(self) -> Optional[ASTNode]:
        """Parse a single statement."""
        token = self.peek()
        logger.debug("Parsing statement starting with token: %s", token)

        if token.kind == "TERMINATOR":
            logger.debug("Skipping TERMINATOR token at line %d.", token.line)
            self.advance()
            return None

        elif token.kind == "KEYWORD" and token.value == "IF":
            return self.parse_if_statement()

        elif token.kind == "KEYWORD" and token.value == "DEFUN":
            self.current_context.append("FUNCTION")
            func_def = self.parse_function_definition()
            self.current_context.pop()
            return func_def

        elif token.kind == "LOOP" and token.value == "WHILE":
            self.current_context.append("LOOP")
            while_loop = self.parse_while_loop()
            self.current_context.pop()
            return while_loop

        elif token.kind == "LOOP" and token.value == "REPEAT":
            self.current_context.append("LOOP")
            repeat_loop = self.parse_repeat_loop()
            self.current_context.pop()
            return repeat_loop

        elif token.kind == "KEYWORD" and token.value in ["BREAK", "CONTINUE"]:
            if "LOOP" not in self.current_context:
                error_msg = f"{token.value} statement outside of loop at line {token.line}"
                logger.error(error_msg)
                raise SyntaxError(error_msg)
            return self.parse_control_statement()

        elif token.kind == "KEYWORD" and token.value == "RETURN":
            if "FUNCTION" not in self.current_context:
                error_msg = f"RETURN statement outside of function at line {token.line}"
                logger.error(error_msg)
                raise SyntaxError(error_msg)
            return self.parse_control_statement()

        elif token.kind == "KEYWORD" and token.value == "YIELD":
            if "FUNCTION" not in self.current_context:
                error_msg = f"YIELD statement outside of function at line {token.line}"
                logger.error(error_msg)
                raise SyntaxError(error_msg)
            return self.parse_control_statement()

        elif token.kind == "KEYWORD" and token.value == "PASS":
            return self.parse_control_statement()

        elif token.kind == "ID":
            # Look ahead for increment/decrement operators or function call
            next_token = self.tokens[self.pos + 1] if self.pos + 1 < len(self.tokens) else None
            if next_token:
                if next_token.kind in ["INCREMENT", "DECREMENT"]:
                    stmt = self.parse_increment_decrement()
                    if self.peek().kind == "TERMINATOR":
                        self.consume("TERMINATOR")
                    return stmt
                elif next_token.kind == "L_PAREN":
                    stmt = self.parse_function_call()
                    if self.peek().kind == "TERMINATOR":
                        self.consume("TERMINATOR")
                    return stmt

            error_msg = f"Unexpected ID token '{token.value}' without context at line {token.line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)

        elif token.kind in ["INCREMENT", "DECREMENT"]:
            return self.parse_increment_decrement()

        elif token.kind == "KEYWORD" and token.value == "SET":
            return self.parse_assignment_statement()

        elif token.kind == "KEYWORD" and token.value in ["PRINTLN", "PRINT"]:
            return self.parse_print_statement()

        elif token.kind == "KEYWORD" and token.value == "WAIT":
            self.consume("KEYWORD")
            return self.parse_wait_statement()

        elif token.kind == "KEYWORD" and token.value == "MOVE":
            self.consume("KEYWORD")
            if self.peek().kind == "TYPE_KEYWORD" and self.peek().value == "WINDOW":
                return self.parse_move_window()
            else:
                return self.parse_move_mouse()

        elif token.kind == "KEYWORD" and token.value == "FOCUS":
            return self.parse_focus_window()

        elif token.kind == "KEYWORD" and token.value in ["HOLD", "RELEASE", "PRESS"]:
            return self.parse_key_operation()

        elif (
            token.kind == "KEYWORD"
            and token.value == "PRESS"
            and self.pos + 1 < len(self.tokens)
            and self.tokens[self.pos + 1].value == "BUTTON"
        ):
            return self.parse_button_operation()

        elif token.kind == "KEYWORD" and token.value == "LAMBDA":
            lambda_expr = self.parse_lambda_function()
            if self.peek().kind == "TERMINATOR":
                self.consume("TERMINATOR")
            return lambda_expr

        elif token.kind == "KEYWORD" and token.value == "POINT":
            point_expr = self.parse_point()
            if self.peek().kind == "TERMINATOR":
                self.consume("TERMINATOR")
            return point_expr

        else:
            error_msg = f"Unexpected token: {token.kind} with value '{token.value}' at line {token.line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)

    def parse_function_definition(self) -> FunctionDefinition:
        """Parse a function definition statement."""
        logger.debug("Parsing function definition at position %d.", self.pos)
        self.consume("KEYWORD")  # Consume DEFUN
        function_name_token = self.consume("ID")
        function_name = function_name_token.value

        # Register the function name in defined_functions
        self.defined_functions.add(function_name)
        logger.debug("Registered function '%s' in defined_functions.", function_name)

        # Parse parameters
        if self.peek().kind != "L_PAREN":
            error_msg = f"Expected '(' after function name at line {self.peek().line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)
        
        self.consume("L_PAREN")
        parameters = []
        if self.peek().kind != "R_PAREN":
            parameters.append(self.consume("ID").value)
            while self.peek().kind == "COMMA":
                self.consume("COMMA")
                parameters.append(self.consume("ID").value)
        self.consume("R_PAREN")
        self.consume("L_BRACE")

        # Enter new scope for function body
        # Add parameters as variables in the local scope
        new_scope = {
            'variables': {param: {'type': None, 'is_parameter': True} for param in parameters},
            'functions': {}
        }
        self.scope_stack.append(new_scope)
        logger.debug("Entered new scope for function '%s' with parameters.", function_name)

        # Parse function body
        body = []
        while self.peek().kind != "R_BRACE" and self.peek().kind != "EOF":
            stmt = self.parse_statement()
            if stmt is not None:
                body.append(stmt)

        self.consume("R_BRACE")

        if self.peek().kind == "TERMINATOR":
            self.consume("TERMINATOR")

        # Exit function scope
        self.scope_stack.pop()
        logger.debug("Exited scope for function '%s'.", function_name)

        return FunctionDefinition(function_name, parameters, body)

    def parse_function_call(self) -> FunctionCall:
        """Parse a function call."""
        logger.debug("Parsing function call.")
        function_name_token = self.consume("ID")
        function_name = function_name_token.value
        self.consume("L_PAREN")
        arguments = self.parse_function_arguments()
        self.consume("R_PAREN")
        # self.consume("TERMINATOR")  # Assuming function calls end with a terminator

        # Verify function exists
        if not self.is_function(function_name):
            error_msg = f"Undefined function '{function_name}' at line {function_name_token.line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)

        logger.debug("Parsed FunctionCall: %s with arguments %s", function_name, arguments)
        return FunctionCall(function_name, arguments)

    def parse_function_parameters(self) -> List[str]:
        """Parse the parameters of a function."""
        logger.debug("Parsing function parameters.")
        parameters = []
        if self.peek().kind == "ID":
            parameters.append(self.consume("ID").value)
            while self.peek().value == ",":
                self.consume("COMMA")
                parameters.append(self.consume("ID").value)
        logger.debug("Parsed parameters: %s", parameters)
        return parameters

    def parse_function_arguments(self) -> List[ASTNode]:
        """Parse the arguments passed to a function call, including named arguments."""
        logger.debug("Parsing function call arguments.")
        arguments = []
        if self.peek().kind != "R_PAREN":
            # Check if it's a named argument
            if self.peek().kind == "ID" and self.pos + 1 < len(self.tokens) and self.tokens[self.pos + 1].value == "=":
                name = self.consume("ID").value
                self.consume("OP_ASSIGN")  # Consume '='
                value = self.parse_expression()
                arguments.append(NamedArgument(name, value))
            else:
                arguments.append(self.parse_expression())

            while self.peek().value == ",":
                self.consume("COMMA")
                # Check for named argument after comma
                if self.peek().kind == "ID" and self.pos + 1 < len(self.tokens) and self.tokens[self.pos + 1].value == "=":
                    name = self.consume("ID").value
                    self.consume("OP_ASSIGN")  # Consume '='
                    value = self.parse_expression()
                    arguments.append(NamedArgument(name, value))
                else:
                    arguments.append(self.parse_expression())

        logger.debug("Parsed arguments: %s", arguments)
        return arguments

    def parse_assignment_statement(self) -> Assignment:
        """Parse a variable assignment statement with optional type checking."""
        logger.debug("Parsing assignment statement at position %d.", self.pos)
        self.consume("KEYWORD")  # Consume SET
        
        # Get variable name
        var_token = self.consume("ID")
        var_name = var_token.value
        
        # Check if variable already exists and is typed
        current_scope = self.scope_stack[-1]
        existing_var = current_scope['variables'].get(var_name, {})
        existing_type = existing_var.get('type') if isinstance(existing_var, dict) else None
        
        # Look ahead for type hint
        var_type = None
        if self.peek().kind == "TYPE_HINT":
            self.consume("TYPE_HINT") # consume ':'
            type_token = self.consume("ID")
            if type_token.value.upper() not in self.data_types:
                error_msg = f"Invalid type '{type_token.value}' at line {type_token.line}"
                logger.error(error_msg)
                raise SyntaxError(error_msg)
            var_type = type_token.value.upper()

        elif self.peek().kind == "KEYWORD" and self.peek().value == "AS":
            self.consume("KEYWORD")  # Consume AS
            type_token = self.consume("ID")
            if type_token.value not in self.data_types:
                error_msg = f"Invalid type '{type_token.value}' at line {type_token.line}"
                logger.error(error_msg)
                raise SyntaxError(error_msg)
            var_type = type_token.value

        elif existing_type:  # If variable exists and was previously typed
            var_type = existing_type

        self.consume("OP_ASSIGN")
        expr = self.parse_expression()
        
        # Infer type for point
        if isinstance(expr, Point) and not var_type:
            var_type = "POINT"

        # Only consume terminator if not at the end of a block
        if self.peek().kind == "TERMINATOR":
            self.consume("TERMINATOR")
        
        # Register variable in current scope
        current_scope['variables'][var_name] = {
            'type': var_type,
            'initialized': True
        }
        
        logger.debug("Registered variable '%s' with type '%s' in current scope.", 
                    var_name, var_type if var_type else "dynamic")

        return Assignment(var_name, expr, var_type)

    def parse_print_statement(self) -> PrintStatement:
        """Parse a print or println statement with possible string concatenation."""
        print_token = self.consume("KEYWORD")  # Consume PRINT or PRINTLN
        print_type = print_token.value
        logger.debug("Parsing %s statement.", print_type)

        expr = self.parse_expression()  # Expression parsing now handles increment/decrement
        self.consume("TERMINATOR")

        logger.debug("Parsed print statement of type '%s' with expression '%s'.", print_type, expr)
        return PrintStatement(print_type, expr)

    def parse_wait_statement(self) -> WaitStatement:
        """Parse a wait statement with an expression (time value or variable), handling additions and other operations."""
        logger.debug("Parsing WAIT statement.")
        expr = self.parse_expression()

        # Handle additional time operations (e.g., WAIT 5s + 3s)
        while self.peek().kind == "OP" and self.peek().value in ['+', '-', '*', '/', '//', '%', '**']:
            op = self.consume("OP").value
            right_expr = self.parse_expression()
            expr = BinaryOperation(op, expr, right_expr)
            logger.debug("Parsed binary operation in WAIT statement: %s %s %s", expr.left, op, expr.right)

        logger.debug("Parsed WAIT statement with expression '%s'.", expr)
        return WaitStatement(expr)

    def parse_move_statement(self) -> MoveMouse:
        """Parse a mouse movement statement."""
        logger.debug("Parsing MOVE MOUSE statement.")
        # Expect MOUSE keyword
        if self.peek().value == "MOUSE":
            self.consume("KEYWORD")  # Consume MOUSE
        elif self.peek().value == "WINDOW":
            self.consume("KEYWORD")
        else:
            error_msg = f"Expected 'MOUSE' after MOVE, got {self.peek().value} at line {self.peek().line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)

        if self.peek().value == "TO":
            self.consume("KEYWORD_TARGET")  # Consume TO
        else:
            error_msg = f"Expected 'TO' after MOUSE, got {self.peek().value} at line {self.peek().line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)

        # Parse coordinates
        self.consume("L_PAREN")
        x = self.parse_expression()
        self.consume("COMMA")
        y = self.parse_expression()
        self.consume("R_PAREN")
        self.consume("TERMINATOR")

        logger.debug("Parsed MOVE MOUSE to coordinates (%s, %s).", x, y)
        return MoveMouse(x, y)

    def parse_key_operation(self) -> KeyOperation:
        """Parse key operations like HOLD, RELEASE, PRESS KEY"""
        operation = self.consume("KEYWORD").value
        logger.debug("Parsing key operation: %s", operation)
        if self.peek().value == "KEY":
            self.consume("TYPE_KEYWORD")  # Consume KEY
            key_token = self.consume("KEYBOARD_KEY")
            key = key_token.value
            self.consume("TERMINATOR")
            logger.debug("Parsed KeyOperation: %s %s", operation, key)
            return KeyOperation(operation, key)
        elif self.peek().value == "BUTTON":
            return self.parse_button_operation()
        else:
            error_msg = f"Expected 'KEY' or 'BUTTON' after {operation}, got {self.peek().value} at line {self.peek().line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)

    def parse_button_operation(self) -> ButtonOperation:
        """Parse button operations like PRESS BUTTON LEFT"""
        logger.debug("Parsing button operation.")
        self.consume("TYPE_KEYWORD")  # Consume BUTTON
        button = self.consume("MOUSE_KEY").value
        self.consume("TERMINATOR")

        logger.debug("Parsed ButtonOperation: PRESS BUTTON %s", button)
        return ButtonOperation(button)

    def parse_expression(self) -> ASTNode:
        """Parse an expression, handling operators based on precedence."""
        logger.debug("Parsing expression.")
        try:
            # Parse the initial expression
            left = self.parse_expression_precedence(0)
            
            # Handle top-level function composition
            while self.peek().kind == "COMP_OP" and self.peek().value == "|>":
                op = self.consume("COMP_OP").value
                right = self.parse_primary()  # Parse the function reference
                left = BinaryOperation(op, left, right)
                logger.debug("Parsed top-level function composition: %s |> %s", left, right)
            
            return left
        except SyntaxError as e:
            if "at line" in str(e):
                error_msg = f"{e}"
            else:
                error_msg = f"{e} at line {self.peek().line}"
                logger.error(error_msg)
            raise SyntaxError(error_msg) from e

    def parse_expression_precedence(self, min_precedence: int) -> ASTNode:
        """Parse expressions with operator precedence."""
        logger.debug("Parsing expression with minimum precedence %d.", min_precedence)
        left = self.parse_primary()

        while True:
            current = self.peek()
            # Handle binary operations
            if current.kind == "OP" and current.value in self.precedence:
                precedence = self.precedence[current.value]
                if precedence < min_precedence:
                    break
                op = self.consume("OP").value
                # Handle right-associative operators like '**'
                next_min_prec = precedence + 1 if op == '**' else precedence
                right = self.parse_expression_precedence(next_min_prec)
                left = BinaryOperation(op, left, right)
                logger.debug("Parsed binary operation: %s %s %s", left, op, right)
            # Handle comparison operations and function composition
            elif current.kind == "COMP_OP":
                op = self.consume("COMP_OP").value
                if op == "|>":
                    # Handle function composition
                    right = self.parse_primary()  # Parse the function reference
                    left = BinaryOperation(op, left, right)
                    logger.debug("Parsed function composition: %s |> %s", left, right)
                else:
                    right = self.parse_expression()
                    left = BinaryOperation(op, left, right)
                    logger.debug("Parsed comparison operation: %s %s %s", left, op, right)
            # Handle postfix increment/decrement
            elif current.kind in ["INCREMENT", "DECREMENT"] and isinstance(left, Identifier):
                op = self.consume(current.kind).value
                left = IncrementDecrement(left.name, op, is_prefix=False)
                logger.debug("Parsed postfix increment/decrement: %s%s", left.variable, op)
            else:
                break

        return left

    def parse_primary(self) -> ASTNode:
        """Parse primary expressions: literals, identifiers, or expressions in parentheses."""
        token = self.peek()
        logger.debug("Parsing primary expression with token: %s", token)

        if token.kind == "INT":
            self.consume("INT")
            logger.debug("Parsed Integer: %d", token.value)
            return Integer(token.value)
        elif token.kind == "FLOAT":
            self.consume("FLOAT")
            logger.debug("Parsed Float: %f", token.value)
            return Float(token.value)
        elif token.kind == "TIME":
            self.consume("TIME")
            time_value, unit = token.value
            logger.debug("Parsed Time: %f %s", time_value, unit)
            return Time(time_value, unit)
        elif token.kind == "STR":
            self.consume("STR")
            logger.debug("Parsed String: %s", token.value)
            return String(token.value)
        elif token.kind == "BOOL":
            self.consume("BOOL")
            logger.debug("Parsed Boolean: %s", token.value)
            return Boolean(token.value)
        elif token.kind == "KEYWORD" and token.value == "POINT":
            return self.parse_point()
        elif token.kind == "KEYWORD" and token.value == "LAMBDA":
            return self.parse_lambda_function()
        elif token.kind in ["INCREMENT", "DECREMENT"]:
            # Handle prefix increment/decrement
            op = self.consume(token.kind).value
            var_token = self.consume("ID")
            var_name = var_token.value

            # Check if variable exists in any scope
            if not self.is_variable(var_name):
                error_msg = f"Undefined variable '{var_name}' at line {token.line}"
                logger.error(error_msg)
                raise SyntaxError(error_msg)

            logger.debug("Parsed prefix increment/decrement: %s%s", op, var_name)
            return IncrementDecrement(var_name, op, is_prefix=True)
        elif token.kind == "ID":
            # Look ahead for the next token
            next_token = self.tokens[self.pos + 1] if self.pos + 1 < len(self.tokens) else None
            
            # Get the identifier
            identifier = self.consume("ID").value
            
            # Check if it's a function call
            if next_token and next_token.kind == "L_PAREN":
                self.consume("L_PAREN")
                arguments = self.parse_function_arguments()
                self.consume("R_PAREN")
                return FunctionCall(identifier, arguments)
            
            # Otherwise it's just an identifier (could be variable or function reference)
            return Identifier(identifier)
            
        elif token.kind == "L_PAREN":
            self.consume("L_PAREN")
            expr = self.parse_expression()
            self.consume("R_PAREN")
            logger.debug("Parsed parenthesized expression: %s", expr)
            return expr
        elif token.kind == "TYPE_KEYWORD" and token.value == "WINDOW":
            return self.parse_window_operation()
        else:
            error_msg = f"Unexpected token in expression: {token.kind} with value '{token.value}' at line {token.line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)

    def is_variable(self, name: str) -> bool:
        """Check if a variable is defined in any accessible scope."""
        # Check global scope
        if name in self.global_scope:
            logger.debug("Variable '%s' found in global scope.", name)
            return True
        
        # Check local scopes in the scope stack
        for scope in reversed(self.scope_stack):
            if name in scope['variables']:
                logger.debug("Variable '%s' found in local scope.", name)
                return True
        
        logger.debug("Variable '%s' not found in any scope.", name)
        return False

    def is_function(self, name: str) -> bool:
        """Check if a function name is valid in the current context."""
        # Check if it's a parameter in the current function scope
        if len(self.scope_stack) > 1:
            current_scope = self.scope_stack[-1]
            if name in current_scope['variables'] and current_scope['variables'][name].get('is_parameter'):
                logger.debug("Function '%s' found as parameter in current scope.", name)
                return True

        # Check if it's a function name that's been seen before in defined_functions or externally provided functions
        if name in self.defined_functions or name in self.functions:
            logger.debug("Function '%s' found in defined_functions or external functions.", name)
            return True

        logger.debug("Function '%s' not found in any scope.", name)
        return False

    def parse_identifier_usage(self) -> Identifier:
        """Parse an identifier usage and ensure it's defined."""
        token = self.consume("ID")
        var_name = token.value
        if not self.is_variable(var_name):
            error_msg = f"Undefined variable '{var_name}' at line {token.line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)
        logger.debug("Parsed Identifier: %s", var_name)
        return Identifier(var_name)

    def parse_while_loop(self) -> WhileLoop:
        """Parse a while loop statement."""
        self.consume("LOOP")  # Consume WHILE
        self.consume("L_PAREN")
        condition = self.parse_expression()
        self.consume("R_PAREN")
        self.consume("L_BRACE")
        body = []
        while self.peek().kind != "R_BRACE":
            stmt = self.parse_statement()
            if stmt:
                body.append(stmt)
        self.consume("R_BRACE")
        return WhileLoop(condition, body)

    def parse_repeat_loop(self) -> RepeatLoop:
        """Parse a repeat loop statement."""
        self.consume("LOOP")  # Consume REPEAT
        count = self.parse_expression()
        self.consume("KEYWORD")  # Consume TIMES
        self.consume("L_BRACE")
        body = []
        while self.peek().kind != "R_BRACE":
            stmt = self.parse_statement()
            if stmt:
                body.append(stmt)
        self.consume("R_BRACE")
        return RepeatLoop(count, body)

    def parse_control_statement(self) -> ControlStatement:
        """Parse control statements (BREAK, CONTINUE, RETURN, YIELD, PASS)."""
        token = self.peek()
        keyword = self.consume("KEYWORD").value
        value = None

        if keyword in ["RETURN", "YIELD"]:
            # Check if there's an expression after RETURN/YIELD
            if self.peek().kind != "TERMINATOR" and self.peek().kind != "R_BRACE":
                value = self.parse_expression()
                logger.debug("Parsed %s value: %s", keyword, value)

        # Only consume terminator if not at the end of a block
        if self.peek().kind == "TERMINATOR":
            self.consume("TERMINATOR")
        logger.debug("Parsed control statement: %s with value: %s", keyword, value)
        
        if keyword == "PASS":
            # For PASS, no action is needed
            return ControlStatement(keyword)
        
        return ControlStatement(keyword, value)

    def parse_increment_decrement(self) -> IncrementDecrement:
        """Parse increment/decrement operations in both prefix (++i) and postfix (i++) forms."""
        token = self.peek()
        is_prefix = False

        if token.kind in ["INCREMENT", "DECREMENT"]:
            # Prefix notation (++i or --i)
            is_prefix = True
            op = self.consume(token.kind).value
            var_token = self.consume("ID")
            var_name = var_token.value

            # Check if variable exists in any scope
            if not self.is_variable(var_name):
                error_msg = f"Undefined variable '{var_name}' at line {token.line}"
                logger.error(error_msg)
                raise SyntaxError(error_msg)

            logger.debug("Parsed prefix increment/decrement: %s%s", op, var_name)
            return IncrementDecrement(var_name, op, is_prefix)

        elif token.kind == "ID":
            # Postfix notation (i++ or i--)
            var_name = self.consume("ID").value

            # Check if variable exists in any scope
            if not self.is_variable(var_name):
                error_msg = f"Undefined variable '{var_name}' at line {token.line}"
                logger.error(error_msg)
                raise SyntaxError(error_msg)

            op_token = self.peek()
            if op_token.kind not in ["INCREMENT", "DECREMENT"]:
                error_msg = f"Expected increment or decrement operator, got {op_token.kind} at line {op_token.line}"
                logger.error(error_msg)
                raise SyntaxError(error_msg)

            op = self.consume(op_token.kind).value
            logger.debug("Parsed postfix increment/decrement: %s%s", var_name, op)
            return IncrementDecrement(var_name, op, is_prefix)

        else:
            error_msg = f"Expected ID or increment/decrement operator, got {token.kind} at line {token.line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)

    def parse_if_statement(self) -> IfStatement:
        """Parse an if statement with optional else-if and else clauses."""
        logger.debug("Parsing if statement")
        self.consume("KEYWORD")  # Consume IF
        self.consume("L_PAREN")
        condition = self.parse_expression()
        self.consume("R_PAREN")

        # Handle optional THEN keyword
        if self.peek().kind == "KEYWORD" and self.peek().value == "THEN":
            self.consume("KEYWORD")

        self.consume("L_BRACE")
        then_body = []
        while self.peek().kind != "R_BRACE":
            stmt = self.parse_statement()
            if stmt:
                then_body.append(stmt)
        self.consume("R_BRACE")

        else_if_conditions = []
        else_if_bodies = []
        else_body = []

        while self.peek().kind == "KEYWORD" and self.peek().value == "ELSEIF":
            self.consume("KEYWORD")  # Consume ELSEIF
            self.consume("L_PAREN")
            else_if_condition = self.parse_expression()
            self.consume("R_PAREN")
            self.consume("L_BRACE")
            
            else_if_body = []
            while self.peek().kind != "R_BRACE":
                stmt = self.parse_statement()
                if stmt:
                    else_if_body.append(stmt)
            self.consume("R_BRACE")
            
            else_if_conditions.append(else_if_condition)
            else_if_bodies.append(else_if_body)

        if self.peek().kind == "KEYWORD" and self.peek().value == "ELSE":
            self.consume("KEYWORD")  # Consume ELSE
            self.consume("L_BRACE")
            while self.peek().kind != "R_BRACE":
                stmt = self.parse_statement()
                if stmt:
                    else_body.append(stmt)
            self.consume("R_BRACE")

        return IfStatement(condition, then_body, else_if_conditions, else_if_bodies, else_body)

    def parse_move_window(self) -> MoveWindow:
        """Parse a window movement statement."""
        self.consume("TYPE_KEYWORD")  # Consume WINDOW
        window_name = self.parse_expression()
        self.consume("KEYWORD_TARGET")  # Consume TO
        self.consume("L_PAREN")
        x = self.parse_expression()
        self.consume("COMMA")
        y = self.parse_expression()
        self.consume("R_PAREN")
        self.consume("TERMINATOR")
        return MoveWindow(window_name, x, y)

    def parse_focus_window(self) -> FocusWindow:
        """Parse a window focus statement."""
        self.consume("KEYWORD")  # Consume FOCUS
        self.consume("TYPE_KEYWORD")  # Consume WINDOW
        window_name = self.parse_expression()
        self.consume("TERMINATOR")
        return FocusWindow(window_name)

    def parse_window_operation(self) -> ASTNode:
        """Parse window-related operations."""
        self.consume("TYPE_KEYWORD")  # Consume WINDOW
        
        # Parse the window name as a string or identifier
        if self.peek().kind == "STRING":
            window_name = String(self.consume("STRING").value)
        elif self.peek().kind == "ID":
            window_name = Identifier(self.consume("ID").value)
        else:
            error_msg = f"Expected string or identifier for window name, got {self.peek().kind} at line {self.peek().line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)

        # Check for EXISTS keyword
        if self.peek().kind == "KEYWORD_ASSERTION" and self.peek().value == "EXISTS":
            self.consume("KEYWORD_ASSERTION")
            return WindowExists(window_name)
        
        error_msg = f"Expected window operation (EXISTS), got {self.peek().kind} at line {self.peek().line}"
        logger.error(error_msg)
        raise SyntaxError(error_msg)

    def parse_lambda_function(self) -> LambdaFunction:
        """Parse a lambda function definition."""
        logger.debug("Parsing lambda function.")
        self.consume("KEYWORD")  # Consume LAMBDA
        self.consume("L_PAREN")
        parameters = self.parse_function_parameters()
        self.consume("R_PAREN")
        self.consume("L_BRACE")

        # Enter new scope for lambda body
        new_scope = {
            'variables': {param: {'type': None, 'is_parameter': True} for param in parameters},
            'functions': {}
        }
        self.scope_stack.append(new_scope)
        self.current_context.append("FUNCTION")  # Mark that we're in a function context

        body = []
        while self.peek().kind != "R_BRACE" and self.peek().kind != "EOF":
            stmt = self.parse_statement()
            if stmt is not None:
                body.append(stmt)

        self.consume("R_BRACE")

        # Exit lambda scope and function context
        self.scope_stack.pop()
        self.current_context.pop()

        return LambdaFunction(parameters, body)

    def parse_point(self) -> Point:
        """Parse a point constructor."""
        logger.debug("Parsing point constructor.")
        self.consume("KEYWORD")  # Consume POINT
        self.consume("L_PAREN")
        x = self.parse_expression()
        self.consume("COMMA")
        y = self.parse_expression()
        self.consume("R_PAREN")
        return Point(x, y)

    def parse_function_composition(self) -> FunctionComposition:
        """Parse function composition using the '|>' operator."""
        logger.debug("Parsing function composition.")
        functions = []
        functions.append(self.parse_primary())

        while self.peek().kind == "OP" and self.peek().value == "|>":
            self.consume("OP")  # Consume '|>'
            functions.append(self.parse_primary())

        return FunctionComposition(functions)

    def parse_move_mouse(self) -> MoveMouse:
        """Parse a mouse movement statement."""
        logger.debug("Parsing MOVE MOUSE statement.")
        # Expect MOUSE keyword
        if self.peek().kind == "KEYWORD" and self.peek().value == "MOUSE":
            self.consume("KEYWORD")  # Consume MOUSE
        else:
            error_msg = f"Expected 'MOUSE' after MOVE, got {self.peek().value} at line {self.peek().line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)

        if self.peek().kind == "KEYWORD_TARGET" and self.peek().value == "TO":
            self.consume("KEYWORD_TARGET")  # Consume TO
        else:
            error_msg = f"Expected 'TO' after MOUSE, got {self.peek().value} at line {self.peek().line}"
            logger.error(error_msg)
            raise SyntaxError(error_msg)

        # Parse coordinates or point
        if self.peek().kind == "ID":
            # Handle point variable
            point_var = self.parse_expression()  # This will return an Identifier node
            self.consume("TERMINATOR")
            return MoveMouse(variable = point_var)  # Pass the identifier directly

        if self.peek().kind == "KEYWORD" and self.peek().value == "POINT":
            # Handle direct point constructor
            point = self.parse_point()
            self.consume("TERMINATOR")
            return MoveMouse(variable = point)  # Pass the point directly
        else:
            # Parse regular coordinates
            self.consume("L_PAREN")
            x = self.parse_expression()
            self.consume("COMMA")
            y = self.parse_expression()
            self.consume("R_PAREN")
            self.consume("TERMINATOR")
            return MoveMouse(x=x, y=y)

