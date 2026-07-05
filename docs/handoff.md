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

Status against the phase table in [PLAN.md](file:///home/meshwa/Documents/Projects/CommandPro/PLAN.md) §5.

- **Phase 0 — Foundations:** ✅ CMake presets (`debug`/`release`), Catch2 runner, conformance harness (`tests/run.py`, auto-builds the preset), benchmark runner.
- **Phase 1 — Calculator core:** ✅ Lexer, Pratt parser, diagnostics, 64-bit bytecode + register VM. int64/float64, strings + interpolation (incl. `{{`/`}}` escapes), booleans, `none`, `let`/`const`, arithmetic, comparison, `and/or/not`, `say`.
- **Phase 2 — Control flow:** ✅ `if/else if/else`, `while`, `repeat`, `for`, ranges, `break`/`continue`, `match`.
- **Phase 3 — Functions:** ✅ `fn`, returns, closures/upvalues, native fn interface, recursion.
- **Phase 4 — Collections:** ✅ lists, maps, tuples, indexing, slicing, methods, iteration.
- **Phase 5 — Errors & modules:** ✅ `try/catch/finally`, `throw`, error values; `use` multi-file imports.
- **Phase 6 — Automation layer:** 🔄 Closing. Command statements (`mouse`/`click`/`type`/`open`/`wait`/…) desugar to `mouse.*`/`keyboard.*`/`window.*`/`app.*` stdlib. Platform interface with **mock** (CI), real **Linux** (X11/XTest, libpng, spawn), and **Windows** stub backends. Conformance-tested via mock (`SYN_MOCK_PLATFORM`).
  - Remaining DoD: benchmark-not-regressed check + published methodology.
- **Also present (post-1.0 track):** a template JIT (`src/backend/jit.cpp`, default path; `SYN_NO_JIT=1` forces the interpreter — the correctness ground truth) and a **RAT** trajectory-model skeleton (`src/rat/`, not yet wired into the move path).

### Notable invariants / gotchas
- **`say` is the sole print verb**, space-form: `say "hi {name}"`. There is no `print`. Multi-value output uses interpolation, never extra args.
- **`tests/run.py` auto-builds** `build/<preset>/syn`. Never trust an ad-hoc `cmake --build build` — that produces `build/syn`, which the harness ignores. Verify against the preset binary.
- The JIT has silently produced wrong answers before — check correctness under `SYN_NO_JIT=1`, not just speed.

---

## 7. Next Actions for the Incoming Agent

1. **Finish Phase 6 DoD:** run the full benchmark matrix on a release build, confirm no >5% regression, record methodology.
2. **RAT calibrate loop:** wire the trajectory model into the `mouse`/`move` path, then build `syn rat calibrate` (fullscreen overlay → per-user `~/.config/synapse/rat_user.bin`).
3. **Phase 7 — Package manager:** `syn.toml` + lockfile, `syn add`/`syn install`, path + git deps, semver resolution (design doc 004).
4. **Phases 8–9:** performance program (published `docs/BENCHMARKS.md` vs CPython) and tooling (REPL, `syn disasm`, `syn fmt`, guide, AI prompt-pack).

**Build/verify loop:** `cmake --build --preset debug` (or `release`), then `python tests/run.py` (auto-builds) and `ctest --test-dir build/debug`.
