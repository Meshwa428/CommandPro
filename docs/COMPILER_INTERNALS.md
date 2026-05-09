# Synapse Compiler Internals

> This document is a deep-dive into the Synapse compiler's internal architecture, intended for contributors and developers who wish to understand or extend the compiler.

---

## Overview

The Synapse compiler is a **classic four-phase interpreter** written in C++17. It does **not** generate machine code or bytecode in its current form — it directly interprets the AST at runtime. This is sometimes called a **Tree-Walking Interpreter**.

```
Source (.syn)
     │
     ▼
 [PHASE 1] Lexical Analysis (Lexer)
     │   Reads characters, emits Tokens
     ▼
 [PHASE 2] Syntactic Analysis (Parser)
     │   Reads Tokens, builds Abstract Syntax Tree
     ▼
 [PHASE 3] Semantic Analysis (Checker)
     │   Validates types, scopes, declarations
     ▼
  [PHASE 4] Execution (Bytecode VM)
     │   Compiles AST to OpCodes, Executes in VM
     ▼
  OS APIs (X11, Win32)
```

---

## Phase 1: Lexical Analysis (Lexer)

### Responsibility
Convert a raw character stream into a flat sequence of `Token` objects.

### How It Works
The lexer maintains:
- `position`: current index into the source string
- `line`, `column`: for error reporting
- `currentChar`: the character at `position`

It scans character by character and applies the following rules:

| Input                       | Output Token         |
|-----------------------------|----------------------|
| `#`                         | Skip until newline (comment) |
| Whitespace                  | Skip                 |
| `[a-zA-Z_]`                 | Identifier or Keyword |
| `[0-9]`                     | Number literal       |
| `"` or `'`                  | String literal       |
| `=`                         | EQUALS               |
| `;`                         | SEMICOLON            |
| `(`                         | LPAREN               |
| `)`                         | RPAREN               |
| `{`                         | LBRACE               |
| `}`                         | RBRACE               |
| `+`, `-`, `*`, `/`, etc.    | Operator             |
| Unknown character           | LexerError exception |

### Token Structure
```cpp
struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
};
```

### Keyword Resolution
When the lexer scans an identifier, it checks the keyword map:
```cpp
static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"let",    TokenType::LET},
    {"fn",     TokenType::FN},
    {"if",     TokenType::IF},
    {"else",   TokenType::ELSE},
    {"repeat", TokenType::REPEAT},
    {"times",  TokenType::TIMES},
    {"loop",   TokenType::LOOP},
    {"while",  TokenType::WHILE},
    {"return", TokenType::RETURN},
    {"true",   TokenType::TRUE_LIT},
    {"false",  TokenType::FALSE_LIT},
    {"null",   TokenType::NULL_LIT},
    // ... automation keywords
    {"MOUSE",  TokenType::MOUSE},
    {"KEY",    TokenType::KEY},
    {"WINDOW", TokenType::WINDOW},
    // etc.
};
```
If a scanned identifier is not in the keyword map, it is classified as `IDENTIFIER`.

---

## Phase 2: Syntactic Analysis (Parser)

### Responsibility
Convert the token stream into a typed, hierarchical **Abstract Syntax Tree (AST)**.

### Strategy: Recursive Descent Parsing
Each grammar rule maps to a dedicated `parse*` method. The parser calls these methods recursively to match the input, building AST nodes as it goes.

```
parseProgram()
  └─ parseStatement()
        ├─ parseVariableDeclaration()  →  "let x = 5;"
        ├─ parsePrintStatement()       →  "print x;"
        ├─ parseFunctionDeclaration()  →  "fn foo(a, b) { ... }"
        ├─ parseFunctionCall()         →  "foo(1, 2);"
        ├─ parseIfStatement()          →  "if (...) { ... } else { ... }"
        ├─ parseRepeatStatement()      →  "repeat N times { ... }"
        ├─ parseWhileLoop()            →  "loop while (...) { ... }"
        ├─ parseMouseCommand()         →  "MOUSE CLICK LEFT AT (x, y);"
        ├─ parseKeyCommand()           →  "KEY PRESS ENTER;"
        ├─ parseWindowCommand()        →  "WINDOW FOCUS "App";"
        └─ ...etc
```

### AST Node Hierarchy

All AST nodes inherit from the base `ASTNode` class and implement the `accept(ASTVisitor&)` method (Visitor Pattern):

