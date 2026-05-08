# Synapse

## Overview
Synapse is a high-performance, cross-platform language written in C++ designed specifically to control a computer through AI interfaces or manual scripting. It provides a natural-language syntax mapped to underlying Object-Oriented paradigms to manipulate mouse movements, simulate keyboard actions, manage windows, and automate repetitive tasks. 

Files use the `.syn` extension.

## Syntax & Grammar
Synapse enforces a rigid grammar to remove ambiguity, particularly to make it easier for AI logic or parsers to execute without error.

**Basic Grammar Structure:**
`<ACTION> <TARGET> [ATTRIBUTES];`

### Variables
```sql
let message = "Welcome to Synapse!";
let speed = 1.5;
```

### Future Automation Features
```sql
MOUSE MOVE TO (300, 400);
MOUSE CLICK LEFT AT (100, 300) TIMES 2;
KEY PRESS SPACE;
WINDOW OPEN "Notepad";
```

## Compiler/Interpreter Architecture (C++)
This project is structured around a traditional compiler frontend with an interpreting backend built in C++:
1. **Lexer**: Tokenizes raw `.syn` files.
2. **Parser**: Generates an Abstract Syntax Tree (AST) mapping natural language actions into OOP structures.
3. **Interpreter**: Executes the AST nodes using C++ OS-level libraries for rapid performance.
