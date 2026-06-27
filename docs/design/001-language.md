# Design 001 — Language Design

Status: **approved** · Scope: full surface syntax + semantics
Supersedes: draft version from initial v2 planning

---

## 1. Core Principle — Caveman Efficiency

> A caveman sentence strips all filler and keeps only load-bearing words.
> "Me want food" → 3 tokens. "I would like some food please" → 7 tokens.
> Same meaning. 57% fewer tokens. That ratio is the design target.

Synapse's primary author is an LLM. Every syntax decision is measured in
BPE token cost. Lowercase English words ≈ 1 token each. UPPERCASE words
cost 2–3 tokens. Prepositions (to, at, from, into) add tokens with zero
semantic gain when argument position already encodes meaning.

**Result:** same expressive power as Python+pyautogui, 2–3× cheaper in tokens.

---

## 2. Design Rules

1. **Lowercase everything.** Keywords, commands, key names.
2. **Newline ends a statement.** No semicolons. `;` only to put two statements on one line.
3. **One spelling per concept.** Rejected aliases live in §15.
4. **Words for prose, symbols for math.** `and or not`; `+ - * /`. No `&&/||/!`.
5. **No silent coercion.** `"a" + 1` is a compile error. Use `"a{1}"`.
6. **Everything is an expression call underneath.** Command statements are parser sugar over stdlib calls.

---

## 3. Variables & Assignment

```syn
let name = "Synapse"
let count = 10
let speed = 1.5
let ready = true
let nothing = none
let pos = (300, 400)

const PI = 3.14159        # immutable — compile error on reassign

count = count + 1
count += 1                # += -= *= /= //= %= **=

let a, b = 1, 2           # multi-assign
a, b = b, a               # swap

x ??= "default"           # assign only if x is none
```

---

## 4. Data Types

| Type | Literal | Notes |
|---|---|---|
| Integer | `42` `-7` | 64-bit signed |
| Float | `3.14` `-0.5` | 64-bit double |
| Bool | `true` `false` | lowercase only |
| None | `none` | |
| String | `"hello"` | always double-quoted |
| Duration | `2s` `500ms` `1m` `0.5h` | first-class type |

### Strings

```syn
say "hello {name}"
say "2 + 2 = {2 + 2}"
say "literal brace {{ok}}"
let path = r"C:\Users\mesh\file"    # raw string
let msg = """
  multiline
  string
"""
```

### Type conversion

```syn
int("42")    float(3)    str(99)    bool(0)    list((1,2))
```

### Type checking

```syn
type(x)         # "int" | "float" | "string" | "bool" | "none" | "list" | "map" | "tuple"
x is int        # → bool
x is not none   # → bool
```

---

## 5. Operators

### Arithmetic: `+  -  *  /  //  %  **`
- `/` always float result. `//` floor division (int result).

### Comparison: `==  !=  <  <=  >  >=`
- `==` is strict (no coercion). No `===`.

### Logical: `and   or   not` (short-circuit, no `&&  ||  !`)

### Membership: `in   not in   is   is not`

### Null coalescing: `x ?? default`

### Ternary: `let label = "big" if score > 90 else "small"`

### Chained comparisons: `if 0 < x < 100 { ... }`

### Pipe operator:
```syn
items
    |> filter(fn(x) { x > 0 })
    |> map(fn(x) { x * 2 })
    |> sum()
```

### Operator precedence (high → low):
```
**
unary -  not
*  /  //  %
+  -
==  !=  <  <=  >  >=  in  not in  is  is not
and
or
??
|>
```

---

## 6. Control Flow

### If / else if / else

```syn
if score >= 90 {
    say "A"
} else if score >= 75 {
    say "B"
} else {
    say "C"
}
```

No parentheses around conditions. No `elif`.

### While

```syn
while n > 0 { n -= 1 }
```

### Repeat

```syn
repeat 5 { say "tick" }
repeat 5 as i { say "tick {i}" }    # 0-based index
```

### For-in

```syn
for item in items { say item }
for i in 0 to 9 { say i }
for i in 0 to 10 by 2 { say i }
for i in 9 to 0 by -1 { say i }
for k, v in mymap { say "{k}: {v}" }
for i, x in enumerate(items) { say "{i}: {x}" }
for a, b in zip(xs, ys) { say "{a} {b}" }
```

### Break / Continue

```syn
while true {
    if done { break }
    if skip { continue }
}
```

