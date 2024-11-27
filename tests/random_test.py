import re


class CommandParser:
    def __init__(self):
        self.patterns = {
            "MOVE": r"MOVE (MOUSE|WINDOW) ?(.*)? TO \((\d+|\w+), (\d+|\w+)\);",
            "PRESS": r"PRESS (KEY|BUTTON) (.*);",
            "HOLD": r"HOLD (KEY|BUTTON) (.*);",
            "TYPE": r'TYPE "(.*)";',
            "WAIT": r"WAIT\s+((?:\d+(?:\.\d+)?)(?:h|m|s|ms)?|[\w]+);",
            "REPEAT": r"REPEAT (\d+) TIMES {",
            "CLOSE": r"CLOSE WINDOW (.*);",
            "OPEN": r"OPEN (APP|FILE) (.*);",
            "SCROLL": r"SCROLL (MOUSE|WINDOW) (UP|DOWN) (\d+);",
            "FOCUS": r"FOCUS WINDOW (.*);",
            "IF": r'IF WINDOW "(.*)" EXISTS THEN',
            "END_IF": r"END IF;",
            "DEFFUN": r"DEFFUN (\w+)\(([\w\s,]*)\) {",
            "FUNCTION_CALL": r"(\w+)\((.*)\);",  # Captures Function calls
            "SET": r"SET (\w+) = (.*);",
            "PRINT": r"PRINT\s+(.*);",  # Modified to capture entire expression
            "PRINTLN": r"PRINTLN\s+(.*);",  # Modified to capture entire expression
            "INPUT": r'INPUT\s+"([^"]*)"\s+(?:TO|INTO)\s+(\w+)(?:\s+AS\s+(INT|FLOAT|STR))?;',  # Updated to handle type specification
        }
        self.functions = {}
        self.variables = {}
        self.type_converters = {"INT": int, "FLOAT": float, "STR": str}

    def is_valid_variable_name(self, var_name):
        # Regex pattern for valid Python variable names
        pattern = r"^[a-zA-Z_][a-zA-Z0-9_]*$"

        # Check if the variable name matches the pattern
        if re.match(pattern, var_name):
            return True
        else:
            return False

    def evaluate_expression(self, expr, variables):
        """Evaluate a print expression that might contain variables and strings."""
        parts = []
        # Split the expression by quotes to separate string literals and variable references
        segments = re.split(r'("(?:[^"\\]|\\.)*")', expr)
        for segment in segments:
            if not segment:
                continue
            if segment.startswith('"') and segment.endswith('"'):
                # String literal - remove quotes and add to parts
                parts.append(segment[1:-1])
            else:
                # Check for variables and operators
                tokens = segment.strip().split()
                for token in tokens:
                    if token in ["+"]:  # Handle concatenation operator
                        continue
                    if token in variables:
                        parts.append(str(variables[token]))
                    else:
                        try:
                            # Try to evaluate as a numeric expression
                            parts.append(
                                str(eval(token, {"__builtins__": None}, variables))
                            )
                        except:
                            parts.append(token)
        return "".join(parts)

    def parse_block(self, lines, start_index, variables):
        """Parse a block of code and return the commands and ending index."""
        block_commands = []
        i = start_index
        block_stack = 1

        while i < len(lines) and block_stack > 0:
            line = lines[i].strip()
            i += 1

            if not line or line.startswith("--"):
                continue

            if line == "};" or line == "END IF;":
                block_stack -= 1
                if block_stack == 0:
                    break
            elif any(
                line.endswith("{")
                for pattern in [
                    self.patterns["REPEAT"],
                    self.patterns["IF"],
                    self.patterns["DEFFUN"],
                ]
            ):
                block_stack += 1

            if block_stack > 0:
                block_commands.append(line)

        return block_commands, i

    def convert_to_milliseconds(self, time_str):
        """Convert a time string with units to milliseconds."""
        # If it's already a number, convert to milliseconds
        if isinstance(time_str, (int, float)):
            return int(time_str * 1000)

        # If it's a variable name, resolve it from variables
        if time_str in self.variables:
            time_str = self.variables[time_str]

        time_str = str(time_str).strip()
        match = re.match(r"^(\d+(?:\.\d+)?)(h|m|s|ms)?$", time_str)
        if not match:
            raise ValueError(f"Invalid time format: {time_str}")

        value, unit = match.groups()
        value = float(value)

        if unit == "h":
            return int(value * 3600 * 1000)
        elif unit == "m":
            return int(value * 60 * 1000)
        elif unit == "s":
            return int(value * 1000)
        elif unit == "ms":
            return int(value)
        else:
            return int(value * 1000)  # Default to seconds if no unit specified

    def format_time_for_display(self, milliseconds):
        """Format milliseconds into a human-readable string."""
        if milliseconds >= 3600000:
            return f"{milliseconds/3600000:.1f} hours"
        elif milliseconds >= 60000:
            return f"{milliseconds/60000:.1f} minutes"
        elif milliseconds >= 1000:
            return f"{milliseconds/1000:.1f} seconds"
        else:
            return f"{milliseconds} milliseconds"

    def process_escape_sequences(self, text):
        """Process special characters in string literals."""
        special_chars = {
            r"\n": "\n",
            r"\t": "\t",
            r"\r": "\r",
            r"\b": "\b",
            r"\f": "\f",
            r"\"": '"',
            r"\'": "'",
            r"\\": "\\",
        }
        for escaped, char in special_chars.items():
            text = text.replace(escaped, char)
        return text

    def resolve_variable(self, var_name, variables):
        """Resolve variable value from the current scope."""
        if var_name in variables:
            return variables[var_name]
        try:
            # Check if it's a coordinate or numeric value
            if re.match(r"^\d+$", var_name):
                return int(var_name)
            elif re.match(r"^\d*\.\d+$", var_name):
                return float(var_name)
            return var_name
        except ValueError:
            return var_name

    def parse(self, code, local_vars=None):
        if local_vars is None:
            local_vars = {}

        lines = code.splitlines()
        variables = {**self.variables, **local_vars}
        i = 0

        while i < len(lines):
            line = lines[i].strip()
            i += 1

            # evaluate comment
            if not line or line.startswith("--"):
                continue

            # Handle REPEAT blocks
            repeat_match = re.match(self.patterns["REPEAT"], line)
            if repeat_match:
                repeat_count = int(repeat_match.groups()[0])
                repeat_commands, new_i = self.parse_block(lines, i, variables)
                i = new_i
                for _ in range(repeat_count):
                    self.parse("\n".join(repeat_commands), variables)
                continue

            # Handle IF blocks
            if_match = re.match(self.patterns["IF"], line)
            if if_match:
                window_name = if_match.groups()[0]
                print(f"Checking if window '{window_name}' exists.")
                if_commands, new_i = self.parse_block(lines, i, variables)
                i = new_i
                self.parse("\n".join(if_commands), variables)
                continue

            # Handle function definitions
            deffun_match = re.match(self.patterns["DEFFUN"], line)
            if deffun_match:
                func_name, params_str = deffun_match.groups()
                func_commands, new_i = self.parse_block(lines, i, variables)
                i = new_i
                self.functions[func_name] = {
                    "params": self.parse_args(params_str) if params_str else [],
                    "body": "\n".join(func_commands),
                }
                continue

            # Handle function calls - Updated pattern to handle time units
            function_match = re.match(r"(\w+)\((.*)\);", line)
            if function_match:
                func_name, args_str = function_match.groups()
                if func_name in self.functions:
                    func_info = self.functions[func_name]
                    # Parse arguments handling special cases like time units
                    provided_args = []
                    if args_str:
                        # Split by commas but handle nested parentheses
                        args = []
                        current_arg = ""
                        paren_count = 0

                        for char in args_str:
                            if char == "(" and paren_count == 0:
                                paren_count += 1
                                current_arg += char
                            elif char == ")" and paren_count == 1:
                                paren_count -= 1
                                current_arg += char
                            elif char == "," and paren_count == 0:
                                args.append(current_arg.strip())
                                current_arg = ""
                            else:
                                current_arg += char
                                if char == "(":
                                    paren_count += 1
                                elif char == ")":
                                    paren_count -= 1

                        if current_arg:
                            args.append(current_arg.strip())

                        for arg in args:
                            # Check if argument is a time value
                            time_match = re.match(r"^(\d+(?:\.\d+)?)(h|m|s|ms)$", arg)
                            if time_match:
                                provided_args.append(arg)
                            else:
                                resolved_value = self.resolve_variable(arg, variables)
                                provided_args.append(resolved_value)

                    if len(provided_args) != len(func_info["params"]):
                        raise ValueError(
                            f"Function {func_name} expects {len(func_info['params'])} arguments, got {len(provided_args)}"
                        )

                    func_vars = {}
                    for param, arg in zip(func_info["params"], provided_args):
                        func_vars[param] = arg

                    self.parse(func_info["body"], func_vars)
                    continue

            # Handle other commands
            for command, pattern in self.patterns.items():
                match = re.match(pattern, line)
                # print(match)
                if match:
                    try:
                        self.execute(command, match.groups(), variables)
                    except ValueError as ve:
                        print(f"Runtime Error: {ve}")
                    break
            else:
                if line != "};" and line != "END IF;":
                    raise SyntaxError(f"Unknown command '{line}', line '{i}'")

    def parse_args(self, args_str):
        """Parse function arguments, handling quoted strings and preserving whitespace."""
        if not args_str or args_str.strip() == "":
            return []

        args = []
        current_arg = ""
        in_quotes = False

        for char in args_str:
            if char == '"':
                in_quotes = not in_quotes
                current_arg += char
            elif char == "," and not in_quotes:
                args.append(current_arg.strip())
                current_arg = ""
            else:
                current_arg += char

        if current_arg:
            args.append(current_arg.strip())

        return args

    def convert_value(self, value, type_name):
        """Convert a value to the specified type."""
        try:
            if type_name in self.type_converters:
                return self.type_converters[type_name](value)
            return value
        except ValueError:
            raise ValueError(f"Cannot convert '{value}' to {type_name}")

    def execute(self, command, args, variables):
        if command == "MOVE":
            action_type, window_name, x, y = args
            x = self.resolve_variable(x, variables)
            y = self.resolve_variable(y, variables)
            if action_type == "MOUSE":
                print(f"Move mouse to coordinates ({x}, {y}).")
            elif action_type == "WINDOW":
                print(f"Move window '{window_name.strip()}' to coordinates ({x}, {y}).")

        elif command == "WAIT":
            time_value = args[0]
            try:
                # First check if it's a variable in the local scope
                if time_value in variables:
                    time_value = variables[time_value]

                # Convert time value to milliseconds
                ms = self.convert_to_milliseconds(time_value)
                formatted_time = self.format_time_for_display(ms)
                print(f"Wait for {formatted_time}.")
            except ValueError as e:
                raise ValueError(f"Invalid wait time format: {time_value}")

        elif command == "PRINT" or command == "PRINTLN":
            expression = args[0]
            output = self.evaluate_expression(expression, variables)
            output = self.process_escape_sequences(output)
            if command == "PRINTLN":
                print(output)
            else:
                print(output, end="")

        elif command == "INPUT":
            prompt, var_name, type_spec = args + (None,) if len(args) == 2 else args
            # Process escape sequences in the prompt and get input
            prompt = self.process_escape_sequences(prompt)
            user_input = input(prompt)

            try:
                # Convert input to specified type if a type was specified
                if type_spec:
                    user_input = self.convert_value(user_input, type_spec)
            except ValueError as e:
                print(f"Error: {e}")
                return

            # Store in variables
            variables[var_name] = user_input
            if not isinstance(variables, dict):
                self.variables[var_name] = user_input

        elif command == "SET":
            var_name, value = args
            if self.is_valid_variable_name(var_name):
                # Check if it's a time value with units
                time_match = re.match(r"^(\d+(?:\.\d+)?)(h|m|s|ms)$", value.strip())
                if time_match:
                    variables[var_name] = value.strip()
                    if not isinstance(variables, dict):
                        self.variables[var_name] = value.strip()
                else:
                    try:
                        # Handle other types of values
                        if value.startswith('"') and value.endswith('"'):
                            evaluated_value = value[1:-1]
                        else:
                            # Try to evaluate as numeric expression
                            evaluated_value = eval(
                                value, {"__builtins__": None}, variables
                            )
                        variables[var_name] = evaluated_value
                        if not isinstance(variables, dict):
                            self.variables[var_name] = evaluated_value
                    except Exception as e:
                        raise ValueError(
                            f"Error evaluating expression: {e}, {var_name} has invalid value"
                        )
            else:
                raise SyntaxError(f"'{var_name}' is not a valid Identifier!")

        elif command == "PRESS":
            action_type, key = args
            if not key.isalnum():
                raise ValueError(f"Invalid key '{key}'.")
            print(f"Press {action_type.lower()} '{key}'.")

        elif command == "HOLD":
            action_type, key = args
            if not key.isalnum():
                raise ValueError(f"Invalid key '{key}'.")
            print(f"Hold {action_type.lower()} '{key}'.")

        elif command == "TYPE":
            text = args[0]
            print(f"Type text: '{text}'.")

        elif command == "CLOSE":
            window_name = args[0]
            print(f"Close window '{window_name}'.")

        elif command == "OPEN":
            app_type, name = args
            print(f"Open {app_type.lower()} '{name}'.")

        elif command == "SCROLL":
            action_type, direction, steps = args
            steps = int(steps)
            if steps <= 0:
                raise ValueError("Scroll steps must be greater than zero.")
            print(f"Scroll {action_type.lower()} {direction.lower()} {steps} steps.")

        elif command == "FOCUS":
            window_name = args[0]
            print(f"Focus on window '{window_name}'.")


# Example usage
if __name__ == "__main__":
    # code = """
    # SET test = "Multi line test";
    # SET test2 = test + " huh!??";

    # PRINTLN test2;
    # """
    # parser = CommandParser()
    # parser.parse(code)

    # with open("../test.comp", "r") as file:
    #     code = file.read()

    from prompt_toolkit import PromptSession

    # Create the input session
    session = PromptSession()

    def custom_input():
        print("Press Enter for multiline input, or Esc + Enter to submit.")
        return session.prompt("> ", multiline=True)

    parser = CommandParser()

    while True:
        user_input = custom_input()
        parser.parse(user_input)
