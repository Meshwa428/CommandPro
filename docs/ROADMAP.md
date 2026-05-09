# Synapse Project Roadmap

> This document outlines the planned milestones and feature roadmap for Synapse. Features are organized by version and priority.

---

## Current Status: v0.1.0 — Foundation

The initial skeleton of the C++ Lexer, Parser, and Interpreter has been created. Simple variable declarations and print statements execute successfully.

---

## v0.2.0 — Core Language Completeness

**Goal:** Make the language fully functional as a general scripting language before adding automation.

**Planned Features:**
- [x] Full operator support (arithmetic, logical, comparison, bitwise)
- [x] Full `if / else` conditional support
- [x] `repeat N times` loop
- [x] `loop while (condition)` loop
- [x] User-defined functions (`fn`) with arguments and return values
- [x] Variable scoping (nested blocks have their own scope)
- [x] `print` and `println` with string concatenation
- [x] `ASK ... INTO var AS TYPE` for user input
- [x] Type casting (`INT "42"`, `STR true`, `BOOL 0`)
- [x] Time literal parsing (`500ms`, `2s`, `5m`, `1.5h`)
- [x] `WAIT` command
- [x] Proper error reporting with line and column numbers

---

## v0.3.0 — Automation Layer (Mouse & Keyboard)

**Goal:** Expose the OS Abstraction Layer for mouse and keyboard control.

**Planned Features:**
- [x] OS Abstraction Layer interface (`platform.h`)
- [x] Linux X11 implementation:
  - [x] Mouse move, click, drag, scroll, hold, release
  - [x] Key press, hold, release, type
- [ ] Windows Win32 implementation:
  - [ ] Mouse move, click, drag, scroll, hold, release
  - [ ] Key press, hold, release, type
- [x] Keyboard shortcut combos (`KEY PRESS (CTRL + C)`)
- [x] `MOUSE CLICK LEFT AT (x, y) TIMES n`
- [x] All mouse and keyboard AST nodes wired to OAL
- [x] **New**: Factual verification & Integrity checks (Wait for Arrival)

---

## v0.4.0 — Window Management & App Control

**Goal:** Allow full control of application windows.

**Planned Features:**
- [x] `WINDOW OPEN / CLOSE / FOCUS / MOVE / RESIZE` 
- [x] `WINDOW MINIMIZE / MAXIMIZE / RESTORE`
- [x] `APP OPEN / CLOSE / LIST`
- [x] **Advanced Data Types**:
  - [x] `LIST` (Dynamic arrays)
  - [x] `MAP` (Key-value dictionaries)
  - [ ] `TUPLE` (Immutable collections - *planned for v0.8.x*)
  - [ ] `SET` (Unique collections - *planned for v0.8.x*)
- [ ] `WINDOW "<name>" EXISTS` condition
- [ ] `SCREEN CAPTURE INTO "<path>"` (partial and full)
- [ ] CMake cross-platform library detection

---

## v0.5.0 — Async & Event Loop Architecture

**Goal:** Transition from synchronous execution to an event-driven runtime suitable for complex automation.

**Planned Features:**
- [ ] **Core Event Loop**: Implementation of the primary polling/dispatch loop for system events.
- [ ] **Async / Await Infrastructure**: Language-level support for non-blocking operations (`WAIT UNTIL`, `ASYNC FN`).
- [ ] **Timer & Job Scheduler**: Refactor intervals and scheduled tasks to run atop the event loop.
- [ ] **Event Hooks**: Syntax and runtime support for system-level triggers (e.g., `ON WINDOW OPEN`).

---

## v0.6.0 — Error Handling & Robustness

**Goal:** Make scripts resilient to unexpected conditions and formalize the language structure.

**Planned Features:**
- [x] `try { ... } catch (err) { ... }` blocks
- [ ] **Formal Grammar Specification**: Publish an EBNF grammar to eliminate parsing ambiguity and prevent syntax entropy.
- [ ] Semantic Analyzer (Phase 3) — full variable scope and type checking
- [x] User-friendly error messages with source context (underline the bad token)
- [ ] `--dry-run` CLI flag (validate without execution)
- [x] `--verbose` CLI flag (show AST dump and execution trace)
- [x] Case-insensitive keywords for natural scripting