---

## 7. Match

```syn
match status {
    case 200 { say "ok" }
    case 404 { say "not found" }
    else     { say "unknown {status}" }
}

# Guard clause
match score {
    case x if x >= 90 { say "A" }
    case x if x >= 75 { say "B" }
    else              { say "F" }
}

# Match on type
match value {
    case int    { say "int: {value}" }
    case string { say "string: {value}" }
    else        { say "other" }
}

# Match on structure
match point {
    case (0, 0)  { say "origin" }
    case (x, 0)  { say "on x-axis at {x}" }
    case (x, y)  { say "at {x}, {y}" }
}

# Match as expression
let label = match status {
    case 200 { "ok" }
    case 404 { "not found" }
    else     { "error" }
}
```

No fallthrough by default. Match arms implicitly return last expression.

---

## 8. Functions

```syn
fn add(x, y) { return x + y }

fn greet(who = "world") { say "hello, {who}!" }

fn connect(host, port = 8080, timeout = 30s) { }
connect("localhost", port: 3000, timeout: 5s)    # named args

fn sum(*nums) {
    let total = 0
    for n in nums { total += n }
    return total
}

let double = fn(x) { return x * 2 }    # anonymous / closure

fn apply(f, value) { return f(value) }
apply(double, 5)    # → 10

fn min_max(items) { return (min(items), max(items)) }
let (lo, hi) = min_max([3, 1, 4, 1, 5])

fn make_counter() {
    let n = 0
    return fn() { n += 1; return n }    # closure over n
}
```

No short lambda syntax. One form: `fn(x) { return x * 2 }`.

---

## 9. Error Handling

```syn
try {
    result = parse(input)
} catch err {
    say "failed: {err.message}"
} else {
    say "ok: {result}"      # runs only if NO exception thrown
} finally {
    cleanup()               # always runs
}

throw error("bad input: {x}")
throw error("file not found", type: "io")

# Typed catch
try { risky() }
catch err if err.type == "io" { say "IO error" }
catch err                     { say "other: {err.message}" }
```

Error properties: `.message` `.type` `.trace`

---

## 10. Collections

### List

```syn
let xs = [1, 2, 3]
xs[0]           xs[-1]          xs[1 to 3]
xs.length       xs.push(4)      xs.pop()        xs.shift()
xs.unshift(0)   xs.insert(1,99) xs.remove(1)
xs.contains(2)  xs.index_of(2)  xs.reverse()
xs.sort()       xs.sort(fn(a,b){ return a-b })
xs.map(fn(x){ return x*2 })
xs.filter(fn(x){ return x>2 })
xs.reduce(fn(acc,x){ return acc+x }, 0)
xs.find(fn(x){ return x>2 })
xs.any(fn(x){ return x>2 })    xs.all(fn(x){ return x>0 })
xs.flat()       xs.join(", ")   xs.copy()

let combined = [...xs, ...ys]    # spread
let combined = xs + ys           # same
```

### Comprehensions — AND method chains (both supported)

```syn
let doubled = [x * 2 for x in items]
let evens   = [x for x in items if x % 2 == 0]
let squares = {x: x**2 for x in 0 to 9}

# Identical method-chain forms:
let doubled = items.map(fn(x) { return x * 2 })
let evens   = items.filter(fn(x) { return x % 2 == 0 })
```

### Map

```syn
let user = {name: "mesh", age: 20}    # bare keys (strings internally)
user["name"]    user.name             # both work
user["name"] = "meshwa"
user.keys()     user.values()    user.items()
user.has("name")    user.delete("age")
user.length         user.merge(other)
```

### Tuple — immutable

```syn
let point = (300, 400)
point[0]    point.length
```

### Set

```syn
let s = set([1, 2, 3])
s.add(4)    s.remove(2)    s.contains(3)
s.union(other)    s.intersect(other)    s.difference(other)
```

### Range — lazy

```syn
0 to 9             # inclusive
0 to 10 by 2
9 to 0 by -1
list(0 to 9)       # materialize
```

---

## 11. Destructuring

```syn
let (x, y) = (300, 400)
let (first, *rest) = items
let (_, y) = point

let {name, age} = user
let {name: username} = user    # rename

a, b = b, a    # swap
```

---

## 12. Optional Chaining & Null Safety

```syn
let city = user?.address?.city ?? "unknown"
```

---

## 13. Modules

