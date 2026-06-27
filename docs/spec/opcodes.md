# Synapse v2 — Bytecode Instruction Set

Status: **draft** · Target: register-based VM, 64-bit fixed-width instructions

---

## 1. Why 64-bit Fixed-Width

A 64-bit CPU fetches 64 bits per memory access regardless of instruction width.
Using 32-bit instructions wastes the upper 32 bits on every single fetch.
Using 64-bit instructions lets us embed large immediates directly in the
instruction word — eliminating constant pool lookups for the most common cases.

**The cost of one instruction execution:**
```
1. Fetch:    load 64-bit word from instruction cache        (1 memory op)
2. Dispatch: computed-goto to handler via dispatch table    (1 indirect jump)
3. Operands: load from register file / decode from instr   (0–2 ops)
```

Inline immediates eliminate step 3 for integers, loop bounds, field keys,
and jump offsets. The constant pool is only hit for strings and 64-bit floats.

**Comparison — `for i in 0 to 99` loop setup:**
```
32-bit approach:                    64-bit approach:
  LOAD_CONST r0, [idx:0]              RANGE_PREP r2, start=0, stop=99, step=1
  LOAD_CONST r1, [idx:99]           → 1 dispatch, 0 pool lookups
  LOAD_CONST r2, [idx:1]
  RANGE_PREP r3, r0, r1, r2
→ 4 dispatches, 3 pool lookups
```

---

## 2. Instruction Formats

All instructions are exactly 64 bits (8 bytes). Fetched as one `uint64_t`.
Fields are packed from the MSB down (big-endian field order in the diagram,
actual byte order follows host endianness — LE on x86-64).

### Format R — Three-register (arithmetic, logic, collections)
```
63       56 55       48 47       40 39       32 31                           0
┌──────────┬──────────┬──────────┬──────────┬──────────────────────────────┐
│  OP  (8) │  A   (8) │  B   (8) │  C   (8) │         padding (32)         │
└──────────┴──────────┴──────────┴──────────┴──────────────────────────────┘
Semantics: R[A] = op(R[B], R[C])
```

### Format RI — Register + 40-bit signed immediate
```
63       56 55       48 47       40 39                                       0
┌──────────┬──────────┬──────────┬──────────────────────────────────────────┐
│  OP  (8) │  A   (8) │  B   (8) │               imm40 (40)                 │
└──────────┴──────────┴──────────┴──────────────────────────────────────────┘
Semantics: R[A] = op(R[B], sign_extend(imm40))
imm40 range: ±549,755,813,887  (covers all practical integers)
```

### Format I — Register + 48-bit signed immediate (load operations)
```
63       56 55       48 47                                                   0
┌──────────┬──────────┬──────────────────────────────────────────────────────┐
│  OP  (8) │  A   (8) │                     imm48 (48)                       │
└──────────┴──────────┴──────────────────────────────────────────────────────┘
Semantics: R[A] = sign_extend(imm48)
imm48 range: ±140,737,488,355,327  (covers all practical integers inline)
```

### Format J — Jump (48-bit signed PC offset)
```
63       56 55       48 47                                                   0
┌──────────┬──────────┬──────────────────────────────────────────────────────┐
│  OP  (8) │ flags(8) │                    offset48 (48)                     │
└──────────┴──────────┴──────────────────────────────────────────────────────┘
Semantics: pc += sign_extend(offset48)  (relative to NEXT instruction)
offset48 range: ±140T instructions — never need WIDE for jumps
```

### Format RJ — Register + 40-bit jump offset (conditional branch)
```
63       56 55       48 47       40 39                                       0
┌──────────┬──────────┬──────────┬──────────────────────────────────────────┐
│  OP  (8) │  A   (8) │  B   (8) │              offset40 (40)               │
└──────────┴──────────┴──────────┴──────────────────────────────────────────┘
Semantics: if cond(R[A], R[B]) then pc += sign_extend(offset40)
```

