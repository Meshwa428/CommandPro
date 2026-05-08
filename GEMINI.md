# Synapse Language Project Context Handoff

Welcome to the Synapse Language project! This document serves as an exhaustive state, context, and technical handoff for the incoming AI coding agent. It contains the deep architectural details, implementation patterns, and exact current state needed to immediately resume feature development without breaking existing systems.

---

## 1. Project Overview & Philosophy
**Synapse** is a custom compiled/interpreted automation scripting language designed specifically for programmatic OS control (Mouse/Keyboard) with built-in primitives and explicit data types. 
- **Tech Stack**: C++17 (Modern C++, heavily utilizing `std::variant`, `std::shared_ptr`, and smart pointers).
- **Build System**: CMake 3.10+
- **Platform Dependencies**: Currently requires `libX11` and `libXtst` on Linux for automation.
- **Target OS**: Linux (Primary dev target via X11/uinput), Windows (Planned Architecture `Win32Platform`).
- **Core Engine**: A custom Lexer, recursive-descent Parser (Operator Precedence Climbing), AST (Abstract Syntax Tree), and a tree-walking Interpreter.

---

## 2. Deep System Architecture

### A. The Lexer (`src/lexer/`, `include/lexer/`)
The Lexer tokenizes raw script input. 
- **Design**: One-pass scanner producing a `std::vector<Token>`. 
- **Case-Insensitive**: Keywords are matched case-insensitively using an internal `std::unordered_map` that converts read tokens to uppercase before matching against the `KEYWORDS` dictionary.
- **Tokens**: Supports complex token groups such as `Core Keywords` (`let`, `fn`, `return`, `if`, `while`), `Type Keywords` (`int`, `str`, `tuple`, etc.), and `Automation Keywords` (`mouse`, `key`, `move`, `click`). 

### B. The Parser (`src/parser/`, `include/parser/`)
The Parser converts tokens into a tree of `ASTNode` objects.
- **Structure**: Recursive-descent parsing for statements and control flow.
- **Expressions**: Uses **Operator Precedence Climbing** (Pratt parsing variant) to handle complex mathematical and logical expressions without deep recursive calls.
- **AST Nodes**: Fully polymorphic. Base `ASTNode` has an `accept(ASTVisitor&)` method for double-dispatch visitor patterns. Nodes manage memory safely via `std::unique_ptr<ASTNode>`.
- **Disambiguation**: The parser intelligently distinguishes between grouping expressions `(1 + 2)` and tuple literals `(1, 2)` or `(1,)` natively within `parsePrimary`.

### C. The Runtime Interpreter (`src/interpreter/`, `include/interpreter/`)
The Interpreter executes the AST by visiting nodes.
- **Value System**: All language values are stored in a `SynapseValue` alias, which is a `std::variant` containing: `long long`, `double`, `std::string`, `bool`, `SynapseNull`, `SynapseTime`, and `std::shared_ptr` to collections (`SynapseTuple`, `SynapseList`, `SynapseMap`).
- **Memory Management**: Primitive types are stored inline in the variant. Collections are heap-allocated via `std::shared_ptr` to allow passing by reference cheaply in the scripts without heavy deep copies.
- **Environment Scope**: Managed by the `Environment` class, which holds a scoped mapping of variables and their specific `Type Constraints`. Scope chaining is implemented via a `parent` pointer.

### D. The OS Abstraction Layer (OAL) (`src/platform/`, `include/platform/`)
The OAL decouples language logic from physical hardware APIs.
- **Interface**: The `IPlatform` interface mandates methods like `mouseMove(x, y)`, `mouseClick(button)`, `keyPress(keysym)`, and `keyType(text)`.
- **LinuxX11Platform**: The active backend. Uses `XTestFakeMotionEvent` and `XTestFakeButtonEvent` to manipulate the hardware cursor, and `XStringToKeysym`/`XTestFakeKeyEvent` to simulate keystrokes natively at the OS level.
- **MockPlatform**: A headless mock platform used purely for validation and unit testing. Instead of moving the real mouse, it intercepts OAL calls and logs them to stdout (`[MOCK] MOUSE_MOVE 100 100`), which Pytest reads to verify behavior.