---

## v0.7.0 — Wayland Support (Linux)

**Goal:** Extend Linux support to modern Wayland-native sessions.

**Planned Features:**
- [x] `libwayland-client` / `uinput` integration
- [x] Virtual pointer protocol support
- [x] Virtual keyboard protocol support
- [x] Auto-detect X11 vs Wayland at runtime and pick correct backend

---

## v0.8.0 — Performance & VM Engineering

**Goal:** Transition from a naive tree-walking interpreter to a high-performance Bytecode VM.

**Planned Features:**
- [ ] **Variable Slot Compilation**: Replace string-based `unordered_map` lookups in the execution loop with zero-cost index-based slots.
- [ ] **Flattened Scope Frames**: Replace recursive `Environment` pointers with a flat stack of activation records (frames) to eliminate deep scope chain traversal.
- [ ] **Bytecode Compiler**: Implement a compilation pass that transforms the AST into a linear stream of opcodes.
- [ ] **Stack-based Virtual Machine**: Replace `ASTVisitor::visit` recursion with a high-speed dispatch loop (Computed Gotos / Opcode Switching).
- [ ] **Memory Management Strategy**: Implement a dedicated Garbage Collection (GC) or an optimized Arena + Reference Counting hybrid to handle closures and async tasks safely.
- [ ] **Optimized Value System**: Investigate Tagged Unions or NaN-boxing to reduce `std::variant` and `std::shared_ptr` overhead.
- [ ] **Constant Folding & Interning**: Pre-calculate static expressions and intern all strings to minimize runtime allocations.
- [ ] **Closure & Upvalue Support**: Implement proper lexical closures for callbacks and async tasks.

---

## v0.9.0 — Ecosystem & Extensibility

**Goal:** Enable modularity and ensure secure execution through a formal plugin and sandbox architecture.

**Planned Features:**
- [ ] **Sandboxing & Permission Model**: Implement a capabilities-based security model to restrict script access to sensitive OS APIs (e.g., `PERMISSION_INPUT`, `PERMISSION_SCREEN_CAPTURE`).
- [ ] **C-Plugin ABI**: Formal interface for loading external shared libraries (.so / .dll) as Synapse modules.
- [ ] **Module System**: `import` syntax and namespace management to decouple automation logic from language core.
- [ ] **Package Manager (synpkg)**: Initial tool for distributing and installing Synapse runtime modules.
- [ ] **Standard Library Decoupling**: Move non-essential features (OCR, Browser, AI) into official, independently versioned modules.

---

## v1.0.0 — Stable Release

**Goal:** Stable, production-quality release with full documentation and test coverage.

**Planned Features:**
- [ ] 90%+ unit test coverage for Lexer and Parser
- [ ] Integration tests for all STDLIB commands
- [ ] Full documentation (LANGUAGE_SPEC, SYNTAX, STDLIB, INTERNALS, CROSS_PLATFORM)
- [ ] Pre-built binary releases for Linux (x86_64) and Windows (x64)
- [ ] GitHub Actions CI/CD pipeline (build + test on Linux and Windows)

---

## v2.0.0 — Semantic Automation *(Official Modules)*

> This is the major advanced feature planned for Synapse beyond v1.0. These features are implemented as **decoupled runtime modules**, not core language syntax.

**Goal:** Enable scripts to find and interact with UI elements using high-level semantic queries.

### Planned Official Modules

#### 2.1 `ai.vision` (Template & AI Matching)
Find UI elements by image matching or semantic description:
```sql
import ai.vision;

# Find a button on screen by image
let btnPos = vision.find_image("assets/submit.png");
MOUSE CLICK LEFT AT btnPos;

# AI-assisted semantic finding
let loginForm = vision.find_element("the login form");
```

#### 2.2 `ui.accessibility` (OS UI APIs)
Query the operating system's accessibility tree:
```sql
import ui.accessibility;

# Click a button by its accessible name
accessibility.click_button("Submit");
accessibility.type_into("Username", "john.doe");
```

#### 2.3 `ui.ocr` (Text Detection)
Use OCR to find elements based on visible text:
```sql
import ui.ocr;

let okBtn = ocr.find_text("OK");
MOUSE CLICK LEFT AT okBtn;
```
