from .ast_nodes import (
    ASTNode, Program,
    FunctionDefinition, FunctionCall, Assignment, PrintStatement,
    WaitStatement, MoveMouse, KeyOperation, ButtonOperation,
    BinaryOperation, Identifier, Integer, Time, String, Boolean, Float,
    WhileLoop, RepeatLoop, ControlStatement, IncrementDecrement, 
    IfStatement, MoveWindow, FocusWindow, WindowExists,
    LambdaFunction, FunctionComposition, Point, NamedArgument
)
import logging
from typing import Any, Dict, List 
from .errors import TypeError, RuntimeError, ContinueException, ControlFlowException, ZeroDivisionError
from .utils import WindowManager
from .utils import MouseManager


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
        """Execute the entire AST program.
        
        Args:
            ast (Program): The AST program to execute
        """
        logger.info("Starting execution of AST.")
        
        for stmt in ast.statements:
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
                if target_type == 'INT':
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
                elif target_type == 'STR':
                    value = str(value)
                elif target_type == 'BOOL':
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
                elif target_type == 'POINT':
                    if not isinstance(value, Point):
                        raise ValueError(f"Cannot convert {value} to POINT")
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
        """Execute a mouse movement statement."""
        if stmt.variable:
            # If we have a point variable or direct point
            point = self.evaluate_expression(stmt.variable, scope)
            if isinstance(point, Point):
                # Get the actual numeric values from the point
                x = self.evaluate_expression(point.x, scope)
                y = self.evaluate_expression(point.y, scope)
            else:
                raise TypeError(f"Expected Point type, got {type(point)}")
        else:
            # Otherwise evaluate both x and y coordinates
            x = self.evaluate_expression(stmt.x, scope)
            y = self.evaluate_expression(stmt.y, scope)

        # Convert to integers if needed
        x = int(x) if isinstance(x, float) else x
        y = int(y) if isinstance(y, float) else y

        self.mouse_manager.move(x, y)
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
        # First check if it's a variable in the current scope (could be a function parameter)
        if stmt.function_name in scope and callable(scope[stmt.function_name]):
            # If it's a callable in the scope, use it directly
            function = scope[stmt.function_name]
            args = [self.evaluate_expression(arg, scope) for arg in stmt.arguments]
            return function(*args)
        
        # Then check if it's a variable holding a function (e.g., lambda)
        function_var = scope.get(stmt.function_name)
        if isinstance(function_var, dict) and 'lambda' in function_var:
            function = function_var['lambda']
        else:
            # Then check if it's a registered function
            function = self.functions.get(stmt.function_name)
            if not function:
                # Check call stack for function parameters
                for call_scope in reversed(self.call_stack):
                    if stmt.function_name in call_scope and callable(call_scope[stmt.function_name]):
                        args = [self.evaluate_expression(arg, scope) for arg in stmt.arguments]
                        return call_scope[stmt.function_name](*args)
                
                logger.error("Undefined function '%s'.", stmt.function_name)
                raise RuntimeError(f"Undefined function '{stmt.function_name}'.")

        # Create new scope and set up parameters
        new_scope = {}
        
        # Handle named arguments and regular arguments
        named_args = {}
        positional_args = []
        
        for arg in stmt.arguments:
            if isinstance(arg, NamedArgument):
                named_args[arg.name] = self.evaluate_expression(arg.value, scope)
            else:
                # If the argument is an identifier, it might be a function reference
                if isinstance(arg, Identifier):
                    # Check if it's a function in the functions dictionary
                    if arg.name in self.functions:
                        func_def = self.functions[arg.name]
                        def callable_wrapper(*args):
                            call_expr = FunctionCall(func_def.name, [Integer(arg) if isinstance(arg, (int, float)) else arg for arg in args])
                            return self.execute_functioncall(call_expr, scope)
                        positional_args.append(callable_wrapper)
                    else:
                        # Otherwise evaluate it normally
                        positional_args.append(self.evaluate_expression(arg, scope))
                else:
                    positional_args.append(self.evaluate_expression(arg, scope))

        # Map arguments to parameters
        for i, param in enumerate(function.parameters):
            if param in named_args:
                new_scope[param] = named_args[param]
            elif i < len(positional_args):
                new_scope[param] = positional_args[i]
            else:
                logger.error("Missing argument for parameter '%s'.", param)
                raise RuntimeError(f"Missing argument for parameter '{param}'.")

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

    def execute_lambdafunction(self, stmt: LambdaFunction, scope: Dict[str, Any]):
        """Execute a lambda function definition."""
        # Store lambda function in current scope
        lambda_func = {
            'parameters': stmt.parameters,
            'body': stmt.body,
            'closure': dict(scope)  # Capture current scope for closure
        }
        # Mark it as a lambda
        # scope[stmt.name] = {
        #     'lambda': lambda_func
        # }
        # logger.debug("Registered lambda function '%s' in scope.", stmt.name)
        return lambda_func

    def execute_functioncomposition(self, stmt: FunctionComposition, scope: Dict[str, Any]):
        """Execute function composition."""
        def composed_function(*args):
            result = args[0] if args else None
            for func in stmt.functions[1:]:
                result = self.evaluate_expression(func, scope)(result)
            return result
        return composed_function

    def execute_point(self, stmt: Point, scope: Dict[str, Any]):
        """Execute a point constructor."""
        return self.evaluate_point(stmt, scope)

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
        """Evaluate a function call expression."""
        return self.execute_functioncall(expr, scope)

    def evaluate_binaryoperation(self, expr: BinaryOperation, scope: Dict[str, Any]) -> Any:
        left = self.evaluate_expression(expr.left, scope)
        right = self.evaluate_expression(expr.right, scope)
        operator = expr.operator

        logger.debug("Evaluating BinaryOperation: %s %s %s", left, operator, right)

        try:
            if operator == '+':
                # Handle string concatenation with non-string types
                if isinstance(left, str) or isinstance(right, str):
                    return str(left) + str(right)
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
            elif operator == '|>':
                # Handle function composition
                if not callable(right):
                    raise TypeError(f"Right operand of |> must be a function, got {type(right)}")
                return right(left)
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
        """Evaluate an identifier which could be a variable or function reference."""
        # First check if it's a variable in the current scope or call stack
        value = scope.get(expr.name)
        
        # Check call stack if not found in current scope
        if value is None and self.call_stack:
            for s in reversed(self.call_stack):
                if expr.name in s:
                    value = s[expr.name]
                    break
        
        # If found as a variable, return its value
        if value is not None:
            # If the value is stored with type information, return the actual value
            if isinstance(value, dict) and 'value' in value:
                return value['value']
            return value
        
        # If not found as a variable, check if it's a function reference
        if expr.name in self.functions:
            # Return a callable that will execute the function when called
            func_def = self.functions[expr.name]
            def callable_function(*args):
                call_expr = FunctionCall(expr.name, [Integer(arg) if isinstance(arg, (int, float)) else arg for arg in args])
                return self.execute_functioncall(call_expr, scope)
            
            # Add a custom __repr__ to the callable_function
            callable_function.__repr__ = lambda : f"<function {func_def.name}({', '.join(func_def.parameters)})>"
            return callable_function
        
        # If not found anywhere, raise an error
        logger.error("Undefined identifier '%s'.", expr.name)
        raise RuntimeError(f"Undefined identifier '{expr.name}'.")

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
        """Execute an increment/decrement operation.
        
        For prefix operations (++i or --i), increment/decrement first and return new value
        For postfix operations (i++ or i--), return original value and then increment/decrement
        """
        try:
            # Get the current value
            current_value = self.evaluate_expression(Identifier(stmt.variable), scope)
            if not isinstance(current_value, (int, float)):
                raise TypeError(f"Cannot {stmt.operation} non-numeric value")
            
            # Calculate the new value
            new_value = current_value + (1 if stmt.operation == "++" else -1)
            
            # Find the appropriate scope to update
            target_scope = None
            if stmt.variable in scope:
                target_scope = scope
            elif self.call_stack:
                # Search call stack for the variable
                for s in reversed(self.call_stack):
                    if stmt.variable in s:
                        target_scope = s
                        break
            
            if target_scope is None:
                raise RuntimeError(f"Variable '{stmt.variable}' not found in any scope")
            
            # Update the value in the appropriate scope
            target_scope[stmt.variable] = new_value
            
            logger.debug("Executed %s%s: %d -> %d", 
                        stmt.variable, stmt.operation, current_value, new_value)
            
            # For prefix operations (++i), return the new value
            # For postfix operations (i++), return the original value
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
        """Execute a mouse movement statement."""
        if stmt.variable:
            # If we have a point variable or direct point
            point = self.evaluate_expression(stmt.variable, scope)
            if isinstance(point, Point):
                # Get the actual numeric values from the point
                x = self.evaluate_expression(point.x, scope)
                y = self.evaluate_expression(point.y, scope)
            else:
                raise TypeError(f"Expected Point type, got {type(point)}")
        else:
            # Otherwise evaluate both x and y coordinates
            x = self.evaluate_expression(stmt.x, scope)
            y = self.evaluate_expression(stmt.y, scope)

        # Convert to integers if needed
        x = int(x) if isinstance(x, float) else x
        y = int(y) if isinstance(y, float) else y

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

    def evaluate_incrementdecrement(self, expr: IncrementDecrement, scope: Dict[str, Any]) -> Any:
        """Evaluate an increment/decrement operation in an expression context.
        
        For prefix operations (++i), increment/decrement first and return new value
        For postfix operations (i++), return original value and then increment/decrement
        """
        try:
            # Get the current value
            current_value = self.evaluate_expression(Identifier(expr.variable), scope)
            if not isinstance(current_value, (int, float)):
                raise TypeError(f"Cannot {expr.operation} non-numeric value")
            
            # Calculate the new value
            new_value = current_value + (1 if expr.operation == "++" else -1)
            
            # Find the appropriate scope to update
            target_scope = None
            if expr.variable in scope:
                target_scope = scope
            elif self.call_stack:
                # Search call stack for the variable
                for s in reversed(self.call_stack):
                    if expr.variable in s:
                        target_scope = s
                        break
            
            if target_scope is None:
                raise RuntimeError(f"Variable '{expr.variable}' not found in any scope")
            
            # Update the value in the appropriate scope
            target_scope[expr.variable] = new_value
            
            logger.debug("Evaluated %s%s: %d -> %d", 
                        expr.variable, expr.operation, current_value, new_value)
            
            # For prefix operations (++i), return the new value
            # For postfix operations (i++), return the original value
            return new_value if expr.is_prefix else current_value
            
        except Exception as e:
            logger.error("Error in increment/decrement operation: %s", str(e))
            raise

    def evaluate_point(self, expr: Point, scope: Dict[str, Any]) -> Point:
        """Evaluate a Point expression."""
        logger.debug("Evaluating Point with x=%s and y=%s", expr.x, expr.y)
        x = self.evaluate_expression(expr.x, scope)
        y = self.evaluate_expression(expr.y, scope)
        # Return a new Point with the evaluated values
        return Point(Integer(x), Integer(y))

    def evaluate_lambdafunction(self, expr: LambdaFunction, scope: Dict[str, Any]) -> Any:
        """Evaluate a lambda function expression."""
        # Lambda functions are evaluated when they are called, not when they are defined
        # Here we return a callable that will execute the lambda function when called
        
        lambda_func = {
            'parameters': expr.parameters,
            'body': expr.body,
            'closure': dict(scope)  # Capture current scope for closure
        }
        
        def callable_lambda(*args):
            # Create a new scope for the lambda function
            lambda_scope = dict(lambda_func['closure'])  # Copy the closure
            
            # Map arguments to parameters
            for i, param in enumerate(lambda_func['parameters']):
                if i < len(args):
                    lambda_scope[param] = args[i]
                else:
                    raise RuntimeError(f"Missing argument for parameter '{param}'.")
            
            # Execute the lambda function body
            result = None
            for stmt in lambda_func['body']:
                try:
                    result = self.execute_statement(stmt, lambda_scope)
                except ControlFlowException as cf:
                    if cf.statement_type == "RETURN":
                        return cf.value
                    else:
                        raise
            return result

        callable_lambda.__repr__ = lambda : f"<lambda ({', '.join(expr.parameters)})>"
        
        return callable_lambda






    def execute_functiondefinition(self, stmt: FunctionDefinition, scope: Dict[str, Any]):
        """Execute a function definition by registering it in the executor.
        
        Args:
            stmt (FunctionDefinition): The function definition AST node
            scope (Dict[str, Any]): The current scope
        """
        logger.debug("Registering function '%s' with parameters %s", 
                    stmt.name, stmt.parameters)
        
        # Check if function already exists
        if stmt.name in self.functions:
            logger.error("Function '%s' is already defined.", stmt.name)
            raise RuntimeError(f"Function '{stmt.name}' is already defined.")
        
        # Register the function in the executor's function dictionary
        self.functions[stmt.name] = stmt
        
        logger.debug("Successfully registered function '%s'", stmt.name)

    def evaluate_functiondefinition(self, expr: FunctionDefinition, scope: Dict[str, Any]) -> Any:
        """Evaluate a function definition expression.
        
        When a function definition is evaluated as an expression (e.g., in an assignment),
        it returns a callable that wraps the function.
        
        Args:
            expr (FunctionDefinition): The function definition AST node
            scope (Dict[str, Any]): The current scope
        
        Returns:
            callable: A callable wrapper for the function
        """
        logger.debug("Evaluating function definition '%s' as expression", expr.name)
        
        # Register the function if not already registered
        if expr.name not in self.functions:
            self.execute_functiondefinition(expr, scope)

        return self.functions[expr.name]