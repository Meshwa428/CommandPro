from ast_nodes import (
    Program, FunctionDefinition, FunctionCall, Assignment, PrintStatement,
    WaitStatement, MoveMouse, KeyOperation, ButtonOperation,
    BinaryOperation, Identifier, Integer, Time, String, Boolean, Float,
    ASTNode, EmptyStatement,
    WhileLoop, RepeatLoop, ControlStatement, IncrementDecrement, 
    IfStatement, MoveWindow, FocusWindow, WindowExists
)
import logging
from typing import Any, Dict, List 
from errors import TypeError, RuntimeError, ContinueException, ControlFlowException, ZeroDivisionError
from utils.window_manager import WindowManager
from utils.mouse_manager import MouseManager


# Configure logger for this module
logger = logging.getLogger(__name__)

class Executor:
    def __init__(self, verbose=False):
        self.global_scope = {}
        self.functions = {}
        self.call_stack = []
        self.verbose = verbose
        self.window_manager = WindowManager()
        self.mouse_manager = MouseManager()

    def log_execution(self, message: str):
        """Log execution message if verbose mode is enabled."""
        if self.verbose:
            print(message)
        logger.debug(message)

    def execute(self, ast: Program):
        logger.info("Starting execution of AST.")
        for stmt in ast.statements:
            if isinstance(stmt, FunctionDefinition):
                if stmt.name in self.functions:
                    logger.error("Function '%s' is already defined.", stmt.name)
                    raise Exception(f"Function '{stmt.name}' is already defined.")
                self.functions[stmt.name] = stmt
                logger.debug("Registered function '%s' in executor.", stmt.name)
            else:
                self.execute_statement(stmt, self.global_scope)
        logger.info("Execution completed successfully.")

    def execute_statement(self, stmt: ASTNode, scope: Dict[str, Any]):
        """Execute a single AST statement within the given scope."""
        method_name = f"execute_{stmt.__class__.__name__.lower()}"
        method = getattr(self, method_name, self.generic_execute)
        logger.debug("Executing statement: %s", stmt)
        method(stmt, scope)

    def generic_execute(self, stmt: ASTNode, scope: Dict[str, Any]):
        logger.error("No execute method defined for %s", type(stmt).__name__)
        raise NotImplementedError(f"No execute method defined for {type(stmt).__name__}")

    def execute_assignment(self, stmt: Assignment, scope: Dict[str, Any]):
        value = self.evaluate_expression(stmt.value, scope)
        
        # Get existing variable info if it exists
        existing_var = scope.get(stmt.variable_name)
        existing_type = None
        if isinstance(existing_var, dict) and 'type' in existing_var:
            existing_type = existing_var['type']
        
        # Type checking for typed variables
        if stmt.var_type or existing_type:
            target_type = stmt.var_type or existing_type
            try:
                if target_type == 'INTEGER':
                    if not isinstance(value, (int, float)):
                        value = int(value)
                    elif isinstance(value, float):
                        if not value.is_integer():
                            raise ValueError("Cannot convert float with decimal part to INTEGER")
                        value = int(value)
                elif target_type == 'FLOAT':
                    if not isinstance(value, (int, float)):
                        raise ValueError("Cannot convert value to FLOAT")
                    value = float(value)
                elif target_type == 'STRING':
                    value = str(value)
                elif target_type == 'BOOLEAN':
                    if isinstance(value, str):
                        if value.upper() in ('TRUE', 'FALSE'):
                            value = value.upper() == 'TRUE'
                        else:
                            raise ValueError(f"Cannot convert string '{value}' to BOOLEAN")
                    elif not isinstance(value, bool):
                        raise ValueError(f"Cannot convert {value} to BOOLEAN")
                elif target_type == 'TIME':
                    if not isinstance(value, tuple) or len(value) != 2:
                        raise ValueError(f"Invalid TIME value: {value}")
                    # Assuming TIME is stored as seconds
                    value = float(value[0])
            except (ValueError, TypeError) as e:
                logger.error("Type conversion error for variable '%s': %s", 
                            stmt.variable_name, str(e))
                raise TypeError(f"Cannot assign value of type {type(value).__name__} to variable of type {target_type}")
        
        # Store the value along with its type information
        if stmt.var_type:
            scope[stmt.variable_name] = {
                'value': value,
                'type': stmt.var_type
            }
        else:
            # For dynamically typed variables, store the value directly
            scope[stmt.variable_name] = value
        
        logger.debug("Assigned variable '%s' = %s (type: %s)", 
                    stmt.variable_name, value, 
                    stmt.var_type or type(value).__name__)

    def execute_printstatement(self, stmt: PrintStatement, scope: Dict[str, Any]):
        value = self.evaluate_expression(stmt.expression, scope)
        if stmt.print_type == "PRINTLN":
            print(value)
        else:
            print(value, end='')
        self.log_execution(f"Executed PrintStatement: {value}")

    def execute_waitstatement(self, stmt: WaitStatement, scope: Dict[str, Any]):
        value = self.evaluate_expression(stmt.expression, scope)
        # Implement wait logic here, e.g., time.sleep(value)
        self.log_execution(f"Executed WaitStatement: Waiting for {value} seconds.")

    def execute_movemouse(self, stmt: MoveMouse, scope: Dict[str, Any]):
        x = self.evaluate_expression(stmt.x, scope)
        y = self.evaluate_expression(stmt.y, scope)
        # Implement mouse movement logic here
        self.log_execution(f"Executed MoveMouse to ({x}, {y}).")

    def execute_keyoperation(self, stmt: KeyOperation, scope: Dict[str, Any]):
        key = stmt.key
        operation = stmt.operation
        # Implement key operation logic here
        self.log_execution(f"Executed KeyOperation: {operation} {key}.")

    def execute_buttonoperation(self, stmt: ButtonOperation, scope: Dict[str, Any]):
        button = stmt.button
        # Implement button operation logic here
        self.log_execution(f"Executed ButtonOperation: {button}.")

    def execute_functioncall(self, stmt: FunctionCall, scope: Dict[str, Any]):
        """Execute a function call with proper return value handling."""
        function = self.functions.get(stmt.function_name)
        if not function:
            logger.error("Undefined function '%s'.", stmt.function_name)
            raise RuntimeError(f"Undefined function '{stmt.function_name}'.")

        # Create new scope and set up parameters
        new_scope = {}
        for param, arg_expr in zip(function.parameters, stmt.arguments):
            arg_value = self.evaluate_expression(arg_expr, scope)
            new_scope[param] = arg_value

        # Push scope to call stack
        self.call_stack.append(new_scope)
        
        try:
            # Execute function body
            for func_stmt in function.body:
                try:
                    self.execute_statement(func_stmt, new_scope)
                except ControlFlowException as cf:
                    if cf.statement_type == "RETURN":
                        return cf.value
                    elif cf.statement_type == "YIELD":
                        # For yield statements, we'll create a generator function
                        def generator():
                            yield cf.value
                            # Continue execution after yield
                            for remaining_stmt in function.body[function.body.index(func_stmt) + 1:]:
                                try:
                                    self.execute_statement(remaining_stmt, new_scope)
                                except ControlFlowException as inner_cf:
                                    if inner_cf.statement_type == "YIELD":
                                        yield inner_cf.value
                                    elif inner_cf.statement_type == "RETURN":
                                        return inner_cf.value
                                    else:
                                        raise
                        return generator()
                    else:
                        raise  # Re-raise other control statements
            return None  # If no return/yield statement was encountered
        finally:
            # Always pop the scope after execution
            self.call_stack.pop()

    def evaluate_expression(self, expr: ASTNode, scope: Dict[str, Any]) -> Any:
        """Evaluate an expression node and return its value."""
        method_name = f"evaluate_{expr.__class__.__name__.lower()}"
        method = getattr(self, method_name, self.generic_evaluate)
        logger.debug("Evaluating expression: %s", expr)
        return method(expr, scope)

    def generic_evaluate(self, expr: ASTNode, scope: Dict[str, Any]) -> Any:
        logger.error("No evaluate method defined for %s", type(expr).__name__)
        raise NotImplementedError(f"No evaluate method defined for {type(expr).__name__}")

    def evaluate_functioncall(self, expr: FunctionCall, scope: Dict[str, Any]) -> Any:
        """Evaluate a function call expression by executing it.
        
        Args:
            expr: The FunctionCall AST node to evaluate
            scope: The current variable scope
            
        Returns:
            The return value from executing the function
            
        Raises:
            RuntimeError: If the function is undefined
        """
        try:
            return self.execute_functioncall(expr, scope)
        except RuntimeError as e:
            logger.error("Error evaluating function call: %s", e)
            raise

    def evaluate_binaryoperation(self, expr: BinaryOperation, scope: Dict[str, Any]) -> Any:
        left = self.evaluate_expression(expr.left, scope)
        right = self.evaluate_expression(expr.right, scope)
        operator = expr.operator

        logger.debug("Evaluating BinaryOperation: %s %s %s", left, operator, right)

        try:
            if operator == '+':
                return left + right
            elif operator == '-':
                return left - right
            elif operator == '*':
                return left * right
            elif operator == '/':
                if right == 0:
                    raise ZeroDivisionError("Division by zero.")
                return left / right
            elif operator == '//':
                if (isinstance(left, float) or isinstance(right, float)) and right == 0:
                    raise ZeroDivisionError("Float floor division by zero.")
                elif right == 0:
                    raise ZeroDivisionError("Division by zero.")
                return left // right
            elif operator == '**':
                return left ** right
            elif operator == '==':
                return left == right
            elif operator == '!=':
                return left != right
            elif operator == '>':
                return left > right
            elif operator == '<':
                return left < right
            elif operator == '>=':
                return left >= right
            elif operator == '<=':
                return left <= right
            elif operator == '&&':
                return left and right
            elif operator == '||':
                return left or right
            else:
                logger.error("Unsupported operator '%s'.", operator)
                raise TypeError(f"Unsupported operator '{operator}'.")
        except TypeError as e:
            logger.error("Type error in binary operation: %s", e)
            raise TypeError(f"Type error in binary operation: {e}")

    def evaluate_float(self, expr: Float, scope: Dict[str, Any]) -> float:
        """Return the float value of the expression."""
        logger.debug("Evaluating Float: %s", expr.value)
        return expr.value

    def evaluate_identifier(self, expr: Identifier, scope: Dict[str, Any]) -> Any:
        """Modified to handle both typed and untyped variables"""
        value = scope.get(expr.name)
        
        # Check call stack if not found in current scope
        if value is None and self.call_stack:
            for s in reversed(self.call_stack):
                if expr.name in s:
                    value = s[expr.name]
                    break
        
        if value is None:
            logger.error("Undefined variable '%s'.", expr.name)
            raise Exception(f"Undefined variable '{expr.name}'.")
        
        # If the value is stored with type information, return the actual value
        if isinstance(value, dict) and 'value' in value:
            return value['value']
        
        return value

    def evaluate_integer(self, expr: Integer, scope: Dict[str, Any]) -> int:
        return expr.value

    def evaluate_time(self, expr: Time, scope: Dict[str, Any]) -> float:
        # Convert all time to seconds for consistency
        unit = expr.unit
        value = expr.value
        if unit == 'ms':
            return value / 1000
        elif unit == 's':
            return value
        elif unit == 'm':
            return value * 60
        elif unit == 'h':
            return value * 3600
        else:
            logger.error("Unsupported time unit '%s'.", unit)
            raise Exception(f"Unsupported time unit '{unit}'.")
    
    def evaluate_string(self, expr: String, scope: Dict[str, Any]) -> str:
        return expr.value.strip('"')  # Remove surrounding quotes

    def evaluate_boolean(self, expr: Boolean, scope: Dict[str, Any]) -> bool:
        return expr.value

    def evaluate_windowexists(self, expr: WindowExists, scope: Dict[str, Any]) -> bool:
        window_name = self.evaluate_expression(expr.window_name, scope)
        return self.window_manager.exists(window_name)


    def execute_whileloop(self, stmt: WhileLoop, scope: Dict[str, Any]):
        """Execute a while loop with proper control flow."""
        while self.evaluate_expression(stmt.condition, scope):
            try:
                for body_stmt in stmt.body:
                    try:
                        self.execute_statement(body_stmt, scope)
                    except ControlFlowException as cf:
                        if cf.statement_type == "BREAK":
                            return None
                        elif cf.statement_type == "CONTINUE":
                            break  # Break inner loop to continue outer loop
                        elif cf.statement_type == "RETURN":
                            return cf.value  # Propagate return value
                        else:
                            raise
            except ContinueException:
                continue  # Continue the while loop

    def execute_repeatloop(self, stmt: RepeatLoop, scope: Dict[str, Any]):
        """Execute a repeat loop with proper control flow."""
        count = int(self.evaluate_expression(stmt.count, scope))
        
        for _ in range(count):
            try:
                for body_stmt in stmt.body:
                    try:
                        self.execute_statement(body_stmt, scope)
                    except ControlFlowException as cf:
                        if cf.statement_type == "BREAK":
                            return None
                        elif cf.statement_type == "CONTINUE":
                            break  # Break inner loop to continue outer loop
                        elif cf.statement_type == "RETURN":
                            return cf.value  # Propagate return value
                        else:
                            raise
            except ContinueException:
                continue  # Continue the repeat loop

    def execute_controlstatement(self, stmt: ControlStatement, scope: Dict[str, Any]):
        """Execute a control statement."""
        if stmt.statement_type == "PASS":
            self.log_execution("Executed PASS statement (no operation).")
            return  # No operation for PASS
        value = None
        if stmt.value:
            value = self.evaluate_expression(stmt.value, scope)
        raise ControlFlowException(stmt.statement_type, value)

    def execute_incrementdecrement(self, stmt: IncrementDecrement, scope: Dict[str, Any]):
        """Execute an increment/decrement operation."""
        try:
            current_value = self.evaluate_expression(Identifier(stmt.variable), scope)
            if not isinstance(current_value, (int, float)):
                raise TypeError(f"Cannot {stmt.operation} non-numeric value")
            
            new_value = current_value + (1 if stmt.operation == "++" else -1)
            
            # Update the value in the appropriate scope
            if stmt.variable in scope:
                scope[stmt.variable] = new_value
            elif self.call_stack:
                # Search call stack for the variable
                for s in reversed(self.call_stack):
                    if stmt.variable in s:
                        s[stmt.variable] = new_value
                        break
            
            logger.debug("Executed %s%s: %d -> %d", 
                        stmt.variable, stmt.operation, current_value, new_value)
            return new_value if stmt.is_prefix else current_value
        except Exception as e:
            logger.error("Error in increment/decrement operation: %s", str(e))
            raise

    def execute_ifstatement(self, stmt: IfStatement, scope: Dict[str, Any]):
        """Execute an if statement with optional else-if and else clauses."""
        logger.debug("Executing if statement")
        
        # Evaluate main condition
        if self.evaluate_expression(stmt.condition, scope):
            for body_stmt in stmt.then_body:
                self.execute_statement(body_stmt, scope)
            return
        
        # Check else-if conditions
        for condition, body in zip(stmt.else_if_conditions, stmt.else_if_bodies):
            if self.evaluate_expression(condition, scope):
                for body_stmt in body:
                    self.execute_statement(body_stmt, scope)
                return
        
        # Execute else body if no conditions were true
        if stmt.else_body:
            for body_stmt in stmt.else_body:
                self.execute_statement(body_stmt, scope)

    def execute_movemouse(self, stmt: MoveMouse, scope: Dict[str, Any]):
        x = self.evaluate_expression(stmt.x, scope)
        y = self.evaluate_expression(stmt.y, scope)
        self.mouse_manager.move(x, y)
        self.log_execution(f"Executed MoveMouse to ({x}, {y}).")

    def execute_movewindow(self, stmt: MoveWindow, scope: Dict[str, Any]):
        window_name = self.evaluate_expression(stmt.window_name, scope)
        x = self.evaluate_expression(stmt.x, scope)
        y = self.evaluate_expression(stmt.y, scope)
        if self.window_manager.move(window_name, x, y):
            self.log_execution(f"Moved window '{window_name}' to ({x}, {y}).")
        else:
            raise RuntimeError(f"Window '{window_name}' does not exist.")

    def execute_focuswindow(self, stmt: FocusWindow, scope: Dict[str, Any]):
        window_name = self.evaluate_expression(stmt.window_name, scope)
        if self.window_manager.focus(window_name):
            self.log_execution(f"Focused window '{window_name}'.")
        else:
            raise RuntimeError(f"Window '{window_name}' does not exist.")

