# Design 001 — Language Design

Status: **draft for approval** · Scope: surface syntax + core semantics

This doc does three things: audits what was wrong in v1, states the design
rules that prevent those classes of mistakes, and lays out the v2 syntax.

---

## 1. Audit: v1 irregularities (the "psychological disorders")

| # | v1 behavior | Problem |
|---|------------|---------|
| 1 | `let x = 1;` (lowercase) vs `MOUSE MOVE TO (300,400);` (UPPERCASE) | Two languages in one file. Also UPPERCASE words tokenize *worse* (`WINDOW` → 2–3 BPE tokens, `window` → 1). |
| 2 | `loop while (n > 0)` | `loop` is noise; every other language and every human says `while`. |
| 3 | `repeat 5 times { }` with no loop index | Forces users back to manual counters, defeating the construct. |
| 4 | `KEY PRESS ENTER` vs `KEY PRESS "Return"` vs `KEY PRESS "a"` | Three spellings for "press a key"; named keys sometimes bare words, sometimes X11 keysym strings. |
| 5 | `ASK "Enter name:" INTO name AS INT` | SQL-style data flow (`INTO`) that contradicts `let name = …` everywhere else. |
| 6 | `==` loose vs `===` strict | If `==` needs a stricter sibling, `==` is broken. |
| 7 | `"a = " + a` with implicit number→string coercion | JS-ism; hides type bugs and makes `+` mean two things. |
| 8 | `AND`/`OR`/`NOT` uppercase, `true`/`false` lowercase, plus a redundant `!` | Casing soup; two negation operators. |
| 9 | `print` / `println` keywords vs `size(apps)` function | Output is magic syntax, everything else is calls; and `size()` vs methods is unresolved. |
| 10 | Mandatory `;` and parenthesized conditions `if (x > 1)` | Pure token overhead; neither resolves any ambiguity in a newline-terminated grammar. |
| 11 | `WAIT 2s` literal exists, but units stop there | Duration literals were ad hoc, not a real literal type. |

## 2. Design rules

1. **Lowercase everything.** Keywords, commands, key names. (Token-cheap,
   case-consistent, shoutyless.)
2. **Newline ends a statement.** No semicolons. `;` is *allowed* only to put
   two statements on one line. No parens required around conditions.
3. **One spelling per concept.** Enforced by a reserved-word list with no
   synonyms; the spec contains a table "rejected aliases" so we never
   accidentally re-add them.
4. **Words for prose, symbols for math.** `and or not` for logic; `+ - * / %`
   for arithmetic; no `&&/||/!`.
5. **Everything is an expression call underneath.** Command statements and
   `say` are thin sugar over stdlib functions — the AST below the sugar is
   ordinary calls, so there is exactly one semantics to test.

## 3. Core syntax (v2)

### Variables & types

```syn
let name = "Synapse"
let count = 10            # int (64-bit)
let speed = 1.5           # float (64-bit)
let ready = true
let nothing = none
let pos = (300, 400)      # tuple
count = count + 1         # assignment to existing binding
count += 1                # compound assignment: += -= *= /=
```

Dynamic typing, but **strict**: no implicit conversions between types.
Explicit conversion: `int("42")`, `float(3)`, `str(99)`.

### Strings: interpolation, not coercion

```syn
say "a = {a} and a+b = {a + b}"     # interpolation with {expr}
let s = "ab" + "cd"                  # + concatenates strings ONLY
say "literal brace {{ok}}"           # {{ }} escapes
```

`"x" + 1` → compile-time-detectable cases are compile errors; otherwise a
runtime type error with a hint ("use interpolation: \"x{1}\"").

### Output / input

```syn
say "hello"            # newline included (the 99% case = shortest spelling)
write "no newline"     # rare case gets the longer word
let name = ask "Your name: "
let age = int(ask "Age: ")
```

`say`/`write`/`ask` are ordinary stdlib functions with statement-call sugar
(parens optional when used as a statement / simple expression).

### Operators

```syn
+ - * / % **            # / is float division, like Python 3
//                      # floor division
== != < <= > >=         # == is strict (no coercion); there is no ===
and or not
```

### Control flow

```syn
if score >= 90 {
    say "A"
} else if score >= 75 {
    say "B"
} else {
    say "C"
}

while n > 0 {
    n -= 1
}

repeat 5 times {            # when you don't need the index
    say "tick"
}

repeat 5 times as i {       # 0-based index when you do
    say "tick {i}"
}

for item in items {
    say item
}

for i in 0 to 9 {           # inclusive range; `0 to 10 by 2` for step
    say i
}

break
continue
```

Rejected aliases (spec-pinned): `loop`, `until`, `elif`, `foreach`, `do`.

### Functions

```syn
fn add(x, y) {
    return x + y
}

fn greet(who = "world") {       # default parameter values
    say "hello, {who}!"
}

let f = fn(x) { return x * 2 }  # anonymous fn / closure
```

Closures capture by reference (upvalues). Wrong arity is a runtime error with
the function's signature in the message.

### Collections

