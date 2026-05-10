# Synapse — Core Design Architecture

> Version: 0.1.0 (Prototype)  
> Status: Active Design  
> Last Updated: 2026-05-09

---

## 1. Project Summary

**Synapse** is a high-performance, cross-platform automation language and interpreter built in C++. It allows users to write human-readable scripts using a strict `<ACTION> <TARGET> [ATTRIBUTES];` grammar to control and automate any aspect of their desktop environment — including mouse control, keyboard simulation, window management, file interaction, and more.

Synapse is positioned as a developer-grade alternative to tools like AutoHotkey, offering:
- A formally defined, rigid grammar (no ambiguity)
- A compiled C++ interpreter for maximum speed
- Cross-platform support (Linux and Windows)
- An Object-Oriented AST architecture that enables future extensibility
- A clear roadmap toward advanced computer vision (CV) and UI-tree detection

---

## 2. Problem Statement

Existing automation tools have fundamental limitations:

| Tool        | Problem                                                             |
|-------------|----------------------------------------------------------------------|
| AutoHotkey  | Windows-only, loose syntax, poor extensibility                       |
| xdotool     | Linux-only, no real scripting language, CLI flags only              |
| PyAutoGUI   | Slow (Python), limited scripting, no formal grammar                 |
| Sikuli      | Heavyweight, Java-based, relies on image matching only              |

**Synapse** fills the gap by providing a formal language with:
1. A rigid, parser-friendly grammar
2. A native C++ engine for low-latency execution
3. Cross-platform abstractions using native OS APIs
4. A future pathway to AI-assisted and CV-assisted UI detection

---

## 3. Goals & Non-Goals

### Goals (Current Scope)
- Define a rigid, formal grammar for an automation scripting language
- Implement a C++ Lexer, Parser (AST), and Interpreter
- Support: variables, control flow, functions, mouse, keyboard, window commands
- Run as a standalone CLI tool: `synapse run script.syn`
- Cross-platform on Linux (X11/Wayland) and Windows (Win32)

### Non-Goals (Out of Scope for v0.x)
- GUI editor or IDE plugin
- Computer Vision / AI-assisted UI element detection *(planned for v2.x)*
- Network communication or remote control
- macOS support
- JIT compilation

---

## 4. System Architecture Overview

```
┌─────────────────────────────────────────────────┐
│                  CLI Entry Point                │
│               synapse run <file.syn>            │
└──────────────────────┬──────────────────────────┘
                       │
             ┌─────────▼─────────┐
             │     Source File   │
             │    (.syn text)    │
             └─────────┬─────────┘
                       │
             ┌─────────▼─────────┐
             │      LEXER        │  Phase 1: Tokenization
             │  Character → Token│
             └─────────┬─────────┘
                       │
             ┌─────────▼─────────┐
             │      PARSER       │  Phase 2: Syntax Analysis
             │  Token → AST Node │
             └─────────┬─────────┘
                       │
             ┌─────────▼─────────┐
             │  SEMANTIC CHECKER │  Phase 3: Type & Scope Validation
             │  AST validation   │
             └─────────┬─────────┘
                       │
             ┌─────────▼─────────┐
             │   EXECUTION       │  Phase 4: Bytecode VM (Current Primary)
             │   Bytecode/JIT    │  Phase 5: JIT Optimization (Planned)
             │                   │  Fallback: Tree-walking Interpreter
             └─────────┬─────────┘
                       │
          ┌────────────▼────────────┐
          │   OS Abstraction Layer  │  Platform-Agnostic Bridge
          ├─────────────────────────┤
          │  Linux (X11 / Wayland)  │
          │  Windows (Win32 API)    │
          └─────────────────────────┘
```

---

## 5. Component Responsibilities

### 5.1 Lexer
- Reads the raw `.syn` source code character by character
- Produces a flat list of categorized `Token` objects
- Handles: keywords, identifiers, string literals, numeric literals, operators, punctuation
- Emits error with precise `(line, column)` information on invalid characters

### 5.2 Parser
- Consumes the token stream from the Lexer
- Validates the token sequence against the formal grammar
- Generates a typed, hierarchical Abstract Syntax Tree (AST)
- Each AST node maps directly to a natural-language command category
- Uses a **Recursive Descent** parsing strategy

