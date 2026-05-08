# Synapse Language Specification

> Version: 0.1.0  
> Status: Active Design  

This document is the **authoritative reference** for the Synapse scripting language grammar, syntax, type system, and execution semantics. When in doubt, this spec takes priority over any implementation.

---

## 1. Philosophy

Synapse is designed around a single guiding principle:

> **A script should read like instructions you would give to another person.**

Commands are structured as `<ACTION> <TARGET> [ATTRIBUTES];` — matching how humans naturally describe automation tasks (e.g., "Move mouse to 300, 400", "Press Enter", "Open Notepad").

Under the hood, these natural commands map to a strictly typed, Object-Oriented Abstract Syntax Tree.

---

## 2. Lexical Elements

### 2.1 Character Set
Synapse source files are UTF-8 encoded text. ASCII is a valid subset.

### 2.2 Whitespace
Whitespace (spaces, tabs, newlines) is non-significant between tokens. It serves only as a token separator.

### 2.3 Comments
```
# This is a single-line comment
```
There are no multi-line comments in the current specification. They are planned for v0.3.

### 2.4 Identifiers
Identifiers are used for variable names and function names.
```
identifier := [a-zA-Z_][a-zA-Z0-9_]*
```
**Examples:** `myVar`, `window_name`, `_count`, `MoveAndWait`

**Reserved keywords (cannot be used as identifiers):**
```
let  print  println  fn  return  if  else  loop  while  repeat  times
true  false  null  wait  and  or  not  in  is  as  to  from  at  into
MOUSE  KEY  WINDOW  SCREEN  APP  BUTTON
```

### 2.5 Literals

#### String Literals
Enclosed in double quotes. Supports basic escape sequences.
```
"Hello, World!"
"Line one\nLine two"
"Tab\there"
```

| Escape | Meaning       |
|--------|---------------|
| `\n`   | Newline        |
| `\t`   | Tab            |
| `\\`   | Backslash      |
| `\"`   | Double quote   |

#### Numeric Literals
```
42          # Integer
3.14        # Float
```

#### Time Literals
Time literals are a first-class value in Synapse (used in `WAIT`, `INTERVAL`).
```
1.5h    # 1.5 hours
30m     # 30 minutes
45s     # 45 seconds
500ms   # 500 milliseconds
```

#### Boolean Literals
```
true
false
```

#### Point Literal
Coordinates are expressed as a point pair:
```
(x, y)
```
**Example:** `(300, 400)`

---

## 3. Data Types

| Type      | Keyword  | Example Value     |
|-----------|----------|-------------------|
| Integer   | `INT`    | `42`              |
| Float     | `FLOAT`  | `3.14`            |
| String    | `STR`    | `"Hello"`         |
| Boolean   | `BOOL`   | `true` / `false`  |
| Time      | `TIME`   | `500ms`, `2s`     |
| Point     | `POINT`  | `(100, 300)`      |
| Null      | `NULL`   | `null`            |

### 3.1 Type Inference
Types are inferred at assignment. Explicit casting is supported:
```sql
let x = INT "42";       -- Cast string to int
let s = STR true;       -- Cast bool to string "true"
let b = BOOL 1;         -- Cast int to bool: 0=false, non-zero=true
```

---

## 4. Variables

Variables are declared with the `let` keyword. They are mutable.
```sql
let message = "Welcome to Synapse!";
let speed = 100;
let active = true;
let pos = (200, 300);
```

**Re-assignment** (no keyword needed after declaration):
```sql
let x = 5;
x = 10;
```

### 4.1 Compound Assignment Operators
```sql
x += 3;
x -= 3;
x *= 3;
x /= 3;
x %= 3;
x //= 3;   -- Floor division
x **= 3;   -- Power
```

---

## 5. Operators

### 5.1 Arithmetic
| Operator | Description       |
|----------|-------------------|
| `+`      | Addition / concat |
| `-`      | Subtraction       |
| `*`      | Multiplication    |
| `/`      | Division (float)  |
| `//`     | Floor division    |
| `%`      | Modulo            |
| `**`     | Exponentiation    |

### 5.2 Comparison
| Operator | Description        |
|----------|--------------------|
| `==`     | Equal              |
| `!=`     | Not equal          |
| `<`      | Less than          |
| `>`      | Greater than       |
| `<=`     | Less or equal      |
| `>=`     | Greater or equal   |
| `===`    | Strict type equal  |

### 5.3 Logical
| Operator | Description |
|----------|-------------|
| `AND`    | Logical AND |
| `OR`     | Logical OR  |
| `NOT`    | Logical NOT |

### 5.4 Bitwise
| Operator | Description     |
|----------|-----------------|
| `&`      | Bitwise AND     |
| `\|`     | Bitwise OR      |
| `^`      | Bitwise XOR     |
| `~`      | Bitwise NOT     |
| `>>`     | Right shift     |
| `<<`     | Left shift      |

