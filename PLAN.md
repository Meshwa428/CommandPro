# Synapse v2 — Master Plan

> Ground-up rewrite of the Synapse language. This document is the single source of
> truth for scope, phasing, and engineering rules. Detailed designs live in
> `docs/design/`:
>
> - [001 — Language Design](docs/design/001-language.md): syntax, semantics, what changed from v1 and why
> - [002 — Architecture](docs/design/002-architecture.md): compiler pipeline, bytecode VM, runtime
> - [003 — Testing & Benchmarks](docs/design/003-testing.md): test taxonomy, harness, Python-comparison benchmark suite
> - [004 — Modules & Packages](docs/design/004-modules-packages.md): module system, manifest, package manager

---

## 1. Vision

Synapse is a scripting language for OS automation (mouse, keyboard, windows,
apps) built for the AI era: the **primary author of Synapse code is an LLM**,
the primary reviewer is a human. That ordering drives every design decision:

1. **Token-cheap.** Every syntax decision is measured in tokenizer cost. A
   Synapse script for a task should cost fewer output tokens than the
   equivalent Python + pyautogui.
2. **Unambiguous.** One way to spell each concept. An LLM should never have to
   guess between synonyms, and a human should be able to read any script aloud.
3. **Fast.** Compiles to bytecode, runs on a high-performance C++ VM, with the
   explicit target of beating CPython on comparable workloads.
4. **Modular.** First-class module system and a package manager, so automation
   libraries (app-specific flows, site recipes, shared helpers) are shareable.

**Non-goals for v2.0:** static generics, in-language multithreading, JIT
(post-1.0 track, §6). A JIT-ready bytecode design *is* in scope.

## 2. Principles (every PR is judged against these)

1. **One grammar, one register.** All keywords are lowercase. No `loop while`
   vs `while`, no `ENTER` vs `"Return"`, no `==` vs `===`.
2. **Token economy is a benchmark.** Lowercase English words are ~1 BPE token;
   `WINDOW` is often 2–3. No mandatory semicolons (1 token/line saved). No
   boilerplate (`import sys` ceremonies). The test suite includes a
   tokenizer-cost report over the example corpus, tracked like a perf number.
3. **Natural but rigid.** Command statements read like English
   (`move mouse to 300, 400`) but each command has exactly one fixed grammar
   production. English-*like*, never English-*loose*.
4. **No silent coercion.** `"a" + 1` is an error. String building uses
   interpolation: `"a = {a}"`.
5. **Tests land with the feature, in the same commit.** A feature without
   conformance tests does not merge. CI red → nothing merges.
6. **Speed is a feature with a number.** Every phase ends with a benchmark run;
   >5% regression on any tracked benchmark blocks the merge.
7. **The VM never crashes on user input.** Bad scripts produce diagnostics with
   file:line:column, a source excerpt, and a hint. Fuzzers run in CI.

## 3. Repository layout (target)

```
synapse/
├── CMakeLists.txt              # top-level; presets in CMakePresets.json
├── cmake/                      # toolchain, warnings, sanitizer modules
├── include/synapse/            # public API of libsynapse (installable)
├── src/
│   ├── frontend/               # lexer, parser, ast, diagnostics
│   ├── sema/                   # resolver, constant folding, lowering
│   ├── bytecode/               # chunk, opcode defs, disassembler
│   ├── vm/                     # interpreter loop, call frames, inline caches
│   ├── runtime/                # value repr, GC, strings, collections
│   ├── stdlib/                 # builtin modules (math, str, list, time, …)
│   ├── modules/                # module loader, package resolution
│   ├── platform/               # OS automation: interface + linux/win/mock impls
│   └── cli/                    # `syn` driver: run, repl, disasm, add/install, fmt
├── tests/
│   ├── unit/                   # C++ unit tests (Catch2) per component
│   ├── conformance/            # .syn files + expected output (golden tests)
│   ├── errors/                 # .syn files + expected diagnostics
│   ├── fuzz/                   # libFuzzer targets: lexer, parser, vm
│   └── bench/                  # benchmark scripts + Python equivalents + runner
├── docs/
│   ├── design/                 # numbered design docs (this plan's children)
│   ├── spec/                   # the normative language spec, grows with phases
│   └── guide/                  # user-facing tutorial & reference
├── examples/
└── .github/workflows/          # ci.yml (build matrix + tests), bench.yml
```

