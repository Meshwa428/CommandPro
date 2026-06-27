# Design 006 — Full Language Feature Specification

Status: **approved (decisions locked)** · Scope: every language concept
See: 001-language.md for syntax reference

> Status legend: ✅ confirmed · 🔵 confirmed this session · ⬜ deferred post-1.0

---

## 1. Variables & Assignment ✅

```syn
let x = 5              # declaration
const PI = 3.14        # immutable binding — compile error on reassign ✅
x = 10                 # reassignment
x += 1                 # += -= *= /= //= %= **=
let a, b = 1, 2        # multi-assign
a, b = b, a            # swap
x ??= "default"        # assign if none
```

---

## 2. Data Types ✅

| Type | Literal | Status |
|---|---|---|
| Integer | `42` | ✅ |
| Float | `3.14` | ✅ |
| Bool | `true` `false` | ✅ |
| None | `none` | ✅ |
| String | `"hello"` | ✅ |
| Duration | `2s` `500ms` | ✅ |
| List | `[1, 2, 3]` | ✅ |
| Map | `{name: "x"}` | ✅ |
| Tuple | `(1, 2)` | ✅ |
| Set | `set([1,2,3])` | ✅ constructor form |
| Range | `0 to 9` | ✅ |

---

## 3. Operators ✅

### Arithmetic: `+  -  *  /  //  %  **`
### Comparison: `==  !=  <  <=  >  >=`  (== is strict, no coercion, no ===)
### Logical: `and  or  not`  (no && || !)
### Membership: `in  not in  is  is not`
### Null coalescing: `??`  and  `??=`
### Optional chaining: `obj?.key`  ✅
### Ternary: `a if cond else b`  ✅
### Chained comparisons: `0 < x < 100`  ✅
### Pipe: `xs |> filter(fn) |> map(fn) |> sum()`  ✅
### Bitwise: `& | ^ ~ << >>`  ⬜ deferred post-1.0

---

## 4. Strings ✅

```syn
"hello {name}"                  # interpolation
"2+2={2+2}"                     # expression
"brace: {{x}}"                  # escaped
r"C:\path\file"                 # raw string
"""
multiline
string
"""
```

String methods: `.length` `.upper()` `.lower()` `.trim()` `.trim_start()` `.trim_end()`
`.split(",")` `.join(list)` `.replace(a,b)` `.contains(x)` `.starts_with(x)`
`.ends_with(x)` `.find(x)` `.slice(1,5)` `.repeat(3)` `.pad_left(n)` `.pad_right(n)`

---

## 5. Control Flow ✅

```syn
if x > 0 { ... } else if x < 0 { ... } else { ... }

while n > 0 { n -= 1 }

repeat 5 { say "tick" }
repeat 5 as i { say "{i}" }

for item in items { ... }
for i in 0 to 9 { ... }
for i in 0 to 10 by 2 { ... }
for i in 9 to 0 by -1 { ... }
for k, v in mymap { ... }
for i, x in enumerate(items) { ... }
for a, b in zip(xs, ys) { ... }

break    continue
```

---

## 6. Match ✅

```syn
match value {
    case 1         { ... }
    case x if x>0  { ... }    # guard
    case int       { ... }    # type match
    case (x, y)    { ... }    # structure match
    else           { ... }
}

# As expression
let label = match x { case 1 { "one" } else { "other" } }
```

No fallthrough by default.

---

## 7. Functions ✅

```syn
fn name(a, b = default, *rest) { return value }

# Named arguments ✅
connect("host", port: 3000, timeout: 5s)

# Anonymous
let f = fn(x) { return x * 2 }

# No short lambda — fn(x) { return x*2 } is the one form ✅

# First-class, higher-order
fn apply(f, x) { return f(x) }

# Closures (upvalues)
fn counter() {
    let n = 0
    return fn() { n += 1; return n }
}
```

---

## 8. Error Handling ✅

```syn
try {
    result = parse(input)
} catch err {
    say err.message
} else {
    say "ok: {result}"     # runs only if NO exception ✅
} finally {
    cleanup()
}

throw error("message")
throw error("message", type: "io")

# Typed catch
catch err if err.type == "io" { ... }
catch err { ... }
```

Error properties: `.message` `.type` `.trace`