```syn
use math
use mouse, keyboard
use "helpers.syn" as helpers
use spotify_flows
use math { sqrt, abs }           # selective
```

---

## 14. Built-in Functions

```syn
say "hello"    write "x"    let line = ask "Name: "

abs(-5)    min(a,b)    max(a,b)    floor(x)    ceil(x)    round(x)
sqrt(x)    pow(x,n)   log(x)      sin(x)      cos(x)     tan(x)
pi    e

type(x)    int(x)    float(x)    str(x)    bool(x)    list(x)

len(x)     sorted(xs)     reversed(xs)    sum(xs)
enumerate(xs)    zip(xs,ys)    range(n)
map(fn,xs)    filter(fn,xs)    reduce(fn,xs,init)
any(xs)    all(xs)    flat(xs)

assert x > 0, "message"
error("message")
error("message", type: "io")
```

---

## 15. Automation Commands

Command statements desugar to stdlib calls — both forms always legal.

### Mouse (RAT model — human-like trajectories)

```syn
mouse 300, 400              # mouse.move(300, 400) — RAT natural speed
mouse 300, 400, 0.5         # speed multiplier (0.5=slow, 2.0=fast)

click                       # left click at current pos
click right 100, 200
click 100, 200, 3           # triple-click (n ≥ 1)
drag 10, 10, 400, 300       # RAT-path drag
drag 10, 10, 400, 300, 0.7  # with speed

scroll up 3    scroll down 5    scroll left 2    scroll right 2
hold left      release left
hold shift     release shift
```

### Keyboard

```syn
press enter    press ctrl+c    press alt+f4    press win+s
hold shift     release shift
type "Hello, World!"
```

Key names: always lowercase. One canonical name per key. Chords: `mod+key`.

### Windows & Apps

```syn
run "firefox"              # launch app
open "report.pdf"          # open file (OS default handler)
close "firefox"
focus "Notepad"
move "Notepad" 100, 100    # teleport window — string first = always window
resize "Notepad" 800, 600
maximize "Notepad"
minimize "Notepad"
```

### Screen & System

```syn
capture "shot.png"
capture 0, 0, 1920, 1080 "region.png"
wait 2s    wait 500ms    wait 1m
if exists "Calc" { focus "Calc" }
```

---

## 16. Rejected Aliases

| Rejected | Use instead | Reason |
|---|---|---|
| `MOUSE MOVE TO` | `mouse x, y` | UPPERCASE + preposition |
| `KEY PRESS` | `press key` | UPPERCASE subject |
| `KEY TYPE` | `type "text"` | UPPERCASE subject |
| `APP OPEN` | `run "app"` | `open` reserved for files |
| `WINDOW MAXIMIZE` | `maximize "name"` | UPPERCASE |
| `WINDOW MINIMIZE` | `minimize "name"` | UPPERCASE |
| `loop while` | `while` | redundant verb |
| `repeat N times` | `repeat N` | `times` is filler |
| `INTO` `AS` | positional args | SQL-ism |
| `===` | `==` | |
| `&&` `\|\|` `!` | `and or not` | dual operators |
| `println` `print` | `say` | |
| `null` `nil` | `none` | |
| `elif` | `else if` | |

---

## 17. Decision Log

| # | Decision | Chosen |
|---|---|---|
| 1 | Statement end | newline |
| 2 | Condition parens | none |
| 3 | Mouse move verb | `mouse x, y` |
| 4 | Window move verb | `move "name" x, y` |
| 5 | App launch | `run "app"` |
| 6 | File open | `open "file"` |
| 7 | Coordinate format | `x, y` with comma |
| 8 | Click count | `click x, y, n` where n ≥ 1 |
| 9 | Window max/min | `maximize` / `minimize` |
| 10 | Repeat | `repeat 5 { }` / `repeat 5 as i { }` |
| 11 | Constants | `const x = 5` |
| 12 | Ternary | `a if cond else b` |
| 13 | Map dot access | both `m["key"]` and `m.key` |
| 14 | try/catch/else | included |
| 15 | Named arguments | `fn(port: 3000)` |
| 16 | Lambda | `fn(x) { return x * 2 }` only |
| 17 | Pipe operator | `\|>` included |
| 18 | Comprehensions | both comprehensions and method chains |
| 19 | Bitwise ops | deferred post-1.0 |
| 20 | Set literal | `set([...])` constructor |
| 21 | RAT mouse model | human-like, optional speed multiplier |
