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

## v0.5.0 — Scheduling & Intervals

**Goal:** Allow time-based and recurring automation.

**Planned Features:**
- [ ] `RUN AT "HH:MM AM/PM" { ... }` 
- [ ] `INTERVAL <duration> { ... }` recurring block
- [ ] Signal handling for graceful script termination (`Ctrl+C`)

---

## v0.6.0 — Error Handling & Robustness

**Goal:** Make scripts resilient to unexpected conditions.

**Planned Features:**
- [x] `try { ... } catch (err) { ... }` blocks
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

## v1.0.0 — Stable Release

**Goal:** Stable, production-quality release with full documentation and test coverage.

**Planned Features:**
- [ ] 90%+ unit test coverage for Lexer and Parser
- [ ] Integration tests for all STDLIB commands
- [ ] Full documentation (LANGUAGE_SPEC, SYNTAX, STDLIB, INTERNALS, CROSS_PLATFORM)
- [ ] Pre-built binary releases for Linux (x86_64) and Windows (x64)
- [ ] GitHub Actions CI/CD pipeline (build + test on Linux and Windows)

---

## v2.0.0 — Advanced UI Detection *(Future Vision)*

> This is the major advanced feature planned for Synapse beyond v1.0. It is **out of scope** for all current releases.

**Goal:** Enable scripts to find and interact with UI elements without hardcoded screen coordinates.

This removes the biggest fragility in coordinate-based automation — if a window moves or the resolution changes, pixel-based scripts break.

### Planned Detection Approaches

#### 2.1 Template Image Matching
Synapse will be able to find a UI element on screen by matching it against a reference image:
```sql
# Find a button on screen by image and click it
let btnPos = SCREEN FIND "assets/submit_button.png";
MOUSE CLICK LEFT AT btnPos;
```

Implementation: OpenCV template matching (`cv::matchTemplate`)

#### 2.2 Accessibility Tree / OS UI APIs
On supported platforms, query the operating system's accessibility APIs:
- **Windows:** `UIAutomation` API
- **Linux:** `AT-SPI2` accessibility framework

```sql
# Click a button by its accessible name
UI CLICK BUTTON "Submit";
UI TYPE FIELD "Username" "john.doe";
UI GET TEXT "status_label" INTO statusMsg;
```

#### 2.3 OCR-Based Text Detection *(v2.1)*
Use OCR to find UI elements based on visible text:
```sql
let okBtn = SCREEN FIND TEXT "OK";
MOUSE CLICK LEFT AT okBtn;
```

Implementation: Tesseract OCR integration.

#### 2.4 AI-Assisted Detection *(v3.0 and beyond)*
Integrate a lightweight on-device vision model to semantically understand screen content:
```sql
# Describe what to find in natural language
let loginForm = AI FIND "the login form";
AI FILL loginForm WITH "username" AS "john", "password" AS "secret";
AI CLICK "the submit button";
```

This represents the long-term AI-native vision for Synapse — a tool that can understand and interact with any interface without manual coordinate mapping.