### Format RRJ — Two registers + 32-bit jump offset (fused compare-branch)
```
63       56 55       48 47       40 39       32 31                           0
┌──────────┬──────────┬──────────┬──────────┬──────────────────────────────┐
│  OP  (8) │  A   (8) │  B   (8) │  C   (8) │         offset32 (32)        │
└──────────┴──────────┴──────────┴──────────┴──────────────────────────────┘
Semantics: if R[A] op R[B] then pc += sign_extend(offset32)
```

### Format RANGE — Inline loop range (all 4 loop vars in one instruction)
```
63       56 55       48 47        32 31        16 15                         0
┌──────────┬──────────┬────────────┬────────────┬────────────────────────────┐
│  OP  (8) │  A   (8) │  start(16) │  stop (16) │        step (16)           │
└──────────┴──────────┴────────────┴────────────┴────────────────────────────┘
Semantics: prepare range loop: R[A]=start, R[A+1]=stop, R[A+2]=step
start/stop/step range: signed 16-bit = -32768..32767
For ranges outside this: use RANGE_PREP_R (register operands)
```

### Format CALL — Function call (registers + arg count)
```
63       56 55       48 47       40 39       32 31                           0
┌──────────┬──────────┬──────────┬──────────┬──────────────────────────────┐
│  OP  (8) │  A   (8) │ nargs(8) │ nret (8) │            key (32)           │
└──────────┴──────────┴──────────┴──────────┴──────────────────────────────┘
A     = base register (function in R[A], args in R[A+1..A+nargs])
nargs = argument count
nret  = expected return count (0 = discard, 255 = variable)
key   = inline cache slot index
```

---

## 3. Register Conventions

- **256 registers per call frame** (R[0]..R[255]) — 8-bit operands
- **R[0]** = implicit self / accumulator in some operations
- **R[250..255]** = reserved for VM temporaries (calling convention scratch)
- Registers are **NaN-boxed 8-byte Values** (see §5)
- Register file is a contiguous array on the C++ stack — fast access
- Call frames **overlap**: callee's R[0] = caller's R[A+1] (no arg copying)

---

## 4. The Constant Pool

Only needed for values that don't fit inline:
- **64-bit float literals** (doubles that are real NaN-boxed Values)
- **String literals** (pointer into interned string table)
- **Function prototypes** (for closures)
- **Integer constants > 48-bit** (extremely rare — virtually never)

Pool entries are indexed by a 16-bit index (up to 65,536 constants per
function — more than enough). Pool is read-only after compilation.

---

## 5. Value Representation — NaN Boxing

Every register holds a 64-bit `Value`. Non-NaN IEEE 754 doubles are stored
as-is. The quiet-NaN payload space tags all other types:

```
Float64 (non-NaN):  [normal IEEE 754 double]
                     ↑ exponent ≠ 0x7FF or mantissa = 0

Tagged types use the quiet-NaN space (exp=0x7FF, quiet bit set):
  Bits 63..48 = 0xFFF8..0xFFFF  (NaN prefix, 16 bits)
  Bits 47..0  = tag (4 bits) + payload (44 bits)

Tags:
  0x0  = int48    payload = signed 48-bit integer (cast to int64)
  0x1  = true     payload = 0
  0x2  = false    payload = 0
  0x3  = none     payload = 0
  0x4  = pointer  payload = 44-bit heap pointer (16-byte aligned → 48-bit effective)
  0x5  = duration payload = nanoseconds (signed 44-bit = ~100 days max)
```

**Heap object types** (distinguished by object header tag, not Value tag):
`String`, `List`, `Map`, `Tuple`, `Function`, `Closure`, `Upvalue`,
`NativeFn`, `Error`, `Module`, `Range` (lazy)

**Why int48 inline:**
The most common integer operations never touch the heap or constant pool.
Loop counters, array indices, small literals — all fit in 48 bits.

---

## 6. Opcode Table

Legend: `R[x]` = register x · `K[x]` = constant pool entry x · `imm` = inline immediate

### 6.1 Load / Move

