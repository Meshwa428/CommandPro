import json
import logging
from lexer import Lexer
from parser import Parser
from executor import Executor
from argparse import ArgumentParser
from errors import *

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

def load_ast_from_json(ast_path):
    """Load AST from a JSON file."""
    try:
        with open(ast_path, 'r') as file:
            return json.load(file)
    except FileNotFoundError:
        raise FileNotFoundError(f"AST file not found: {ast_path}")
    except json.JSONDecodeError:
        raise ValueError(f"Invalid JSON in AST file: {ast_path}")

def save_ast_to_json(ast, save_path):
    """Save AST to a JSON file."""
    try:
        with open(save_path, 'w') as file:
            json.dump(ast, file, indent=4)
    except Exception as e:
        raise Exception(f"Failed to save AST to {save_path}: {str(e)}")

def execute_from_file(file_path, save_ast_path=None, verbose=False):
    """Execute code from a file."""
    try:
        with open(file_path, 'r') as file:
            code = file.read()

        lexer = Lexer(code)
        tokens = lexer.tokenize()
        executor = Executor(verbose=verbose)
        parser = Parser(tokens, executor.global_scope, executor.functions)
        ast = parser.parse()

        if save_ast_path:
            save_ast_to_json(ast, save_ast_path)

        executor.execute(ast)
    except FileNotFoundError:
        raise FileNotFoundError(f"Code file not found: {file_path}")
    except Exception as e:
        raise Exception(f"Failed to execute file {file_path}: {str(e)}")

def execute_from_ast(ast_path, verbose=False):
    """Execute code from an AST file."""
    ast = load_ast_from_json(ast_path)
    executor = Executor(verbose=verbose)
    executor.execute(ast)

def interactive_mode(verbose=False):
    """Run the REPL in interactive mode."""
    logger = logging.getLogger(__name__)
    logger.info("Starting the REPL application.")

    executor = Executor(verbose=verbose)
    print("Welcome to the CommandPro REPL. Type 'exit;' to quit.")
    buffer = ""
    prompt = ">>> "
    brace_balance = 0

    while True:
        try:
            print(prompt, end="")
            line = input()
            if not line.strip():
                continue

            if line.strip().lower() == 'exit;':
                logger.info("Exit command received. Terminating REPL.")
                print("Goodbye!")
                break

            buffer += line + "\n"
            brace_balance += line.count('{') - line.count('}')

            if brace_balance > 0:
                prompt = "... "
                continue
            elif '{' in buffer and brace_balance < 0:
                logger.error("Unbalanced braces detected.")
                print("Syntax Error: Unbalanced braces.")
                buffer = ""
                brace_balance = 0
                prompt = ">>> "
                continue
            elif brace_balance != 0:
                prompt = "... "
                continue

            try:
                lexer = Lexer(buffer)
                tokens = lexer.tokenize()
                parser = Parser(tokens, executor.global_scope, executor.functions)
                ast = parser.parse()
                executor.execute(ast)
                logger.info("Execution completed successfully.")
            except SyntaxError as e:
                logger.exception("Parsing failed with error: %s", e)
                print(f"Parsing Error: {e}")
            except Exception as e:
                logger.exception("Execution failed with error: %s", f"{type(e).__name__}: {e}")
                print(f"{type(e).__name__}: {e}")


            buffer = ""
            prompt = ">>> "

        except (EOFError, KeyboardInterrupt):
            print("\nGoodbye!")
            break

def main():
    parser = ArgumentParser(description="CommandPro Interpreter")
    parser.add_argument("-f", "--file", help="File to run")
    parser.add_argument("-s", "--save_ast_path", help="Save the AST to a json file")
    parser.add_argument("-ast", "--ast_path", help="Path to the AST file")
    parser.add_argument("-i", "--interactive", action="store_true", help="Run in interactive mode")
    parser.add_argument("-log", "--log", action="store_true", help="Enable logging")
    parser.add_argument("-log_level", "--log_level", type=int, default=logging.DEBUG, help="Log level")
    parser.add_argument("-log_file", "--log_file", default="app.log", help="Log file path")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose mode")
    
    args = parser.parse_args()

    # Setup logging based on arguments
    setup_logging(args.log, args.log_level, args.log_file)

    try:
        if args.ast_path:
            # Execute from AST file
            execute_from_ast(args.ast_path, args.verbose)
        elif args.file:
            # Execute from code file
            execute_from_file(args.file, args.save_ast_path, args.verbose)
        elif args.interactive:
            # Run in interactive mode
            interactive_mode(args.verbose)
        else:
            # Default to interactive mode
            interactive_mode(args.verbose)
    except Exception as e:
        print(f"Error: {str(e)}")
        return 1

    return 0

if __name__ == "__main__":
    exit(main())

