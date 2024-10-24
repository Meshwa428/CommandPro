import re


class CommandParser:
    def __init__(self):
        # Command patterns
        self.patterns = {
            "MOVE": r"MOVE (MOUSE|WINDOW) ?(.*)? TO \((\d+), (\d+)\);",
            "PRESS": r"PRESS (KEY|BUTTON) (.*);",
            "HOLD": r"HOLD (KEY|BUTTON) (.*);",
            "TYPE": r'TYPE "(.*)";',
            "WAIT": r"WAIT (\d+)s;",
            "REPEAT": r"REPEAT (\d+) TIMES {",  # Start of REPEAT block
            "CLOSE": r"CLOSE WINDOW (.*);",
            "OPEN": r"OPEN (APP|FILE) (.*);",
            "SCROLL": r"SCROLL (MOUSE|WINDOW) (UP|DOWN) (\d+);",
            "FOCUS": r"FOCUS WINDOW (.*);",
            "IF": r'IF WINDOW "(.*)" EXISTS THEN',
            "END_IF": r"END IF;",  # Closing IF block
        }

    def parse(self, code):
        lines = code.splitlines()
        in_repeat_block = False
        in_if_block = False
        repeat_count = 0
        repeat_commands = []
        block_stack = 0

        for line_num, line in enumerate(lines, 1):
            line = line.strip()

            # Remove inline comments
            line = re.sub(r"--.*", "", line).strip()

            # Skip empty lines
            if not line:
                continue

            # Handle block commands like IF or REPEAT
            if in_repeat_block or in_if_block:
                if line == "};" or line == "END IF;":
                    block_stack -= 1
                    if block_stack < 0:
                        print(
                            f"Syntax Error on line {line_num}: Unexpected closing block."
                        )
                    if in_repeat_block:
                        in_repeat_block = False
                        for _ in range(repeat_count):
                            self.parse("\n".join(repeat_commands))  # Re-execute block
                        repeat_commands = []  # Reset block commands after executing
                    if in_if_block:
                        in_if_block = False
                else:
                    if in_repeat_block:
                        repeat_commands.append(line)  # Collect REPEAT block commands
                    # In the case of IF block, commands are executed directly within the block
                continue

            # Check for each command
            for command, pattern in self.patterns.items():
                match = re.match(pattern, line)
                if match:
                    if command == "REPEAT":
                        in_repeat_block = True  # Entering a REPEAT block
                        block_stack += 1
                        try:
                            repeat_count = int(match.groups()[0])
                            if repeat_count <= 0:
                                raise ValueError(
                                    "Repeat count must be greater than zero."
                                )
                        except ValueError as ve:
                            print(f"Runtime Error on line {line_num}: {ve}")
                            return
                    elif command == "IF":
                        in_if_block = True
                        block_stack += 1
                        print(f"Checking if window '{match.groups()[0]}' exists.")
                    else:
                        try:
                            self.execute(command, match.groups(), line_num)
                        except ValueError as ve:
                            print(f"Runtime Error on line {line_num}: {ve}")
                    break
            else:
                print(f"Syntax Error on line {line_num}: Unknown command '{line}'.")

    def execute(self, command, args, line_num):
        if command == "MOVE":
            action_type, window_name, x, y = args
            x, y = int(x), int(y)
            if action_type == "MOUSE":
                print(f"Move mouse to coordinates ({x}, {y}).")
            elif action_type == "WINDOW":
                print(f"Move window '{window_name.strip()}' to coordinates ({x}, {y}).")
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
        elif command == "WAIT":
            seconds = int(args[0])
            if seconds < 0:
                raise ValueError("Wait time cannot be negative.")
            print(f"Wait for {seconds} seconds.")
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
    with open("../test.comp", "r") as file:
        code = file.read()

    parser = CommandParser()
    parser.parse(code)