---

## 3. Current State & Recent Accomplishments (Phase 4.2)
We have just completed **Phase 4.2**, heavily refactoring the type system for speed and precision.

### 3.1. Tuple Implementation & Point Retirement
- **The Issue**: Originally, the language had a rigid `SynapsePoint` datatype exclusively for automation. 
- **The Fix**: `SynapsePoint` was completely retired. We implemented a native, immutable `SynapseTuple` using Python-like syntax `(x, y)`. 
- **Dynamic Coordinates**: Automation commands (`MOUSE MOVE TO (100, 200)`) now dynamically accept *any* 2-element iterable (Lists or Tuples). The Interpreter uses a specialized helper `extractCoord()` to unpack `SynapseValue` variants into raw C++ integers before passing them to the OAL.

### 3.2. Typed Variable Declarations & Type Enforcement
- **The Issue**: Using the `let` keyword for all variables meant the interpreter had to continuously identify dynamic types at runtime.
- **The Fix**: We implemented explicit datatype mapping (`int`, `float`, `str`, `bool`, `tuple`, `list`, `map`, `time`).
- **Parser**: Handled specifically in `parseTypedVarDecl()`.
- **Enforcement**: The `Environment` class was upgraded to maintain a shadow `types` map alongside the `vars` map.
- **Coercion**: A new `coerceToType()` method automatically boxes primitive variations (e.g. initializing an `int` with `10.5` coerces it to `10`) or throws a hard `RuntimeError` on complete mismatches (e.g. assigning `"hello"` to a `float`), preventing subtle runtime bugs and improving execution predictability.

---

## 4. Current File Structure Highlights
- `CMakeLists.txt`: Root build instructions.
- `src/main.cpp`: CLI entry point. Parses flags (`--mock`), runs the lexer, parser, and interpreter.
- `src/lexer/lexer.cpp`: Contains the scanning loops and keyword mapping logic.
- `src/parser/parser.cpp`: Contains the Pratt expression parser and statement dispatching.
- `include/parser/ast.h`: Defines the `ASTVisitor` and every single node in the tree.
- `src/interpreter/interpreter.cpp`: The core execution loop. Contains the massive `ASTVisitor` implementation and the `coerceToType` type-checking logic.
- `src/platform/linux_x11.cpp`: The X11 hardware backend.
- `examples/`: Reference scripts (`typed_vars.syn`, `mouse_demo.syn`, `tuple_demo.syn`, `keyboard_demo.syn`).
- `tests/`: End-to-end Python test suite that asserts standard output and `MockPlatform` logs.
- `docs/`: 
  - `LANGUAGE_SPEC.md`: Detailed language grammar.
  - `DESIGN.md`: Architecture log and historical design decisions.
  - `SYNTAX.md`: Quick reference guide.

---

## 5. Roadmap & Immediate Next Steps

You are stepping into the project as we prepare for **Phase 3 and beyond**.

1. **Phase 3: Image Recognition & UI Detection**
   - **Goal**: Implement advanced screen detection algorithms to allow Synapse to "see" UI elements, returning coordinates (Tuples) for the mouse to interact with.
   - **Need**: Integration with OpenCV or a similar lightweight pixel-matching library.

2. **Windows Platform Support**
   - **Goal**: Implement `src/platform/win32_platform.cpp`.
   - **Need**: Create a class inheriting from `IPlatform` that utilizes `SendInput` or similar Win32 APIs to match the Linux automation parity.

3. **Enhanced Keyboard Mapping**
   - **Goal**: Support advanced chorded keystrokes (`Ctrl + C`, `Alt + Tab`).
   - **Need**: Updates to the `KEY PRESS` AST structure and the underlying OAL implementations to support modifiers holding state.

---

*Note for the incoming agent: You can compile the project using `cd build && cmake .. && make` and run scripts using `./synapse run ../examples/script.syn` or unit tests via `cd tests && pytest`.*
