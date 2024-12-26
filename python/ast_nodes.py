import json
from typing import List, Optional

class Token:
    def __init__(self, kind, value, line, previous_token=None, next_token=None):
        self.kind = kind
        self.value = value
        self.line = line  # New attribute to store the line number
        self.previous_token = previous_token
        self.next_token = next_token

    def __repr__(self):
        escaped_value = self.value.encode('unicode_escape').decode()
        return f"Token(kind='{self.kind}', value='{escaped_value}', line={self.line})"

class ASTNode:
    def to_dict(self):
        raise NotImplementedError("to_dict method not implemented.")

class Program(ASTNode):
    def __init__(self, statements: List[ASTNode]):
        self.statements = statements

    def __repr__(self):
        return f"Program(statements={self.statements})"

    def to_dict(self):
        return {
            "type": "Program",
            "statements": [stmt.to_dict() for stmt in self.statements if stmt is not None]
        }

class FunctionDefinition(ASTNode):
    def __init__(self, name: str, parameters: List[str], body: List[ASTNode]):
        self.name = name
        self.parameters = parameters
        self.body = body

    def __repr__(self):
        return f"FunctionDefinition(name='{self.name}', parameters={self.parameters}, body={self.body})"

    def to_dict(self):
        return {
            "type": "FunctionDefinition",
            "name": self.name,
            "parameters": self.parameters,
            "body": [stmt.to_dict() for stmt in self.body if stmt is not None]
        }

class Assignment(ASTNode):
    def __init__(self, variable_name: str, value: ASTNode, var_type: str = None):
        self.variable_name = variable_name
        self.value = value
        self.var_type = var_type

    def __repr__(self):
        return f"Assignment(variable_name='{self.variable_name}', value={self.value}, var_type='{self.var_type}')"

    def to_dict(self):
        return {
            "type": "Assignment",
            "variable": self.variable_name,
            "expression": self.value.to_dict(),
            "var_type": self.var_type
        }

class PrintStatement(ASTNode):
    def __init__(self, print_type: str, expression: ASTNode):
        self.print_type = print_type
        self.expression = expression

    def __repr__(self):
        return f"PrintStatement(print_type='{self.print_type}', expression={self.expression})"

    def to_dict(self):
        return {
            "type": "PrintStatement",
            "print_type": self.print_type,
            "expression": self.expression.to_dict()
        }

class WaitStatement(ASTNode):
    def __init__(self, expression: ASTNode):
        self.expression = expression

    def __repr__(self):
        return f"WaitStatement(expression={self.expression})"

    def to_dict(self):
        return {
            "type": "WaitStatement",
            "expression": self.expression.to_dict()
        }

class MoveMouse(ASTNode):
    def __init__(self, x: ASTNode, y: ASTNode):
        self.x = x
        self.y = y

    def __repr__(self):
        return f"MoveMouse(x={self.x}, y={self.y})"

    def to_dict(self):
        return {
            "type": "MoveMouse",
            "x": self.x.to_dict(),
            "y": self.y.to_dict()
        }

class KeyOperation(ASTNode):
    def __init__(self, operation: str, key: str):
        self.operation = operation
        self.key = key

    def __repr__(self):
        return f"KeyOperation(operation='{self.operation}', key='{self.key}')"

    def to_dict(self):
        return {
            "type": "KeyOperation",
            "operation": self.operation,
            "key": self.key
        }

class ButtonOperation(ASTNode):
    def __init__(self, button: str):
        self.button = button

    def __repr__(self):
        return f"ButtonOperation(button='{self.button}')"

    def to_dict(self):
        return {
            "type": "ButtonOperation",
            "button": self.button
        }

class BinaryOperation(ASTNode):
    def __init__(self, operator: str, left: ASTNode, right: ASTNode):
        self.operator = operator
        self.left = left
        self.right = right
    
    def __repr__(self):
        return f"BinaryOperation(operator='{self.operator}', left={self.left}, right={self.right})"

    def to_dict(self):
        return {
            "type": "BinaryOperation",
            "operator": self.operator,
            "left": self.left.to_dict(),
            "right": self.right.to_dict()
        }

class Identifier(ASTNode):
    def __init__(self, name: str):
        self.name = name
    
    def __repr__(self):
        return f"Identifier({self.name})"

    def to_dict(self):
        return {
            "type": "Identifier",
            "name": self.name
        }

class Integer(ASTNode):
    def __init__(self, value: int):
        self.value = value
    
    def __repr__(self):
        return f"Integer({self.value})"

    def to_dict(self):
        return {
            "type": "Integer",
            "value": self.value
        }

class Float(ASTNode):
    def __init__(self, value: float):
        self.value = value
    
    def __repr__(self):
        return f"Float({self.value})"
    
    def to_dict(self):
        return {
            "type": "Float",
            "value": self.value
        }

class Time(ASTNode):
    def __init__(self, value: float, unit: str):
        self.value = value
        self.unit = unit
    
    def __repr__(self):
        return f"Time({self.value}, {self.unit})"

    def to_dict(self):
        return {
            "type": "Time",
            "value": self.value,
            "unit": self.unit
        }