| Op | Hex | Format | Semantics | Notes |
|----|-----|--------|-----------|-------|
| `LOAD_INT`   | 0x01 | I   | `R[A] = imm48` | Integer literal inline, 0 pool lookups |
| `LOAD_FLOAT` | 0x02 | R   | `R[A] = K[B]` | Float from pool (64-bit double) |
| `LOAD_TRUE`  | 0x03 | I   | `R[A] = true` | |
| `LOAD_FALSE` | 0x04 | I   | `R[A] = false` | |
| `LOAD_NONE`  | 0x05 | I   | `R[A] = none` | |
| `LOAD_CONST` | 0x06 | RI  | `R[A] = K[imm]` | Generic pool load |
| `MOVE`       | 0x07 | R   | `R[A] = R[B]` | Register copy |
| `LOAD_DURATION` | 0x08 | I | `R[A] = Duration(imm48 ns)` | Duration literal |

### 6.2 Globals & Upvalues

| Op | Hex | Format | Semantics | Notes |
|----|-----|--------|-----------|-------|
| `GET_GLOBAL` | 0x10 | RI | `R[A] = globals[K[imm]]` | Global var read |
| `SET_GLOBAL` | 0x11 | RI | `globals[K[imm]] = R[A]` | Global var write |
| `GET_UPVAL`  | 0x12 | R  | `R[A] = upvalues[B]` | Closure capture read |
| `SET_UPVAL`  | 0x13 | R  | `upvalues[B] = R[A]` | Closure capture write |
| `CLOSE_UPVAL`| 0x14 | R  | Close upvalues ≥ R[A] onto heap | Called on scope exit |

### 6.3 Arithmetic — Register × Register

| Op | Hex | Format | Semantics |
|----|-----|--------|-----------|
| `ADD`  | 0x20 | R | `R[A] = R[B] + R[C]` |
| `SUB`  | 0x21 | R | `R[A] = R[B] - R[C]` |
| `MUL`  | 0x22 | R | `R[A] = R[B] * R[C]` |
| `DIV`  | 0x23 | R | `R[A] = R[B] / R[C]`  (always float) |
| `IDIV` | 0x24 | R | `R[A] = R[B] // R[C]` (floor div, int) |
| `MOD`  | 0x25 | R | `R[A] = R[B] % R[C]` |
| `POW`  | 0x26 | R | `R[A] = R[B] ** R[C]` |
| `UNM`  | 0x27 | R | `R[A] = -R[B]` |

### 6.4 Arithmetic — Register × Inline Immediate

These eliminate a constant pool lookup for the right operand.
`imm40` is a signed 40-bit value embedded in the instruction.

| Op | Hex | Format | Semantics | Example |
|----|-----|--------|-----------|---------|
| `ADDI`  | 0x28 | RI | `R[A] = R[B] + imm40` | `x + 1`, `x + 1000` |
| `SUBI`  | 0x29 | RI | `R[A] = R[B] - imm40` | `x - 1` |
| `MULI`  | 0x2A | RI | `R[A] = R[B] * imm40` | `x * 2` |
| `DIVI`  | 0x2B | RI | `R[A] = R[B] / imm40` | `x / 2` (float) |
| `IDIVI` | 0x2C | RI | `R[A] = R[B] // imm40` | `x // 2` |
| `MODI`  | 0x2D | RI | `R[A] = R[B] % imm40` | `x % 2` |
| `POWI`  | 0x2E | RI | `R[A] = R[B] ** imm40` | `x ** 2` |

### 6.5 Fused Compare-and-Branch

The single biggest dispatch saver. Instead of `CMP r0, r1` + `JUMP_IF` (2
dispatches), one instruction does both (1 dispatch). The offset is the jump
displacement applied to PC if the condition is true.

| Op | Hex | Format | Semantics |
|----|-----|--------|-----------|
| `JEQ`  | 0x30 | RRJ | `if R[A] == R[B]: pc += offset32` |
| `JNEQ` | 0x31 | RRJ | `if R[A] != R[B]: pc += offset32` |
| `JLT`  | 0x32 | RRJ | `if R[A] < R[B]: pc += offset32` |
| `JLTE` | 0x33 | RRJ | `if R[A] <= R[B]: pc += offset32` |
| `JGT`  | 0x34 | RRJ | `if R[A] > R[B]: pc += offset32` |
| `JGTE` | 0x35 | RRJ | `if R[A] >= R[B]: pc += offset32` |
| `JT`   | 0x36 | RJ  | `if truthy(R[A]): pc += offset40` |
| `JF`   | 0x37 | RJ  | `if falsy(R[A]): pc += offset40` |
| `JUMP` | 0x38 | J   | `pc += offset48` (unconditional) |
| `JNIL` | 0x39 | RJ  | `if R[A] == none: pc += offset40` |
| `JNNIL`| 0x3A | RJ  | `if R[A] != none: pc += offset40` |