### 5.3 Semantic Checker
- Traverses the AST after parsing
- Validates variable declarations before use
- Resolves function signatures and argument types
- Reports semantic errors (undeclared variables, type mismatches)

### 5.4 Interpreter (Virtual Machine)
- **Current (v0.x)**: Traverses the validated AST using the **Visitor Pattern**. Executes each node by delegating to the appropriate OS Abstraction Layer call. Maintains an execution `Environment` (variable scopes, call stack) using chained hashmaps.
- **Future (v0.8+)**: A specialized **Bytecode Compiler** will transform the AST into a linear stream of opcodes. A high-performance **Stack-based VM** will then execute these opcodes, utilizing indexed variable slots instead of string lookups for maximum execution speed.

### 5.5 OS Abstraction Layer (OAL)
- Provides a platform-neutral C++ interface for all OS interactions
- Each capability is behind a common interface with separate platform implementations
- Each capability is behind a common interface with separate platform implementations
- Platform resolved at **runtime** via factory method (`IPlatform::create()`)

---

## 6. Architectural Pillars

To ensure long-term scalability and security, Synapse is built upon three foundational pillars:

### 6.1 Memory Management (Hybrid Strategy)
Synapse employs a hybrid approach to balance performance with ease of use:
- **Small Values**: Primitive types (Int, Float, Bool) are stored inline within the stack or registers.
- **Small String Optimization (SSO)**: Strings up to 15 bytes are stored inline within the `ObjString` header, eliminating heap allocations for short-lived transients and keys.
- **Buffer Ownership**: `ObjString` supports taking ownership of existing `char*` buffers, avoiding redundant copies during concatenation and coercion.
- **Immortal Constants**: Literal strings are globally interned during compilation and marked as immortal (`refCount = -1`), bypassing runtime reference counting.
- **Short-lived Objects**: An **Arena Allocator** is used for temporary AST nodes and intermediate runtime values to minimize heap fragmentation.
- **Long-lived Collections**: Optimized **Intrusive Reference Counting** handles recursive collections (Lists, Maps, Tuples) and closures.

### 6.2 String Subsystem Optimizations
To handle high-volume automation workloads (e.g., repeated command generation), the string subsystem includes specialized optimizations:
- **Concatenation Result Cache**: A thread-safe cache in the VM that memoizes string combinations. Repeatedly concatenating the same pointers (hot in loops) returns a cached reference in $O(1)$, bypassing allocation and hashing.
- **In-place Append**: Internal mutation is enabled for transient strings (`refCount == 1`). This transforms $O(n^2)$ building patterns into linear $O(n)$ operations.
- **Fast-Path Equality**: `OP_EQUAL` for strings uses pointer equality and interning uniqueness guarantees for immediate early-outs.
- **Specialized Opcodes**: `OP_STRING_ADD` and `OP_STRING_EQUAL` fast-path common operations by bypassing generic arithmetic/equality dispatch.

### 6.3 Security & Sandboxing (Capabilities Model)
As an automation engine with hardware access, Synapse implements a strict security model:
- **API-Level Capabilities**: OS interactions (Mouse, Keyboard, Screen) are restricted behind granular permissions.
- **Sandboxed Execution**: External modules and untrusted scripts run in a restricted environment with no access to sensitive platform APIs unless explicitly granted via a manifest.
- **Trusted Module ABI**: Only signed, verified official modules have direct access to the native runtime bridge.

### 6.3 Async-Native Runtime
Automation is fundamentally event-driven. The Synapse runtime is built around a non-blocking **Core Event Loop** that handles system triggers, timeouts, and concurrent automation tasks without blocking the main execution thread.

---

## 7. Data Types & Collections

Synapse supports both primitive and recursive collection types:

