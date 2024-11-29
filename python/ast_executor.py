from typing import Dict, Any
from enum import Enum, auto
from ir_decoder import IRDecoder


class AssignmentOperator(Enum):
    SIMPLE = auto()
    ADD = auto()
    SUBTRACT = auto()
    MULTIPLY = auto()
    DIVIDE = auto()
    MODULO = auto()
    FLOOR_DIVIDE = auto()
    POWER = auto()
    BITWISE_AND = auto()
    BITWISE_OR = auto()
    BITWISE_XOR = auto()
    RIGHT_SHIFT = auto()
    LEFT_SHIFT = auto()


class ASTExecutor:
    def __init__(self):
        self.variables: Dict[str, Any] = {}

    def execute(self, ir_path: str):
        """
        Execute the Intermediate Representation from the JSON file
        """
        ir_data = IRDecoder.decode_ir(ir_path)

        for node in ir_data.get("nodes", []):
            node_type = node.get("type")

            if node_type == "VariableDeclaration":
                self._handle_variable_declaration(node)
            elif node_type == "Print":
                self._handle_print_statement(node)

    def _handle_variable_declaration(self, node: Dict[str, Any]):
        """
        Handle variable declaration and assignment
        """
        var_data = node.get("VariableDeclaration", {})
        name = var_data.get("name")
        value = self._convert_literal(var_data.get("value", {}))
        op = self._get_assignment_operator(var_data.get("assignment_type"))

        # Perform the appropriate assignment operation
        if op == AssignmentOperator.SIMPLE:
            self.variables[name] = value
        elif op == AssignmentOperator.ADD:
            self.variables[name] += value
        elif op == AssignmentOperator.SUBTRACT:
            self.variables[name] -= value
        elif op == AssignmentOperator.MULTIPLY:
            self.variables[name] *= value
        elif op == AssignmentOperator.DIVIDE:
            self.variables[name] /= value
        elif op == AssignmentOperator.MODULO:
            self.variables[name] %= value
        elif op == AssignmentOperator.FLOOR_DIVIDE:
            self.variables[name] //= value
        elif op == AssignmentOperator.POWER:
            self.variables[name] **= value
        elif op == AssignmentOperator.BITWISE_AND:
            self.variables[name] &= value
        elif op == AssignmentOperator.BITWISE_OR:
            self.variables[name] |= value
        elif op == AssignmentOperator.BITWISE_XOR:
            self.variables[name] ^= value
        elif op == AssignmentOperator.RIGHT_SHIFT:
            self.variables[name] >>= value
        elif op == AssignmentOperator.LEFT_SHIFT:
            self.variables[name] <<= value

    def _handle_print_statement(self, node: Dict[str, Any]):
        """
        Handle print statement
        """
        print_data = node.get("Print", {})
        expression = print_data.get("expression", "")

        # Check if the expression is a variable
        if expression in self.variables:
            print(self.variables[expression])
        else:
            print(expression)

    def _convert_literal(self, literal: Dict[str, Any]) -> Any:
        """
        Convert IR literal to Python value
        """
        if "Integer" in literal:
            return int(literal["Integer"])
        elif "Float" in literal:
            return float(literal["Float"])
        elif "String" in literal:
            return str(literal["String"])
        raise ValueError(f"Unknown literal type: {literal}")

    def _get_assignment_operator(self, op_type: str) -> AssignmentOperator:
        """
        Convert IR assignment operator to Python enum
        """
        operator_map = {
            "Simple": AssignmentOperator.SIMPLE,
            "Add": AssignmentOperator.ADD,
            "Subtract": AssignmentOperator.SUBTRACT,
            "Multiply": AssignmentOperator.MULTIPLY,
            "Divide": AssignmentOperator.DIVIDE,
            "Modulo": AssignmentOperator.MODULO,
            "FloorDivide": AssignmentOperator.FLOOR_DIVIDE,
            "Power": AssignmentOperator.POWER,
            "BitwiseAnd": AssignmentOperator.BITWISE_AND,
            "BitwiseOr": AssignmentOperator.BITWISE_OR,
            "BitwiseXor": AssignmentOperator.BITWISE_XOR,
            "RightShift": AssignmentOperator.RIGHT_SHIFT,
            "LeftShift": AssignmentOperator.LEFT_SHIFT,
        }
        return operator_map.get(op_type, AssignmentOperator.SIMPLE)


def main():
    executor = ASTExecutor()
    executor.execute("output/ast_ir.json")


if __name__ == "__main__":
    main()