**Fused compare-and-branch with immediate:**

| Op | Hex | Format | Semantics | Example |
|----|-----|--------|-----------|---------|
| `JEQI`  | 0x3B | RI+off | `if R[A] == imm: pc += off` | `if x == 0` |
| `JLTI`  | 0x3C | RI+off | `if R[A] < imm: pc += off`  | `while n > 0` |
| `JLTEI` | 0x3D | RI+off | `if R[A] <= imm: pc += off` | |

> Note: JEQI/JLTI pack reg(8) + imm(24) + offset(24) = 56 bits + OP(8) = 64 total

### 6.6 Comparison — Value Result (for non-branch contexts)

Used when the comparison result is stored in a register (e.g., `let ok = a < b`).

| Op | Hex | Format | Semantics |
|----|-----|--------|-----------|
| `EQ`   | 0x40 | R | `R[A] = (R[B] == R[C])` |
| `NEQ`  | 0x41 | R | `R[A] = (R[B] != R[C])` |
| `LT`   | 0x42 | R | `R[A] = (R[B] < R[C])` |
| `LTE`  | 0x43 | R | `R[A] = (R[B] <= R[C])` |
| `NOT`  | 0x44 | R | `R[A] = !truthy(R[B])` |

### 6.7 Strings

| Op | Hex | Format | Semantics | Notes |
|----|-----|--------|-----------|-------|
| `CONCAT`   | 0x50 | R  | `R[A] = R[B] + R[C]`              | Two string concat |
| `CONCAT_N` | 0x51 | R  | `R[A] = concat(R[B..B+C-1])`      | N strings, 1 dispatch |
| `STR_LEN`  | 0x52 | R  | `R[A] = len(R[B])`                | string/list/map length |
| `INTERP`   | 0x53 | R  | `R[A] = interpolate(R[B..B+C-1])` | "hello {name}" → 1 op |

> `CONCAT_N` and `INTERP` are the key wins: `say "hi {name}, {age}"` compiles
> to one INTERP instruction instead of 4 CONCAT operations.

### 6.8 Collections

| Op | Hex | Format | Semantics | Notes |
|----|-----|--------|-----------|-------|
| `NEW_LIST`    | 0x60 | R  | `R[A] = List(R[B..B+C-1])` | Create + populate in 1 op |
| `NEW_MAP`     | 0x61 | R  | `R[A] = Map()` | Empty map |
| `NEW_TUPLE`   | 0x62 | R  | `R[A] = Tuple(R[B..B+C-1])` | Immutable |
| `NEW_RANGE`   | 0x63 | R  | `R[A] = Range(R[B], R[C], step=1)` | Lazy range |
| `GET_FIELD`   | 0x64 | R  | `R[A] = R[B][R[C]]` | General index |
| `SET_FIELD`   | 0x65 | R  | `R[A][R[B]] = R[C]` | General set |
| `GET_FIELDK`  | 0x66 | RI | `R[A] = R[B][K[imm]]` | String key from pool |
| `SET_FIELDK`  | 0x67 | RI | `R[B][K[imm]] = R[A]` | String key set |
| `GETI`        | 0x68 | RI | `R[A] = R[B][imm]` | Integer index inline |
| `SETI`        | 0x69 | RI | `R[A][imm] = R[B]` | Integer index set inline |
| `APPEND`      | 0x6A | R  | `R[A].push(R[B])` | List append in-place |
| `HAS_KEY`     | 0x6B | R  | `R[A] = (R[C] in R[B])` | Map/list membership |
| `MAP_SETK`    | 0x6C | RI | `R[A][K[imm]] = R[B]; R[A]` | Map literal building |

### 6.9 Loops — Range and For-in

