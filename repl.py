import json
import logging
from python import Lexer
from python import Parser
from python import Executor
from argparse import ArgumentParser
from python import Program
from python.errors import *

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
        parser = Parser()
        ast = parser.parse(tokens)

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

def interactive_mode(save_ast_path=None, verbose=False):
    """Run the REPL in interactive mode."""
    logger = logging.getLogger(__name__)
    logger.info("Starting the REPL application.")

    executor = Executor(verbose=verbose)
    print("Welcome to the CommandPro REPL. Type 'exit;' to quit.")
    buffer = ""
    prompt = ">>> "
    
    # Track different types of brackets for better balance checking
    brackets = {
        'curly': 0,    # {}
        'round': 0,    # ()
        'square': 0    # []
    }
    
    # Initialize cumulative program for AST
    cumulative_program = Program([])
    parser = Parser()

    def update_bracket_count(line):
        """Update bracket counts for a line of code."""
        brackets['curly'] += line.count('{') - line.count('}')
        brackets['round'] += line.count('(') - line.count(')')
        brackets['square'] += line.count('[') - line.count(']')
        
    def is_complete_statement(buffer):
        """Check if the current buffer contains a complete statement."""
        if not buffer.strip():
            return True
            
        if buffer.strip().endswith(';'):
            return all(count == 0 for count in brackets.values())
            
        if buffer.strip().endswith('}'):
            return all(count == 0 for count in brackets.values())
            
        return False

    def update_ast(new_ast):
        """Update the cumulative AST and save it if needed."""
        nonlocal cumulative_program
        
        # Add new statements to cumulative program
        if isinstance(new_ast, Program):
            cumulative_program.statements.extend(new_ast.statements)
        else:
            cumulative_program.statements.append(new_ast)

        # Save updated AST immediately if path is provided
        if save_ast_path:
            try:
                save_ast_to_json(cumulative_program.to_dict(), save_ast_path)
                logger.debug(f"Updated AST saved to {save_ast_path}")
            except Exception as e:
                logger.error(f"Failed to save AST: {e}")
                print(f"Warning: Failed to save AST: {e}")

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
            update_bracket_count(line)

            if any(count < 0 for count in brackets.values()):
                logger.error("Unmatched closing bracket detected.")
                print("Syntax Error: Unmatched closing bracket.")
                buffer = ""
                brackets = dict.fromkeys(brackets, 0)
                prompt = ">>> "
                continue

            if not is_complete_statement(buffer):
                prompt = "... "
                continue

            prompt = ">>> "

            try:
                code_to_execute = buffer.rstrip()
                
                if not code_to_execute:
                    buffer = ""
                    continue

                # First try to parse without executing to ensure it's valid
                lexer = Lexer(code_to_execute)
                tokens = lexer.tokenize()
                new_ast = parser.parse(tokens)
                # print("Tokens:", tokens)
                # print("AST:", new_ast)
                
                # If parsing succeeds, update the cumulative AST immediately
                update_ast(new_ast)
                
                # Then execute the code
                executor.execute(new_ast)
                logger.info("Execution completed successfully.")

            except SyntaxError as e:
                logger.exception("Parsing failed with error: %s", e)
                print(f"Parsing Error: {e}")
            except Exception as e:
                logger.exception("Execution failed with error: %s", f"{type(e).__name__}: {e}")
                print(f"{type(e).__name__}: {e}")

            buffer = ""
            brackets = dict.fromkeys(brackets, 0)

        except (EOFError, KeyboardInterrupt):
            print("\nGoodbye!")
            break

def execute_code_string(code_string, save_ast_path=None, verbose=False):
    """Execute code passed as a string."""
    try:
        lexer = Lexer(code_string)
        tokens = lexer.tokenize()
        executor = Executor(verbose=verbose)
        parser = Parser()
        ast = parser.parse(tokens)

        if save_ast_path:
            save_ast_to_json(ast.to_dict(), save_ast_path)

        executor.execute(ast)
    except Exception as e:
        print(f"Error: {str(e)}")
        return 1

    return 0

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
    parser.add_argument("-c", "--code", help="Code string to execute directly")

    args = parser.parse_args()

    # Setup logging based on arguments
    setup_logging(args.log, args.log_level, args.log_file)

    try:
        if args.code:
            # Execute code passed as a string
            execute_code_string(args.code, args.save_ast_path, args.verbose)
        elif args.ast_path:
            # Execute from AST file
            execute_from_ast(args.ast_path, args.verbose)
        elif args.file:
            # Execute from code file
            execute_from_file(args.file, args.save_ast_path, args.verbose)
        elif args.interactive:
            # Run in interactive mode with AST saving if path is provided
            interactive_mode(args.save_ast_path, args.verbose)
        else:
            # Default to interactive mode
            interactive_mode(args.save_ast_path, args.verbose)
    except Exception as e:
        print(f"Error: {str(e)}")
        return 1

    return 0

if __name__ == "__main__":
    exit(main())