```
ASTNode (abstract base)
├── ProgramNode             (root: list of statements)
├── Literals
│   ├── IntLiteralNode
│   ├── FloatLiteralNode
│   ├── StringLiteralNode
│   ├── BoolLiteralNode
│   ├── TimeLiteralNode     (e.g., 500ms, 2s, 1h)
│   └── PointLiteralNode    (x, y pairs)
├── IdentifierNode
├── BinaryExpressionNode    (left OP right)
├── UnaryExpressionNode     (OP value)
├── VariableDeclarationNode (let name = value)
├── AssignmentNode          (name = value)
├── CompoundAssignmentNode  (x += y, x -= y, etc.)
├── BlockNode               (a list of statements)
├── IfStatementNode         (condition, then-block, else-block)
├── RepeatStatementNode     (count, body)
├── WhileStatementNode      (condition, body)
├── FunctionDeclarationNode (name, params, body)
├── FunctionCallNode        (name, args)
├── ReturnStatementNode
├── PrintStatementNode
├── AskStatementNode        (ASK "prompt" INTO var)
├── WaitStatementNode       (WAIT duration)
├── TryCatchNode
│
├── Mouse Nodes
│   ├── MouseMoveNode       (MOUSE MOVE TO (x, y))
│   ├── MouseClickNode      (MOUSE CLICK LEFT AT (x, y) TIMES n)
│   ├── MouseDragNode       (MOUSE DRAG FROM (x1,y1) TO (x2,y2))
│   ├── MouseScrollNode     (MOUSE SCROLL UP/DOWN n)
│   ├── MouseHoldNode
│   └── MouseReleaseNode
│
├── Key Nodes
│   ├── KeyPressNode        (KEY PRESS key/combo)
│   ├── KeyHoldNode
│   ├── KeyReleaseNode
│   └── KeyTypeNode         (KEY TYPE "text")
│
├── Window Nodes
│   ├── WindowOpenNode      (WINDOW OPEN "name")
│   ├── WindowCloseNode
│   ├── WindowFocusNode
│   ├── WindowMoveNode
│   ├── WindowResizeNode
│   ├── WindowMinimizeNode
│   ├── WindowMaximizeNode
│   └── WindowRestoreNode
│
├── App Nodes
│   ├── AppOpenNode
│   └── AppCloseNode
│
└── Screen Nodes
    └── ScreenCaptureNode
```

### The Visitor Pattern
Every `ASTNode` subclass implements:
```cpp
void accept(ASTVisitor& visitor) override {
    visitor.visit(*this);
}
```

The `ASTVisitor` interface declares a `visit()` overload for every node type. This allows multiple passes (Semantic Checker pass, Interpreter pass) to traverse the same AST without modifying the node code.

---

## Phase 3: Semantic Analysis (Checker)

### Responsibility
Validate the AST for semantic correctness **before execution**.

### What It Checks
1. **Variable declaration before use** — Accessing `x` before `let x = ...` is a semantic error.
2. **Function signatures** — Calling `foo(a, b)` when `fn foo(a)` is declared is an error.
3. **Type mismatches** — Assigning a `POINT` to a field expecting an `INT` (future).
4. **Return statement presence** in non-void functions.

### Scope System
The semantic checker maintains a **scope stack** (a vector of hash maps):
- When entering a block `{`, push a new scope
- When exiting `}`, pop the scope
- Variable lookups traverse from the innermost scope outward

---

## Phase 4: Execution (Virtual Machine)

### Responsibility
The execution phase is now split into two sub-phases: **Compilation** and **Execution**.

1. **Bytecode Compilation**: The `Compiler` (a visitor) traverses the validated AST and generates a linear stream of instructions (Bytecode). This eliminates the overhead of recursive tree-traversal during runtime.
2. **VM Execution**: The `VM` executes the generated bytecode using a high-performance stack-based architecture.

### Instruction Set (OpCodes)
Synapse uses a custom instruction set optimized for automation:
- **`OP_CONSTANT`**: Load a literal onto the stack.
- **`OP_GET_LOCAL` / `OP_SET_LOCAL`**: Fast, index-based variable access.
- **`OP_GET_GLOBAL` / `OP_SET_GLOBAL`**: Indexed access to global variables.
- **`OP_ADD` / `OP_SUBTRACT` / ...**: Arithmetic and logical operations.
- **`OP_JUMP` / `OP_JUMP_IF_FALSE` / `OP_LOOP`**: Control flow instructions.
- **`OP_CALL`**: High-performance function calling.
- **`OP_MOUSE_MOVE` / `OP_KEY_PRESS` / ...**: Native automation instructions.