These are purpose-built to minimize loop overhead. A typical `for i in 0 to 99`
costs 1 RANGE_PREP + 1 RANGE_STEP per iteration (loop body instructions aside).

| Op | Hex | Format | Semantics | Notes |
|----|-----|--------|-----------|-------|
| `RANGE_PREP`  | 0x70 | RANGE | `R[A]=start; R[A+1]=stop; R[A+2]=step` | All inline, 1 dispatch |
| `RANGE_PREP_R`| 0x71 | R     | `R[A]=R[B]; R[A+1]=R[C]; R[A+2]=1` | Registers, step=1 |
| `RANGE_PREP_RS`| 0x72| R    | `R[A]=R[B]; R[A+1]=R[C]; R[A+2]=R[D]` | Registers+step |
| `RANGE_STEP`  | 0x73 | RJ   | `R[A]+=R[A+2]; if R[A] > R[A+1]: pc+=offset` | Advance+branch |
| `FOR_PREP`    | 0x74 | R    | `R[A]=iterator(R[B]); R[A+1]=nil` | For-in setup |
| `FOR_STEP`    | 0x75 | RJ   | `R[A+1]=next(R[A]); if done: pc+=offset` | Advance+branch |
| `ENUMERATE`   | 0x76 | R    | `R[A]=enumerate_iter(R[B])` | enumerate(xs) |
| `ZIP`         | 0x77 | R    | `R[A]=zip_iter(R[B], R[C])` | zip(xs,ys) |

### 6.10 Functions

| Op | Hex | Format | Semantics | Notes |
|----|-----|--------|-----------|-------|
| `CLOSURE`  | 0x80 | RI | `R[A] = Closure(proto[imm])` | Capture upvalues |
| `CALL`     | 0x81 | CALL | `R[A..A+nret-1] = R[A](R[A+1..A+nargs])` | Full call |
| `CALL_0`   | 0x82 | R  | `R[A] = R[B]()` | No args, 1 return (fast path) |
| `CALL_1`   | 0x83 | R  | `R[A] = R[B](R[C])` | 1 arg, 1 return (fast path) |
| `CALL_N`   | 0x84 | R  | `R[A](R[A+1..A+B])` | N args, discard return |
| `TAIL`     | 0x85 | R  | tail call R[A] with B args | Reuse frame |
| `RETURN`   | 0x86 | R  | return R[A..A+B-1] | B values |
| `RETURN_1` | 0x87 | R  | return R[A] | Single return (most common) |
| `RETURN_0` | 0x88 | I  | return none | Void return |
| `NAMED_ARG`| 0x89 | RI | mark R[A] as named arg K[imm] | For `fn(port: 3000)` |

### 6.11 Type Operations

| Op | Hex | Format | Semantics |
|----|-----|--------|-----------|
| `TYPEOF`    | 0x90 | R | `R[A] = type_string(R[B])` |
| `IS_INT`    | 0x91 | R | `R[A] = (tag(R[B]) == INT)` |
| `IS_FLOAT`  | 0x92 | R | `R[A] = (tag(R[B]) == FLOAT)` |
| `IS_STR`    | 0x93 | R | `R[A] = (tag(R[B]) == STRING)` |
| `IS_BOOL`   | 0x94 | R | `R[A] = (tag(R[B]) == BOOL)` |
| `IS_NONE`   | 0x95 | R | `R[A] = (R[B] == NONE)` |
| `IS_LIST`   | 0x96 | R | `R[A] = (tag(R[B]) == LIST)` |
| `IS_MAP`    | 0x97 | R | `R[A] = (tag(R[B]) == MAP)` |
| `TO_INT`    | 0x98 | R | `R[A] = coerce_int(R[B])` |
| `TO_FLOAT`  | 0x99 | R | `R[A] = coerce_float(R[B])` |
| `TO_STR`    | 0x9A | R | `R[A] = coerce_str(R[B])` |
| `TO_BOOL`   | 0x9B | R | `R[A] = coerce_bool(R[B])` |

### 6.12 Null Coalescing & Optional Chain

