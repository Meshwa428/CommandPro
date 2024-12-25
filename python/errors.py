class InvalidNumberError(Exception):
    """Custom exception for invalid number input."""

    pass

class CustomError(Exception):
    """Base class for custom exceptions."""

    pass

class ZeroDivisionError(CustomError):
    """Exception raised for division by zero."""
    def __init__(self, message):
        super().__init__(message)

class SyntaxError(CustomError):
    """Exception raised for syntax errors in the input."""
    def __init__(self, message):
        super().__init__(message)

class RuntimeError(CustomError):
    """Exception raised for runtime errors during execution."""
    def __init__(self, message):
        super().__init__(message)

class TypeError(CustomError):
    """Exception raised for type mismatch errors."""
    def __init__(self, message):
        super().__init__(message)

class ControlFlowException(CustomError):
    """Exception raised for control flow statements (break, continue, return, etc.)."""
    def __init__(self, statement_type, value=None):
        self.statement_type = statement_type  # BREAK, CONTINUE, RETURN, YIELD
        self.value = value  # For RETURN and YIELD statements
        super().__init__(f"Control flow {statement_type} with value {value}")

class ContinueException(CustomError):
    """Exception raised for continue statements."""
    def __init__(self, message):
        super().__init__(message)

