# Custom Control Language for AI-Driven Computer Interaction

## Overview
This project aims to create a custom language with SQL-like syntax to control a computer via an AI interface. The language provides commands to manipulate mouse movements, simulate keyboard actions, and automate repetitive tasks. The primary goal is to develop a flexible framework for training AI to execute tasks on a computer environment effectively.

## Current Implementation
The following base commands and attributes have been implemented:

### Base Commands
1. **MOVE**: Move the mouse cursor or a window.
   - Example: `MOVE MOUSE TO (X, Y);`
  
2. **PRESS**: Simulate a single key press or mouse button click.
   - Example: `PRESS KEY [key_name];`
  
3. **HOLD**: Keep a key or button pressed until released.
   - Example: `HOLD BUTTON [mouse_button];`
  
4. **TYPE**: Type a string of text using keyboard simulation.
   - Example: `TYPE "your_text_here";`

### Additional Features
- **TIME**: Controls the duration for actions.
- **WAIT**: Introduces pauses between actions.
- **REPEAT**: Repeats actions a specified number of times.
- **IF CONDITION**: Adds conditional logic to commands.
- **SCROLL**: Scrolls the mouse wheel or window contents.
- **CLOSE**: Closes an application or window.
- **OPEN**: Opens an application or file.
- **CAPTURE**: Captures the screen or a window.
- **FOCUS**: Brings a specific window to the front.

## Future Vision
The future vision for this project includes:
- Expanding the command set to include more advanced actions (e.g., drag-and-drop, context menu interactions).
- Integrating machine learning models to enable adaptive learning and context-aware actions.
- Developing a user-friendly interface for writing and testing scripts in this custom language.
- Implementing a parser and interpreter to process the custom syntax and execute commands in real-time.
- Building a community around this project to gather feedback, enhance features, and contribute new ideas.

## Explanatory Part
This custom language is designed to be intuitive and easy to use, similar to SQL. Developers can issue commands in a structured format, making it straightforward to understand and implement. The syntax allows for clear delineation between different types of actions (mouse movements, key presses, etc.) while maintaining flexibility in how those actions are executed.

## Syntax
The following is a brief overview of the syntax used in this language:

### Command Syntax
```sql
MOVE MOUSE TO (X, Y);          -- Move the mouse cursor to coordinates (X, Y)
MOVE WINDOW [window_name] TO (X, Y);  -- Move a window to coordinates (X, Y)
PRESS KEY [key_name];          -- Simulate a single key press
HOLD KEY [key_name];           -- Hold a key down
TYPE "your_text_here";         -- Type a string of text
WAIT [duration];               -- Pause for a specified duration
REPEAT [number] TIMES {         -- Repeat a block of code
    [commands]
};
IF [condition] THEN {           -- Conditional logic
    [commands]
};
```

## Examples
Here are some practical examples to illustrate how to use the custom language:

1. **Open a Browser and Type a URL:**
   ```sql
   OPEN APP "Browser";
   WAIT 2s;  -- Wait for the app to open
   TYPE "https://example.com";
   PRESS KEY ENTER;  -- Press Enter to navigate
   ```

2. **Move Mouse and Hold a Key:**
   ```sql
   MOVE MOUSE TO (200, 300) TIME 5s;  -- Move mouse to coordinates over 5 seconds
   HOLD KEY "A";  -- Hold down the "A" key
   WAIT 2s;  -- Hold for 2 seconds
   HOLD KEY "A";  -- Release the "A" key
   ```

3. **Conditional Logic Example:**
   ```sql
   IF WINDOW "Calculator" EXISTS THEN {
       FOCUS WINDOW "Calculator";  -- Bring Calculator to the front
       TYPE "123 + 456";  -- Type a calculation
       PRESS KEY ENTER;  -- Press Enter to calculate
   };
   ```

## What Next
- **Documentation:** Further documentation will be developed to provide in-depth usage guides and tutorials for users.
- **Community Engagement:** Creating a platform for developers to share their scripts and experiences using this language.
- **Feature Expansion:** Gathering feedback to identify new features and improvements that can be implemented in future updates.
- **Testing and Validation:** Establishing a robust testing framework to ensure reliability and performance of the commands.

For contributions, feedback, or inquiries, please contact the project maintainer.

---

**Thank you for your interest in this project!**  