| Op | Hex | Format | Semantics | Source |
|----|-----|--------|-----------|--------|
| `NULLC`   | 0xA0 | R  | `R[A] = R[B] ?? R[C]` | `x ?? y` |
| `NULLCI`  | 0xA1 | RI | `R[A] = R[B] ?? K[imm]` | `x ?? "default"` |
| `OPT_GET` | 0xA2 | RI | `R[A] = R[B]?.K[imm] (none if R[B]==none)` | `obj?.field` |

### 6.13 Error Handling

| Op | Hex | Format | Semantics | Notes |
|----|-----|--------|-----------|-------|
| `TRY_PUSH` | 0xB0 | RJ | push try frame; on error: R[A]=err, pc+=offset | Setup try block |
| `TRY_POP`  | 0xB1 | I  | pop try frame (end of try block) | |
| `THROW`    | 0xB2 | R  | throw R[A] | Error must be Error object |
| `ERR_NEW`  | 0xB3 | R  | `R[A] = Error(message=R[B])` | |
| `ERR_NEWK` | 0xB4 | RI | `R[A] = Error(R[B], type=K[imm])` | Typed error |
| `ERR_TYPE` | 0xB5 | R  | `R[A] = R[B].type` | Read error.type |
| `ERR_MSG`  | 0xB6 | R  | `R[A] = R[B].message` | Read error.message |

### 6.14 Match Statement

| Op | Hex | Format | Semantics | Notes |
|----|-----|--------|-----------|-------|
| `MATCH_EQ`   | 0xC0 | RRJ | `if R[A] == R[B]: pc += offset` | Case value |
| `MATCH_TYPE` | 0xC1 | RJ  | `if type_tag(R[A]) == B: pc += offset` | Case type |
| `MATCH_GUARD`| 0xC2 | RJ  | `if truthy(R[A]): pc += offset` | Case guard |
| `MATCH_TUPLE`| 0xC3 | R   | destructure R[A] into R[B..B+C-1]; 0 if wrong arity | |

### 6.15 Modules

| Op | Hex | Format | Semantics |
|----|-----|--------|-----------|
| `USE`      | 0xD0 | RI | `R[A] = load_module(K[imm])` |
| `USE_FROM` | 0xD1 | RI | `R[A] = R[B].export(K[imm])` |

### 6.16 Automation Commands
(Desugar to stdlib calls at compile time — no dedicated opcodes needed.
 `mouse 300, 400` → `CALL mouse.move, (300, 400)`)

### 6.17 VM Control

| Op | Hex | Format | Semantics |
|----|-----|--------|-----------|
| `NOP`     | 0xFD | I | No operation |
| `BREAKPT` | 0xFE | I | Debugger breakpoint |
| `HALT`    | 0xFF | I | Stop VM, return R[0] to host |

---

## 7. Superinstructions (Phase 8 — Profile-Guided)

Not added speculatively. These are added after benchmark profiles identify
the hottest instruction pairs. Candidates:

| Superinstruction | Replaces | Saves |
|---|---|---|
| `ADDI_JLT r0, 1, r1, off` | `ADDI` + `JLT` | 1 dispatch per loop iteration |
| `MOVE_CALL r0, r1, n` | `MOVE` + `CALL` | 1 dispatch per method call |
| `GETI_MOVE r0, r1, i, r2` | `GETI` + `MOVE` | 1 dispatch per array access |
| `GET_FIELDK_JF r0, k, off` | `GET_FIELDK` + `JF` | 1 dispatch per nil check |

---

## 8. Encoding Examples

### `let x = 42`
```
LOAD_INT  r0, 42        # Format I: OP=0x01, A=r0, imm48=42
                        # Constant pool: not touched
```

### `let y = x + 1000`
```
ADDI  r1, r0, 1000      # Format RI: OP=0x28, A=r1, B=r0, imm40=1000
                        # Constant pool: not touched
```

### `for i in 0 to 99 { say i }`
```
RANGE_PREP r0, 0, 99, 1  # Format RANGE: 1 instruction, all inline
:loop
  CALL_1  r2, say, r0    # say(R[0])
  RANGE_STEP r0, :loop   # r0++; if r0 > 99: break (1 instruction)
```
**Total loop overhead: 2 instructions per iteration (1 body + 1 step)**
Python's equivalent: 5+ bytecodes per iteration.

