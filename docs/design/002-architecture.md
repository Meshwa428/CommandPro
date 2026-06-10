# Design 002 — Architecture: Compiler Pipeline & VM

Status: **draft for approval** · Scope: everything between source text and side effects

## 1. Pipeline overview

```
.syn source
   │  Lexer (frontend/)            — hand-written, zero-copy tokens over source buffer
   ▼
tokens
   │  Parser (frontend/)           — recursive descent + Pratt for expressions,
   ▼                                 error-recovering (sync at statement starts)
AST (arena-allocated)
   │  Resolver (sema/)             — scopes, locals→slots, upvalue analysis,
   ▼                                 const-ness, command desugaring to calls
annotated AST
   │  Compiler (sema/ + bytecode/) — register allocation, constant folding,
   ▼                                 jump patching, line tables
bytecode Chunk
   │  VM (vm/ + runtime/)          — register machine, computed goto, NaN boxing
   ▼
effects (stdout, platform layer)
```

Single C++ library `libsynapse`; the `syn` CLI is a thin client. Embedding API
(`synapse::Engine`) is public from day one — it's also how unit tests drive
everything without subprocesses.

**Deleted from v1:** the tree-walking interpreter. One execution path. Debug
introspection comes from the disassembler and VM trace mode instead.

## 2. Why register-based (the headline decision)

Stack VMs (CPython, v1) execute `a*b + c` as ~7 dispatches (3 loads, 2 ops,
2 implicit). A register VM does 2: `MUL r3, r1, r2` / `ADD r4, r3, r0`.
Dispatch is the dominant interpreter cost, so cutting instruction count per
expression by 2–3× is the single biggest win available without a JIT (this is
the Lua 5 lesson). Costs: wider instructions (32-bit fixed), a register
allocator in the compiler. Accepted.

**Instruction format:** fixed 32-bit, `OP A B C` (8/8/8/8) with `OP A Bx`
(8/8/16) variants for constants/jumps. Fixed width keeps decode branchless and
is JIT-friendly later.

## 3. Value representation

NaN-boxed 8-byte `Value`:

- float64 stored as-is (non-NaN payloads)
- quiet-NaN space tags: int (48-bit inline; overflow promotes via checked
  path), `true/false/none`, and 48-bit pointers to heap objects
- Heap objects: common header (type tag, GC bits) + `String`, `List`, `Map`,
  `Tuple`, `Function`, `Closure`, `Upvalue`, `NativeFn`, `Error`, `Module`,
  `Duration` is *not* heap — it's a tagged int of nanoseconds.

**Strings:** interned in a global table with precomputed FNV-1a hashes →
string equality is pointer equality; map keys never rehash. Small-string
optimization inside the object; rope-free (concatenation allocates, but
interpolation compiles to a single sized build, not repeated `+`).

## 4. The interpreter loop

- **Computed-goto dispatch** (GCC/Clang `&&labels`), `switch` fallback for
  MSVC, selected by macro so both stay compiling.
- Hot VM state (`pc`, register window base, chunk constants pointer) lives in
  locals so it stays in CPU registers; spills only at calls/GC.
- **Call frames:** contiguous register stack with overlapping call windows
  (callee registers start where the caller placed the arguments — calls don't
  copy arguments).
- **Inline caches:** global access compiles to an index into the module's slot
  array after first resolution (no hash lookup in steady state); method calls
  on builtin types dispatch through a per-site cache keyed by type tag.
- **Superinstructions** (Phase 8): fuse pairs measured to be hot
  (`cmp+jump`, `getlocal+call`, `add-const`), driven by the benchmark
  profiles, never speculatively.

## 5. Garbage collection

- Precise mark-sweep, stop-the-world, with explicit root enumeration
  (register stack, globals, interned-string weak set, native handles).
- Allocation through a `Heap` interface with bump-pointer nursery
  *plumbed but disabled* — generational GC is a post-1.0 flip, not a rewrite.
- GC stress mode (`SYN_GC_STRESS=1`: collect on every allocation) runs in CI
  on the conformance suite — this is the #1 defense against the lifetime bugs
  that plague hand-rolled VMs.

## 6. Diagnostics engine

One `Diagnostic` type used by lexer/parser/resolver/VM:

```
error[E0012]: cannot add `string` and `int`
  --> flows/login.syn:14:9
   |
14 |     say "total: " + n
   |         ^^^^^^^^^^^^^ left side is a string
   |
help: use interpolation instead: say "total: {n}"
```

- Stable error codes (`E####`) — the errors test suite asserts on codes, not
  message text, so wording can improve without breaking tests.
- Runtime errors carry a Synapse-level stack trace built from line tables.
- Parser recovers at statement boundaries → multiple errors per run.

## 7. Platform / automation layer

```
src/platform/
├── platform.h        # pure-virtual Platform: mouse, keyboard, window, app, screen, clock
├── mock/             # records every call into an inspectable log; virtual clock
├── linux/            # uinput (input), X11 + wlroots/portal (windows/screen)
└── (win/, mac/ post-1.0)
```

- The VM only ever sees the `Platform` interface; stdlib automation modules
  call through it. **The mock backend is a first-class product**, not a test
  hack: it's how conformance tests assert `move mouse to 300, 400` produced
  exactly `MouseMove{300,400}`, and how `wait 2s` runs in microseconds under
  test (virtual clock).
- All platform calls are capability-gated at engine construction
  (`Engine::Options{ .allow_input=…, .allow_screen=… }`) — embedders/AI
  sandboxes can run scripts with automation disabled.

## 8. Module loading & execution model

- A `Module` = compiled chunk + slot array of globals. `use` triggers
  resolve → compile → execute-once → cache, cycle detection with a clear
  diagnostic. Resolution order: stdlib name → project `src/` → `syn_modules/`
  (per design doc 004).
- Stdlib modules are C++-native but registered through the same `Module`
  interface — no special cases in the VM.

## 9. Performance guardrails (engineering process)

- `tests/bench/run.py` is the only quoted source of numbers (release build,
  pinned CPU governor advice, ≥10 runs, reports median + IQR).
- A `perf:` PR must paste before/after from that runner.
- `syn disasm file.syn` exists from Phase 1 — bytecode quality is reviewable
  in PRs from the first feature onward.
