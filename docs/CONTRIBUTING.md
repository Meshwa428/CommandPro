# Contributing to Synapse

> Welcome! We are glad you are interested in contributing. Please read this document fully before opening a PR.

---

## Code Style

- **Language:** C++17 strictly. No extensions beyond the standard.
- **Naming:**  
  - Classes: `PascalCase` (e.g., `LexerError`, `MouseClickNode`)
  - Methods: `camelCase` (e.g., `tokenize()`, `parsePrintStatement()`)
  - Constants / Enum values: `UPPER_CASE`
  - Files: `snake_case.cpp` / `snake_case.h`
- **Namespace:** All Synapse code lives inside `namespace Synapse { }`.
- **Includes:** Use `#pragma once` for all headers.
- **Smart Pointers:** Prefer `std::unique_ptr` for AST nodes. No raw `new/delete`.

---

## Project Layout

See [DESIGN.md](./DESIGN.md) for the full directory structure.

---

## How to Add a New Command

Example: Adding `SCREEN FLASH` command.

### Step 1: Add a Token (if new keyword)
In `include/lexer/token.h`:
```cpp
enum class TokenType {
    ...
    FLASH,    // new
};
```
In `src/lexer/lexer.cpp`, add to KEYWORDS map:
```cpp
{"FLASH", TokenType::FLASH},
```

### Step 2: Add an AST Node
In `include/parser/ast.h`:
```cpp
class ScreenFlashNode : public ASTNode {
public:
    int durationMs;
    ScreenFlashNode(int dur) : durationMs(dur) {}
    void accept(ASTVisitor& visitor) override;
};
```
Add `visit(ScreenFlashNode&)` to the `ASTVisitor` interface.

### Step 3: Parse the Command
In `src/parser/parser.cpp`, add a branch in `parseStatement()`:
```cpp
if (currentToken.type == TokenType::SCREEN) {
    // look ahead to FLASH
    ...
    return parseScreenCommand();
}
```

### Step 4: Implement in Interpreter
In `src/interpreter/interpreter.cpp`:
```cpp
void Interpreter::visit(ScreenFlashNode& node) {
    Platform::screenFlash(node.durationMs);
}
```

### Step 5: Implement in OS Abstraction Layer
Add to `include/platform/platform.h`:
```cpp
void screenFlash(int durationMs);
```
Add platform implementations in `src/platform/linux/` and `src/platform/windows/`.

### Step 6: Write Tests
Add test cases in `tests/`.

---

## Running Tests
```bash
cd build
cmake ..
make
ctest
```

---

## Commit Message Convention
```
<type>: <short description>

Types: feat, fix, docs, refactor, test, chore
```
Examples:
```
feat: add WINDOW RESIZE command
fix: handle unterminated string in lexer
docs: update STDLIB reference
```

---

## Branching Strategy
- `main` — stable releases only
- `dev` — active development
- `feature/<name>` — feature branches off `dev`