```syn
let xs = [1, 2, 3]
let user = { "name": "mesh", "age": 20 }
let pair = (10, 20)

xs.push(4)
say xs[0]
say xs[1 to 2]            # slicing reuses range syntax
say user["name"]
say xs.length             # property, not size() — one convention: methods/properties on values
for k, v in user { say "{k}: {v}" }
```

### Errors

```syn
try {
    risky()
} catch err {
    say "failed: {err.message}"
} finally {
    cleanup()
}

throw error("bad input")
```

### Modules (full design in doc 004)

```syn
use mouse, keyboard            # stdlib namespaces (autoloaded lazily anyway; `use` is for clarity/3rd-party)
use "helpers.syn" as helpers
use spotify_flows              # package from syn.toml dependencies
```

## 4. Automation commands — the token-economy centerpiece

Command statements are **fixed grammar productions**, verb-first, lowercase,
space-separated — i.e. maximally aligned with how BPE tokenizers segment
English. Each is sugar for a stdlib call (shown in comments), and the stdlib
form is always also legal (that's what AI tooling can fall back to, and what
the conformance suite tests directly).

```syn
move mouse to 300, 400                  # mouse.move(300, 400)
click                                   # mouse.click("left")
click right at 100, 200                 # mouse.click("right", at: (100, 200))
double click at 100, 200                # mouse.click("left", at: (100,200), times: 2)
drag mouse from 10, 10 to 400, 300      # mouse.drag((10,10), (400,300))
scroll down 3                           # mouse.scroll("down", 3)
hold left button                        # mouse.hold("left")
release left button                     # mouse.release("left")

press enter                             # keyboard.press("enter")
press ctrl+c                            # keyboard.press("ctrl+c")  — chords via +, one canonical name per key
hold shift                              # keyboard.hold("shift")
release shift                           # keyboard.release("shift")
type "Hello, World!"                    # keyboard.type("Hello, World!")

open app "firefox"                      # app.open("firefox")
close app "firefox"                     # app.close("firefox")
focus window "Notepad"                  # window.focus("Notepad")
move window "Notepad" to 100, 100       # window.move("Notepad", (100,100))
resize window "Notepad" to 800, 600     # window.resize("Notepad", (800,600))
maximize window "Notepad"               # window.maximize("Notepad")

wait 2s                                 # time.wait(2s) — duration literal: 500ms, 2s, 1m, 0.5h
wait 500ms

capture screen to "shot.png"            # screen.capture(path: "shot.png")
capture screen from 0, 0 to 800, 600 to "region.png"

if window "Calc" exists {               # window.exists("Calc") — predicate form
    focus window "Calc"
}
```

Rules that keep this rigid:

- **One canonical name per key** (`enter`, not `return`/`Return`; `win`, not
  `super`/`meta`). The spec ships the full key table; unknown names are
  compile-time errors, not runtime surprises.
- Key chords are always `mod+mod+key` with `+`, no spaces, no parens.
- Coordinates are always `x, y` (bare) in command position; tuple `(x, y)`
  in expression position. The compiler desugars both to the same call.
- Durations are a real literal type (`2s`, `500ms`), usable anywhere a number
  duration is expected — not special syntax that only `wait` understands.
- Command verbs are **contextual keywords**: `type`, `click`, `press`, etc.
  are only special at statement start, so `let press = 1` still works and the
  reserved-word footprint stays small.

### Token-cost check (worked example)

"Open Firefox, wait, search" — v1 vs v2 vs Python+pyautogui:

```
v1:   APP OPEN "Firefox"; WAIT 2s; KEY PRESS (WIN + S); KEY TYPE "hello"; KEY PRESS ENTER;
v2:   open app "firefox"
      wait 2s
      press win+s
      type "hello"
      press enter
py:   import pyautogui, subprocess, time
      subprocess.Popen(["firefox"]); time.sleep(2)
      pyautogui.hotkey("win","s"); pyautogui.write("hello"); pyautogui.press("enter")
```

v2 is the cheapest of the three under cl100k/Claude-style BPE (lowercase
words ≈1 token; no semicolons, no parens, no import ceremony). The test suite
gains a `tests/tokencost/` report that tokenizes the example corpus and fails
CI if a syntax change regresses the corpus cost >2% (see design doc 003).

## 5. Grammar discipline

- The normative grammar lives in `docs/spec/grammar.ebnf` and is the *only*
  definition; the parser is reviewed against it, and a CI check keeps the
  spec's production list in sync with parser test coverage.
- Every production added later (match, structs, etc.) requires: EBNF update,
  spec prose, conformance tests, and a token-cost justification if it adds a
  new keyword.

## 6. Open questions (to resolve before Phase 1 freeze)

1. `say` vs keeping `print` — `print` is the more universal word for LLM
   priors; `say` is shorter and more human. Current pick: **`say`**, with
   `print` listed as a rejected alias so the diagnostic for `print` says
   "did you mean `say`?".
2. Map literal keys: require quotes (`{"name": …}`) or allow bare identifiers
   (`{name: …}`)? Bare is cheaper in tokens; quoted is JSON-compatible.
   Current pick: **bare allowed, quoted allowed**, both meaning string keys.
3. 0-based vs 1-based indexing. Current pick: **0-based** (matches the AI
   training prior of every mainstream language; 1-based would cause constant
   off-by-one generation errors).
