# Synapse v2 — Project Handoff Document

This document transfers 100% of the project context, design alignment, spec decisions, and current implementation details to the incoming developer agent.

---

## 1. Project Overview & Core Philosophy

**Synapse v2** is a fast, lightweight, register-based runtime environment and programming language designed specifically for low-latency desktop automation.

### The "Caveman Efficiency" Principle
The target author for Synapse scripts is an LLM. Since every word costs input/output tokens (BPE), the syntax is optimized to minimize BPE token footprint while keeping it readable for humans.
- **Goal:** Strip 80% of standard programming filler words.
- **Rule:** Lowercase everything, no brackets/parens around conditions, space-delimited coordinates.
- **Ratio:** Yields a 2–3× token cost reduction compared to Python + PyAutoGUI.

---

## 2. Document Index (Single Source of Truth)

All active specs and designs are committed inside the repository:

1. **Language Syntax Reference:** [docs/design/001-language.md](file:///home/meshwa/Documents/Projects/CommandPro/docs/design/001-language.md)
   - Disambiguation rules, verb mappings, desugaring rules.
2. **System Architecture:** [docs/design/002-architecture.md](file:///home/meshwa/Documents/Projects/CommandPro/docs/design/002-architecture.md)
   - Pipeline stages, VM execution loop design, register machine choices.
3. **RAT Model Architecture:** [docs/design/005-rat-model.md](file:///home/meshwa/Documents/Projects/CommandPro/docs/design/005-rat-model.md)
   - Physics of human mouse trajectory emulation, calibration mechanisms, dataset structure.
4. **Feature Specification:** [docs/design/006-language-features.md](file:///home/meshwa/Documents/Projects/CommandPro/docs/design/006-language-features.md)
   - Master list of locked-in features (control flow, tuples, maps, lists, error handling).
5. **EBNF Grammar Spec:** [docs/spec/grammar.md](file:///home/meshwa/Documents/Projects/CommandPro/docs/spec/grammar.md)
   - Lexical & Syntactic grammar, operator precedence levels, desugaring map.
6. **Bytecode Opcode Spec:** [docs/spec/opcodes.md](file:///home/meshwa/Documents/Projects/CommandPro/docs/spec/opcodes.md)
   - 64-bit instruction formats, NaN-boxing layout, opcode table (109 opcodes).

---

## 3. High-Priority Language Decisions (Locked)

1. **Variables & Constants:**
   - `let x = 42` (mutable).
   - `const PI = 3.14` (compiler checks for reassignment).
2. **Expression Ternary:**
   - Suffix-style: `let label = "big" if score > 90 else "small"`.
3. **Map Access:**
   - Supports both `user["name"]` and dot-access `user.name`.
4. **Try/Catch/Else:**
   - Supports `else` block (runs only if the `try` block succeeds without exceptions).
5. **Named Arguments:**
   - Syntax: `connect("host", port: 3000)`.
6. **Anonymous Functions:**
   - Explicit `fn(x) { return x * 2 }` only. No shorter syntax (like `|x|`).
7. **Pipe Operator (`|>`):**
   - Chains operations: `items |> filter(fn) |> sum()`.
8. **Comprehensions & Method Chains:**
   - Both supported: `[x * 2 for x in items]` and `items.map(fn)`.
9. **Automation Syntax:**
   - Mapped directly to standard library under the hood.
   - Example: `mouse 300, 400` desugars to `mouse.move(300, 400)`.

---

## 4. Bytecode & VM Design (Locked)

### 64-bit Fixed-Width Instructions
Instead of naive 32-bit instructions which waste 32 bits on every 64-bit CPU cache fetch, Synapse uses fixed 64-bit instructions. This allows encoding large inline immediates directly:
- **`LOAD_INT r0, 1000`** -> 48-bit immediate, 0 constant pool lookups.
- **`ADDI r0, r1, 1`** -> 40-bit immediate, eliminates separate constant load.
- **`RANGE_PREP`** -> encodable in a single instruction with bounds inline.
- **Fused branch compare:** `JEQ`, `JLT`, etc. combine check + jump in one step.

### Value Representation (NaN Boxing)
Registers are quiet-NaN boxed `double` variables (8 bytes):
- Exp = `0x7FF` (signaling quiet NaN).
- Prefix = `0xFFF8..0xFFFF` (identifies tag).
- Payload = 48-bit signed integer (`int48`), 44-bit heap pointer, or boolean/none.
- Heap objects (`String`, `List`, `Map`, `Tuple`, `Closure`, `Error`) contain a common header with type tags and GC marks.

---

## 5. Late-Game Architecture Blocks (Roadmapped)

### RAT (Realistic Automated Trajectories) Mouse Model
- **Goal:** Human mouse trajectory emulation to bypass heuristics of bot detectors.
- **Mechanism:** Multi-peaked velocity profiles (sub-movements), signal-dependent noise, arm-vs-wrist sweeps, pre-movement hesitation, and terminal targeting jitter.
- **Session Security:** Process seed generated from `RDRAND` + PID + timestamp to randomize curves per session and prevent pattern fingerprinting.
- **Calibration Tool:** Native fullscreen overlay (`syn rat calibrate`) clicks dots to calculate personal Fitts' Law and cursor parameters, saved to `~/.config/synapse/rat_user.bin`.
- **Dataset:** 6,715 clean human movements extracted from AdSERP dataset in `Dataset/movements.jsonl`.

### Vision Automation Stack (Phase 6.6)
- **Layer 0:** Native Accessibility APIs (`libatspi` on Linux, UIA on Windows) - fast, 0ms.
- **Layer 1:** YOLOv8-UI (fine-tuned on 11 element classes, ~6MB, ~8ms CPU) + CRNN text recognition - local.
- **Layer 2:** CLIP semantic mapping (optional dependency) for descriptive matches like `see "the red close button"`.
- **Auto-Annotation:** Pipeline query native element trees to auto-label screenshots for training YOLO-UI.

---

## 6. Current Implementation Status

### Phase 0: Scaffold — ✅ Done
- Build structure, warnings, compilation flags, presets configuration.
- Catch2 unit test discovered runner.

### Phase 1: Lexer Core — ✅ Done & Checked
- **Source Indexing:** Handles line-column mapping and boundary spans.
- **Diagnostics:** Emits clang-style inline diagnostics with caret highlighting (`^`) and line previews.
- **Lexer & Tokens:** Hand-written, zero-copy tokenizer. Parses decimal/hex/bin/oct integers, float exponents, duration prefixes, raw strings, multiline strings, and nested string interpolation.
- **Unit Tests:** `tests/unit/test_lexer.cpp` has 6 test cases (61 assertions), all compilation and checks pass.

---

## 7. Next Actions for the Incoming Agent

You are starting from **Phase 1 (AST & Parser)**. Here is your checklist:

1. **AST Representation (`include/synapse/frontend/ast.h`):**
   - Design node structures for expressions and statements.
   - Design arena allocator (or keep it simple with `std::unique_ptr` for AST nodes).
2. **Pratt Parser (`src/frontend/parser.cpp`):**
   - Follow the grammar spec in [docs/spec/grammar.md](file:///home/meshwa/Documents/Projects/CommandPro/docs/spec/grammar.md).
   - Implement recursive descent for statements and Pratt parser for expressions.
   - Wire AST output.
3. **Resolver / Semantic Analysis (Phase 2):**
   - Scope resolution, local slots binding, upvalue capture analysis.
4. **Compile/Build verification:**
   - Always run `cmake --preset debug`, then `cmake --build --preset debug`, and verify with `./build/debug/tests/syn_tests`.