---

## 9. Collections ✅

### List

```syn
let xs = [1, 2, 3]
xs[0]    xs[-1]    xs[1 to 3]    xs.length
xs.push(x)  xs.pop()  xs.shift()  xs.unshift(x)
xs.insert(i,v)  xs.remove(i)
xs.contains(x)  xs.index_of(x)
xs.reverse()  xs.sort()  xs.sort(fn(a,b){return a-b})
xs.map(fn(x){return x*2})
xs.filter(fn(x){return x>0})
xs.reduce(fn(acc,x){return acc+x}, 0)
xs.find(fn(x){return x>0})
xs.any(fn(x){return x>0})
xs.all(fn(x){return x>0})
xs.flat()  xs.join(", ")  xs.copy()
[...xs, ...ys]    xs + ys    # spread / concat
```

### Comprehensions ✅ (AND method chains — both supported)

```syn
[x * 2 for x in items]
[x for x in items if x > 0]
{k: v*2 for k, v in mymap}
```

### Map

```syn
let m = {name: "mesh", age: 20}
m["name"]   m.name            # both work ✅
m.keys()  m.values()  m.items()
m.has(k)  m.delete(k)  m.length  m.merge(other)
```

### Tuple (immutable)

```syn
let t = (1, 2, 3)
t[0]    t.length
```

### Set

```syn
let s = set([1, 2, 3])
s.add(x)  s.remove(x)  s.contains(x)
s.union(other)  s.intersect(other)  s.difference(other)
```

### Range (lazy)

```syn
0 to 9         0 to 10 by 2        9 to 0 by -1
list(0 to 9)   # materialize
```

---

## 10. Destructuring ✅

```syn
let (x, y) = point
let (first, *rest) = items
let (_, y) = point           # _ = ignore

let {name, age} = user
let {name: username} = user  # rename

a, b = b, a                  # swap
```

---

## 11. Modules ✅

```syn
use math
use mouse, keyboard
use "file.syn" as f
use pkg
use math { sqrt, abs }       # selective
```

---

## 12. Built-in Functions ✅

```syn
# I/O
say "hello"    write "x"    let line = ask "prompt: "

# Math
abs  min  max  floor  ceil  round  sqrt  pow  log  sin  cos  tan  pi  e

# Type
type(x)  int(x)  float(x)  str(x)  bool(x)  list(x)

# Collections
len  sorted  reversed  sum  enumerate  zip  range
map  filter  reduce  any  all  flat

# Assert
assert x > 0, "message"
error("message")
error("message", type: "category")
```

---

## 13. Optional Chaining & Null Safety ✅

```syn
let city = user?.address?.city ?? "unknown"
x ??= "default"
```

---

## 14. Pipe Operator ✅

```syn
items
    |> filter(fn(x) { x > 0 })
    |> map(fn(x) { x * 2 })
    |> sum()
```

---

## 15. Explicitly Out of v2.0 ⬜

| Feature | Status |
|---|---|
| Classes / OOP | post-1.0 |
| Generics | post-1.0 |
| Async / await | post-1.0 |
| Decorators | post-1.0 |
| Operator overloading | post-1.0 |
| Macros | post-1.0 |
| Type annotations | post-1.0 |
| Bitwise operators | post-1.0 |
| Generators / yield | post-1.0 |
| Coroutines | post-1.0 |
| Regex literals | post-1.0 (use `regex.match(s, pattern)`) |

---

## 16. Decision Log — All Confirmed

| # | Feature | Decision |
|---|---|---|
| 1 | Constants | `const x = 5` — immutable binding |
| 2 | Ternary | `a if cond else b` (Python-suffix) |
| 3 | Map dot access | both `m["key"]` and `m.key` |
| 4 | try/catch/else | `else` runs only if no exception |
| 5 | Named arguments | `fn(port: 3000)` |
| 6 | Lambda | `fn(x) { return x * 2 }` — no short form |
| 7 | Pipe operator | `\|>` included |
| 8 | Comprehensions | both comprehensions AND method chains |
| 9 | Set literal | `set([...])` constructor |
| 10 | Bitwise | deferred post-1.0 |
| 11 | Optional chaining | `obj?.key` |
| 12 | Null coalescing | `??` and `??=` |