| Type | Syntax | Description |
|---|---|---|
| **INT** | `42` | 64-bit integer |
| **FLOAT** | `3.14` | Double-precision floating point |
| **STR** | `"Hello"` | UTF-8 encoded string |
| **BOOL** | `true` | Boolean flag |
| **TIME** | `500ms`, `2s` | Native duration type |
| **POINT** | `(x, y)` | Cartesian coordinate pair |
| **LIST** | `[1, 2, 3]` | Dynamic, recursive array |
| **MAP** | `{"key": "val"}` | String-keyed dictionary |
| **NULL** | `null` | Representing absence of value |

---

## 7. Design Decisions Log

| Decision | Alternatives Considered | Reason Chosen |
|---|---|---|
| **C++ as interpreter language** | Python, Go, Rust | Maximum performance, native OS access, no runtime dependency |
| **Recursive Descent Parser** | LALR (Bison/YACC), PEG parsers | Easier to extend, excellent error messages, full control over grammar |
| **Visitor Pattern for AST** | Direct if/else dispatch | Decouples tree traversal from node structure; enables multiple passes (semantic check, interpreter, future compiler) |
| **Native OS APIs (Win32/X11)** | Qt, SDL, libxdo | Avoids heavy dependencies, keeps binary small, Qt is not designed for controlling third-party apps |
| **`.syn` file extension** | `.csc`, `.pilot`, `.auto` | Reflects the Synapse brand identity, short and memorable |
| **CLI-first design** | REPL, GUI, VS Code extension | Fastest path to a usable prototype; CLI-first is also the most scriptable and automatable |
| **Retired `SynapsePoint` type** | Keep as specialized primitive | Any 2-element Tuple or List can serve as a coordinate; YAGNI avoids a redundant type |
| **`TUPLE` syntax: `(a, b, ...)`** | `<a, b>`, `@(a, b)`, `point(a, b)` | Python-style parenthesis is most familiar; trailing-comma disambiguates grouping from single-element tuples |
| **`SynapseTuple` after `SynapseValue`** | Inline buffer (SOO) before variant | C++ forward-declaration allows recursive definition without circular deps; SOO can be added later |

---

## 7. File & Folder Structure

```
CommandPro/                    ← Repository root (will be renamed to Synapse)
├── CMakeLists.txt             ← C++ Build configuration
├── README.md                  ← Quick start and overview
├── .gitignore
│
├── include/                   ← C++ Public Header files
│   ├── lexer/
│   │   ├── token.h            ← TokenType enum and Token struct
│   │   └── lexer.h            ← Lexer class declaration
│   ├── parser/
│   │   ├── ast.h              ← All AST node definitions + ASTVisitor interface
│   │   └── parser.h           ← Parser class declaration
│   ├── semantic/
│   │   └── checker.h          ← SemanticChecker class declaration
│   ├── interpreter/
│   │   └── interpreter.h      ← Interpreter class declaration
│   └── platform/
│       ├── platform.h         ← Common OAL interface
│       ├── linux/
│       │   └── x11_platform.h ← Linux X11 implementation header
│       └── windows/
│           └── win32_platform.h ← Windows Win32 implementation header
│
├── src/                       ← C++ Source files
│   ├── lexer/
│   │   └── lexer.cpp
│   ├── parser/
│   │   ├── ast.cpp
│   │   └── parser.cpp
│   ├── semantic/
│   │   └── checker.cpp
│   ├── interpreter/
│   │   └── interpreter.cpp
│   ├── platform/
│   │   ├── linux/
│   │   │   └── x11_platform.cpp
│   │   └── windows/
│   │       └── win32_platform.cpp
│   └── main.cpp               ← CLI Entry point
│
├── docs/                      ← Documentation
│   ├── DESIGN.md              ← THIS FILE
│   ├── LANGUAGE_SPEC.md       ← Formal grammar and syntax specification
│   ├── SYNTAX.md              ← User-facing syntax quick reference
│   ├── COMPILER_INTERNALS.md  ← Lexer, Parser, and AST deep-dive
│   ├── STDLIB.md              ← Built-in commands and standard library reference
│   ├── CROSS_PLATFORM.md      ← Platform support and build instructions
│   └── ROADMAP.md             ← Version milestones and future vision
│
├── lang-test/                 ← Legacy syntax exploration (reference only)
├── examples/                  ← Example .syn scripts
└── tests/                     ← Unit and integration tests
```