**Build & quality gates:**
- CMake ≥ 3.25 with `CMakePresets.json` (`debug`, `release`, `asan`, `ubsan`, `fuzz`).
- C++20. `-Wall -Wextra -Wpedantic -Werror` in CI.
- `clang-format` + `clang-tidy` configs at repo root; enforced by CI.
- Catch2 via FetchContent; everything runs under `ctest`.
- CI: Linux GCC + Clang, debug + release + ASan/UBSan jobs, fuzz smoke (60s/target).

## 4. Execution engine (summary — full detail in design doc 002)

```
source ─ lexer ─ tokens ─ parser ─ AST ─ resolver ─ annotated AST
       ─ compiler (w/ constant folding) ─ bytecode chunk ─ VM
```

- **Register-based bytecode VM** (not stack-based — fewer dispatches per
  expression; the biggest architectural lever vs CPython).
- **NaN-boxed 8-byte values**, computed-goto dispatch, interned strings with
  precomputed hashes, inline caches for globals and method lookups.
- **GC:** precise mark-sweep first; allocator interface designed for a
  generational upgrade from day one.
- The v1 tree-walking `interpreter/` is **gone**. One execution engine. The
  mock platform backend is how tests run automation commands deterministically.

## 5. Phases

Each phase has a **Definition of Done**: spec section written, feature
implemented, unit + conformance + error tests passing, benchmarks not
regressed, token-cost report not regressed, docs updated. No phase starts
until the previous phase's DoD is met.

| Phase | Name | Contents |
|-------|------|----------|
| 0 | **Foundations** | Repo skeleton, CMake presets, CI, Catch2 wired, conformance harness (`tests/run.py`) running a hello-world golden test end-to-end, benchmark + token-cost runner skeletons, lint configs. *Infrastructure before features.* |
| 1 | **Calculator core** | Lexer, parser, diagnostics engine, bytecode, VM loop. int64/float64 (distinct), strings + interpolation, booleans, `none`, `let`, assignment, arithmetic, comparison, `and/or/not`, `say`. Fuzz targets live. |
| 2 | **Control flow** | `if/else if/else`, `while`, `repeat N times [as i]`, `for x in …`, ranges, `break`/`continue`. Constant folding pass. |
| 3 | **Functions** | `fn`, returns, closures with upvalues, native function interface, arity/error checks, recursion guard. First Python-comparison benchmarks (fib, call overhead). |
| 4 | **Collections** | Lists, maps, tuples, indexing, slicing, methods (`push`, `keys`, …), iteration, value equality rules. Map/list stress benchmarks. |
| 5 | **Errors & modules** | `try/catch/finally`, `throw`, error values with traces; `use` statement with a deterministic module loader (local files + stdlib namespaces). |
| 6 | **Automation layer** | Platform interface + mock backend first, then Linux (uinput/X11/Wayland). Command-statement grammar (`move mouse to …`, `press ctrl+c`, `type "hi"`, `open app "…"`, `wait 2s`) as sugar over `mouse.*`/`keyboard.*`/`window.*`/`app.*` stdlib. All conformance-tested against the mock backend. |
| 7 | **Package manager** | `syn.toml` manifest + lockfile, `syn add` / `syn install`, path + git dependencies, semver resolution. Registry protocol speced (hosted registry itself is post-1.0). See design doc 004. |
| 8 | **Performance program** | Full benchmark matrix vs CPython (Lua as stretch reference), profile-guided passes: superinstructions, IC tuning, allocator tuning, GC pacing. Published `docs/BENCHMARKS.md` with reproducible methodology. |
| 9 | **Tooling** | REPL, `syn disasm`, `syn fmt`, error-message polish pass, user guide, AI prompt-pack (compact grammar card for system prompts). |

## 6. Post-1.0 tracks (explicitly out of scope now)

Template/baseline JIT, generational+incremental GC, Windows/macOS automation
backends, hosted package registry, LSP server, optional type annotations with
checked mode.

## 7. Working agreements

- Conventional commits (`feat:`, `fix:`, `perf:`, `test:`, `docs:`, `chore:`).
- Work happens on `v2`; it replaces `main` once Phase 3 lands.
- Every opcode, grammar production, and stdlib function gets a spec paragraph
  in `docs/spec/` *before or with* implementation — the spec is the contract
  the conformance tests check.
- Benchmark numbers are only quoted from the pinned runner
  (`tests/bench/run.py --compare python3`) on release builds; never from debug.
