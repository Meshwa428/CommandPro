# content of conftest.py
import logging
import pytest
from python.lexer import Lexer
from python.parser import Parser
from python.executor import Executor

def setup_logging(enable_logging=False, log_level=logging.DEBUG, log_file="app.log"):
    """Configure logging for the application."""
    if not enable_logging:
        logging.disable(logging.CRITICAL)
        return
    
    logging.basicConfig(
        level=log_level,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        handlers=[
            logging.FileHandler(log_file),
            logging.StreamHandler()
        ]
    )

@pytest.fixture
def executor():
    return Executor()

@pytest.fixture
def parser():
    return Parser()