### Value System
The VM uses an optimized `SynapseValue` type, which is a `std::variant` containing primitives or a `shared_ptr` to heap-allocated objects (`Obj`). This ensures efficient passing by value for primitives and cheap reference-counting for complex types like Lists and Maps.

---

## Phase 5: Telemetry & Profiling

### Responsibility
Provide deep visibility into the VM's internal behavior and performance characteristics without impacting production performance.

### Implementation
The profiler is implemented using conditional compilation (`SYNAPSE_PROFILER`). When enabled, the VM collects:
- **Allocation Statistics**: Every heap-allocated `Obj` incrementing a global counter.
- **Stack Depth Monitoring**: Tracking the maximum extent of the value stack to prevent overflows.
- **Opcode Frequency Profiling**: An array of counters (one per opcode) to identify the "hot" instructions in any given script.

### Using the Profiler
```bash
./compile.sh --profile
SYNAPSE_PROFILE=1 ./synapse run script.syn --vm
```
The results are dumped to `stderr` upon execution completion, providing a clear map of microarchitectural behavior.

### Execution Flow for a Mouse Command
For `MOUSE CLICK LEFT AT (100, 200) TIMES 2;`:
1. Parser creates a `MouseClickNode` with `button=LEFT`, `pos=(100,200)`, `times=2`
2. Interpreter's `visit(MouseClickNode& node)` is called
3. It evaluates the position and times from the node
4. It calls `Platform::mouseClick(LEFT, 100, 200)` twice in a loop via the OAL

---

## OS Abstraction Layer (OAL)

### Responsibility
Provide a single C++ interface for all OS interactions, with separate implementations per platform.

### Interface (platform.h)
```cpp
namespace Synapse::Platform {
    void mouseMove(int x, int y);
    void mouseClick(MouseButton button, int x, int y);
    void mouseDrag(int x1, int y1, int x2, int y2);
    void mouseScroll(ScrollDirection dir, int amount);
    void mouseHold(MouseButton button);
    void mouseRelease(MouseButton button);

    void keyPress(const std::string& key);
    void keyHold(const std::string& key);
    void keyRelease(const std::string& key);
    void keyType(const std::string& text);

    void windowOpen(const std::string& name);
    void windowClose(const std::string& name);
    void windowFocus(const std::string& name);
    void windowMove(const std::string& name, int x, int y);
    void windowResize(const std::string& name, int w, int h);
    void windowMinimize(const std::string& name);
    void windowMaximize(const std::string& name);

    void screenCapture(const std::string& path, int x1, int y1, int x2, int y2);
    bool windowExists(const std::string& name);
}
```

### Linux Implementation
Uses **libX11** (X11) for mouse/keyboard control and **XLib** for window management.

```cpp
// x11_platform.cpp
void mouseMove(int x, int y) {
    Display* display = XOpenDisplay(nullptr);
    XWarpPointer(display, None, DefaultRootWindow(display), 0,0,0,0, x, y);
    XFlush(display);
    XCloseDisplay(display);
}
```

### Windows Implementation
Uses **Win32 API** (`windows.h`) for all OS operations.

```cpp
// win32_platform.cpp
void mouseMove(int x, int y) {
    SetCursorPos(x, y);
}
```

### Platform Selection (CMake)
Platform implementation is selected at **compile-time**:
```cmake
if (UNIX AND NOT APPLE)
    target_sources(synapse PRIVATE src/platform/linux/x11_platform.cpp)
    target_link_libraries(synapse X11)
elseif (WIN32)
    target_sources(synapse PRIVATE src/platform/windows/win32_platform.cpp)
    target_link_libraries(synapse user32)
endif()
```

---

## Error Handling Strategy

All errors propagate as C++ exceptions:

| Exception Class    | Thrown By         | Description                        |
|--------------------|-------------------|------------------------------------|
| `LexerError`       | Lexer             | Unknown character, unterminated string |
| `ParseError`       | Parser            | Unexpected token, missing semicolon |
| `SemanticError`    | SemanticChecker   | Undeclared variable, type mismatch |
| `RuntimeError`     | Interpreter       | Division by zero, undefined behavior |
| `PlatformError`    | OAL               | Window not found, OS call failure   |

The CLI entry point (`main.cpp`) catches all exceptions and formats them for user display:
```
[SyntaxError] line 5, col 12: Expected ';' but found 'MOUSE'
[RuntimeError] line 9: Undefined variable 'count'
```