class String(ASTNode):
    def __init__(self, value: str):
        self.value = value
    
    def __repr__(self):
        # Use repr to handle escaping and truncate if necessary
        escaped_value = repr(self.value)[1:-1]  # Remove surrounding quotes
        if len(escaped_value) > 50:
            escaped_value = f"{escaped_value[:47]}..."
        return f"String({escaped_value})"

    def to_dict(self):
        return {
            "type": "String",
            "value": self.value
        }

class EmptyStatement(ASTNode):
    def to_dict(self):
        return {
            "type": "EmptyStatement"
        }

class FunctionCall(ASTNode):
    def __init__(self, function_name: str, arguments: List[ASTNode]):
        self.function_name = function_name
        self.arguments = arguments

    def accept(self, visitor):
        return visitor.visit_function_call(self)

    def __repr__(self):
        return f"FunctionCall(function_name='{self.function_name}', arguments={self.arguments})"

    def to_dict(self):
        return {
            "type": "FunctionCall",
            "name": self.function_name,
            "arguments": [arg.to_dict() for arg in self.arguments]
        }

class Boolean(ASTNode):
    def __init__(self, value: bool):
        self.value = value

    def __repr__(self):
        return f"Boolean({self.value})"
    
    def to_dict(self):
        return {
            "type": "Boolean",
            "value": self.value
        }

class WhileLoop(ASTNode):
    def __init__(self, condition: ASTNode, body: List[ASTNode]):
        self.condition = condition
        self.body = body
    
    def __repr__(self):
        return f"WhileLoop(condition={self.condition}, body={self.body})"

    def to_dict(self):
        return {
            "type": "WhileLoop",
            "condition": self.condition.to_dict(),
            "body": [stmt.to_dict() for stmt in self.body]
        }

class RepeatLoop(ASTNode):
    def __init__(self, count: ASTNode, body: List[ASTNode]):
        self.count = count
        self.body = body
    
    def __repr__(self):
        return f"RepeatLoop(count={self.count}, body={self.body})"

    def to_dict(self):
        return {
            "type": "RepeatLoop",
            "count": self.count.to_dict(),
            "body": [stmt.to_dict() for stmt in self.body]
        }

class ControlStatement(ASTNode):
    def __init__(self, statement_type: str, value: Optional[ASTNode] = None):
        self.statement_type = statement_type  # BREAK, CONTINUE, RETURN, YIELD
        self.value = value  # For RETURN and YIELD
    
    def __repr__(self):
        return f"ControlStatement(statement_type='{self.statement_type}', value={self.value})"

    def to_dict(self):
        return {
            "type": "ControlStatement",
            "statement_type": self.statement_type,
            "value": self.value.to_dict() if self.value else None
        }

class IncrementDecrement(ASTNode):
    def __init__(self, variable: str, operation: str, is_prefix: bool = False):
        self.variable = variable
        self.operation = operation  # '++' or '--'
        self.is_prefix = is_prefix

    def __repr__(self):
        return f"IncrementDecrement(variable='{self.variable}', operation='{self.operation}', is_prefix={self.is_prefix})"

    def to_dict(self):
        return {
            "type": "IncrementDecrement",
            "variable": self.variable,
            "operation": self.operation,
            "is_prefix": self.is_prefix
        }

class IfStatement(ASTNode):
    def __init__(self, condition: ASTNode, then_body: List[ASTNode], else_if_conditions: List[ASTNode] = None, 
                 else_if_bodies: List[List[ASTNode]] = None, else_body: List[ASTNode] = None):
        self.condition = condition
        self.then_body = then_body
        self.else_if_conditions = else_if_conditions or []
        self.else_if_bodies = else_if_bodies or []
        self.else_body = else_body or []

    def __repr__(self):
        return f"IfStatement(condition={self.condition}, then_body={self.then_body}, else_if_conditions={self.else_if_conditions}, else_if_bodies={self.else_if_bodies}, else_body={self.else_body})"

    def to_dict(self):
        return {
            "type": "IfStatement",
            "condition": self.condition.to_dict(),
            "then_body": [stmt.to_dict() for stmt in self.then_body],
            "else_if_conditions": [cond.to_dict() for cond in self.else_if_conditions],
            "else_if_bodies": [[stmt.to_dict() for stmt in body] for body in self.else_if_bodies],
            "else_body": [stmt.to_dict() for stmt in self.else_body]
        }

class MoveWindow(ASTNode):
    def __init__(self, window_name: ASTNode, x: ASTNode, y: ASTNode):
        self.window_name = window_name
        self.x = x
        self.y = y

    def __repr__(self):
        return f"MoveWindow(window_name={self.window_name}, x={self.x}, y={self.y})"

    def to_dict(self):
        return {
            "type": "MoveWindow",
            "window_name": self.window_name.to_dict(),
            "x": self.x.to_dict(),
            "y": self.y.to_dict()
        }

class FocusWindow(ASTNode):
    def __init__(self, window_name: ASTNode):
        self.window_name = window_name

    def __repr__(self):
        return f"FocusWindow(window_name={self.window_name})"

    def to_dict(self):
        return {
            "type": "FocusWindow",
            "window_name": self.window_name.to_dict()
        }

class WindowExists(ASTNode):
    def __init__(self, window_name: ASTNode):
        self.window_name = window_name

    def __repr__(self):
        return f"WindowExists(window_name={self.window_name})"

    def to_dict(self):
        return {
            "type": "WindowExists",
            "window_name": self.window_name.to_dict()
        }