### `if x > 0 and x < 100 { ... }`
```
JLTE  r0, r_zero, :skip   # if x <= 0: skip (fused)
JGTE  r0, r_hundred, :skip # if x >= 100: skip (fused)
  ... body ...
:skip
```

### `let result = items |> filter(fn(x){ x > 0 }) |> map(fn(x){ x * 2 }) |> sum()`
```
# Pipe compiles to nested calls — no pipe-specific opcode needed
CALL  r0, filter, items, <fn1>
CALL  r0, map,    r0,    <fn2>
CALL  r0, sum,    r0
MOVE  result, r0
```

### `mouse 300, 400`
```
# Desugars at parse time — no mouse opcode needed
LOAD_INT  r0, 300
LOAD_INT  r1, 400
CALL_N    mouse.move, r0, r1     # native call into platform layer
```

---

## 9. Opcode Count Summary

| Category | Count | Range |
|---|---|---|
| Load/Move | 8 | 0x01–0x0F |
| Globals/Upvalues | 5 | 0x10–0x1F |
| Arithmetic R×R | 8 | 0x20–0x27 |
| Arithmetic R×imm | 7 | 0x28–0x2F |
| Fused branch | 11 | 0x30–0x3F |
| Comparison value | 5 | 0x40–0x4F |
| Strings | 4 | 0x50–0x5F |
| Collections | 13 | 0x60–0x6F |
| Loops | 8 | 0x70–0x7F |
| Functions | 9 | 0x80–0x8F |
| Type ops | 12 | 0x90–0x9F |
| Null/Optional | 3 | 0xA0–0xAF |
| Error handling | 7 | 0xB0–0xBF |
| Match | 4 | 0xC0–0xCF |
| Modules | 2 | 0xD0–0xDF |
| VM control | 3 | 0xFD–0xFF |
| **Total** | **109** | |
| **Available** | **256** | 147 reserved for Phase 8 superinstructions |

---

## 10. VM Loop Skeleton (C++20)

```cpp
// The dispatch table — one entry per opcode
static const void* dispatch_table[256] = {
    [OP_LOAD_INT]  = &&lbl_load_int,
    [OP_ADD]       = &&lbl_add,
    // ...
};

#define FETCH()     (*pc++)
#define OP(w)       ((w) >> 56)
#define A(w)        (((w) >> 48) & 0xFF)
#define B(w)        (((w) >> 40) & 0xFF)
#define C(w)        (((w) >> 32) & 0xFF)
#define IMM48(w)    sign_extend48((w) & 0x0000FFFFFFFFFFFF)
#define IMM40(w)    sign_extend40((w) & 0x000000FFFFFFFFFF)
#define IMM32(w)    sign_extend32((w) & 0x00000000FFFFFFFF)
#define DISPATCH()  do { uint64_t w = FETCH(); goto *dispatch_table[OP(w)]; } while(0)

void vm_run(CallFrame* frame) {
    uint64_t* pc = frame->chunk->code;
    Value*    regs = frame->registers;
    DISPATCH();

    lbl_load_int: {
        uint64_t w = *(pc - 1);  // already fetched
        regs[A(w)] = value_int(IMM48(w));
        DISPATCH();
    }
    lbl_add: {
        uint64_t w = *(pc - 1);
        regs[A(w)] = value_add(regs[B(w)], regs[C(w)]);
        DISPATCH();
    }
    lbl_addi: {
        uint64_t w = *(pc - 1);
        regs[A(w)] = value_add_int(regs[B(w)], IMM40(w));
        DISPATCH();
    }
    lbl_range_step: {
        uint64_t w = *(pc - 1);
        uint8_t base = A(w);
        int64_t cur  = value_to_int(regs[base]) + value_to_int(regs[base+2]);
        regs[base]   = value_int(cur);
        if (cur > value_to_int(regs[base+1]))
            pc += IMM40(w);   // jump past loop body
        DISPATCH();
    }
    // ... 105 more handlers
}
```

One `DISPATCH()` = one `uint64_t` load + one indirect jump. That's it.
All operands decoded from the already-loaded `w` — zero extra memory reads
for inline-immediate instructions.