### 5.5 Membership / Identity
| Operator  | Description                          |
|-----------|--------------------------------------|
| `IN`      | Check if value exists in collection  |
| `NOT IN`  | Negation of `IN`                     |
| `IS`      | Identity comparison                  |
| `IS NOT`  | Negation of `IS`                     |

---

## 6. Control Flow

### 6.1 Conditional — `if` / `else`
```sql
if (WINDOW "Calculator" EXISTS) {
    WINDOW FOCUS "Calculator";
} else {
    WINDOW OPEN "Calculator";
}
```

### 6.2 Repeat Loop
```sql
repeat 5 times {
    MOUSE CLICK LEFT AT (100, 200);
    WAIT 1s;
}
```

### 6.3 While Loop
```sql
loop while (active == true) {
    MOUSE MOVE TO (500, 500);
    WAIT 500ms;
}
```

### 6.4 Nested Blocks
All control flow blocks can be nested arbitrarily:
```sql
repeat 3 times {
    if (WINDOW "Editor" EXISTS) {
        repeat 2 times {
            KEY TYPE "Automated test.";
            KEY PRESS ENTER;
        }
    }
}
```

---

## 7. Functions

### 7.1 Defining Functions
```sql
fn MoveAndWait(x, y, waitTime) {
    MOUSE MOVE TO (x, y);
    WAIT waitTime;
}
```

### 7.2 Calling Functions
```sql
MoveAndWait(300, 400, 2s);
```

### 7.3 Return Values
```sql
fn Add(a, b) {
    return a + b;
}

let result = Add(10, 20);
println result;
```

---

## 8. Input / Output

### 8.1 Console Output
```sql
print "Hello";          -- Print without newline
println "Hello";        -- Print with newline
println "Value: " + x;  -- String concatenation
```

### 8.2 User Input
```sql
ASK "Enter your name:" INTO name;
ASK "Enter a number:" INTO num AS INT;
```
The `AS <TYPE>` clause is optional. If omitted, the value is stored as a string.

---

## 9. Automation Commands — Formal Grammar

### 9.1 Mouse Commands
All mouse commands are prefixed with `MOUSE`.

```sql
MOUSE MOVE TO (x, y);
MOUSE MOVE TO (x, y) SPEED speed;
MOUSE CLICK LEFT AT (x, y);
MOUSE CLICK RIGHT AT (x, y);
MOUSE CLICK MIDDLE AT (x, y);
MOUSE CLICK LEFT AT (x, y) TIMES n;
MOUSE DRAG FROM (x1, y1) TO (x2, y2);
MOUSE SCROLL UP n;
MOUSE SCROLL DOWN n;
MOUSE HOLD LEFT;
MOUSE HOLD RIGHT;
MOUSE RELEASE LEFT;
MOUSE RELEASE RIGHT;
```

### 9.2 Keyboard Commands
All keyboard commands are prefixed with `KEY`.

```sql
KEY PRESS ENTER;
KEY PRESS SPACE;
KEY PRESS "a";
KEY PRESS (CTRL + C);         -- Shortcut / combo
KEY PRESS (WIN + S);
KEY PRESS (ALT + F4);
KEY HOLD SHIFT;
KEY HOLD CTRL;
KEY RELEASE SHIFT;
KEY RELEASE CTRL;
KEY TYPE "Hello, World!";
```

### 9.3 Window Commands
All window commands are prefixed with `WINDOW`.

```sql
WINDOW OPEN "Notepad";
WINDOW CLOSE "Notepad";
WINDOW FOCUS "Notepad";
WINDOW MOVE "Notepad" TO (x, y);
WINDOW RESIZE "Notepad" TO (width, height);
WINDOW MINIMIZE "Notepad";
WINDOW MAXIMIZE "Notepad";
WINDOW RESTORE "Notepad";
WINDOW DRAG "Notepad" TO (x, y);
```

### 9.4 System / App Commands
```sql
APP OPEN "Firefox";
APP CLOSE "Firefox";
WAIT 2s;
WAIT duration;
```

### 9.5 Screen Commands
```sql
SCREEN CAPTURE INTO "screenshot.png";
SCREEN CAPTURE FROM (x1, y1) TO (x2, y2) INTO "partial.png";
```

### 9.6 Scheduling
```sql
RUN AT "10:45 AM" {
    APP OPEN "Browser";
}

INTERVAL 10m {
    SCREEN CAPTURE INTO "screenshot.png";
}
```

---

## 10. Error Handling

```sql
try {
    WINDOW FOCUS "Calculator";
} catch (err) {
    println "Error: " + err;
}
```

---

## 11. Statement Terminator
All statements **must** be terminated with a semicolon `;`.
Exception: The closing `}` of a block does **not** require a trailing `;`.

```sql
let x = 10;           -- correct

if (x > 5) {
    println "big";
}                     -- correct, no semicolon needed

let y = 20            -- INVALID: missing semicolon
```
