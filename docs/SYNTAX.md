# Synapse Syntax Quick Reference

> A cheat-sheet for writing `.syn` scripts. For the full formal specification, see [LANGUAGE_SPEC.md](./LANGUAGE_SPEC.md).

---

## Variables
```sql
let name = "Synapse";
let count = 10;
let active = true;
let pos = (200, 300);
```

## Output
```sql
print "no newline";
println "with newline";
println "Hello " + name + "!";
```

## Input
```sql
ASK "Enter name:" INTO name;
ASK "Enter number:" INTO n AS INT;
```

## Arithmetic
```sql
let result = (10 + 5) * 2 / 3;
let power  = 2 ** 8;
let floor  = 10 // 3;
let mod    = 10 % 3;
```

## Comparisons
```sql
x == y     x != y
x < y      x > y
x <= y     x >= y
x === y    # strict type equality
```

## Logical
```sql
x AND y
x OR y
NOT x
```

---

## Mouse
```sql
MOUSE MOVE TO (300, 400);
MOUSE CLICK LEFT AT (100, 200);
MOUSE CLICK RIGHT AT (100, 200);
MOUSE CLICK LEFT AT (100, 200) TIMES 2;
MOUSE DRAG FROM (10, 10) TO (400, 300);
MOUSE SCROLL UP 3;
MOUSE SCROLL DOWN 5;
MOUSE HOLD LEFT;
MOUSE RELEASE LEFT;
```

## Keyboard
```sql
KEY PRESS ENTER;
KEY PRESS SPACE;
KEY PRESS "a";
KEY PRESS (CTRL + C);
KEY PRESS (WIN + S);
KEY PRESS (ALT + F4);
KEY HOLD SHIFT;
KEY RELEASE SHIFT;
KEY TYPE "Hello, World!";
```

## Window
```sql
WINDOW OPEN "Notepad";
WINDOW CLOSE "Notepad";
WINDOW FOCUS "Notepad";
WINDOW MOVE "Notepad" TO (100, 100);
WINDOW RESIZE "Notepad" TO (800, 600);
WINDOW MINIMIZE "Notepad";
WINDOW MAXIMIZE "Notepad";
WINDOW RESTORE "Notepad";
```

## Apps & System
```sql
APP OPEN "Firefox";
APP CLOSE "Firefox";
WAIT 2s;
WAIT 500ms;
```

## Screen
```sql
SCREEN CAPTURE INTO "shot.png";
SCREEN CAPTURE FROM (0, 0) TO (800, 600) INTO "region.png";
```

---

## If / Else
```sql
if (x > 10) {
    println "big";
} else {
    println "small";
}

if (WINDOW "Calc" EXISTS) {
    WINDOW FOCUS "Calc";
}
```

## Repeat Loop
```sql
repeat 5 times {
    KEY PRESS ENTER;
    WAIT 1s;
}
```

## While Loop
```sql
loop while (count > 0) {
    println count;
    count -= 1;
}
```

---

## Functions
```sql
fn greet(name) {
    println "Hello, " + name + "!";
}

fn add(a, b) {
    return a + b;
}

greet("Synapse");
let sum = add(10, 20);
```

---

## Error Handling
```sql
try {
    WINDOW FOCUS "SomeApp";
} catch (err) {
    println "Caught: " + err;
}
```

## Scheduling
```sql
RUN AT "09:00 AM" {
    APP OPEN "Browser";
}

INTERVAL 30m {
    SCREEN CAPTURE INTO "backup.png";
}
```

---

## Type Casting
```sql
let n = INT "42";
let s = STR 100;
let b = BOOL 0;
```

## Time Literals
```
500ms  →  500 milliseconds
2s     →  2 seconds
5m     →  5 minutes
1.5h   →  1.5 hours
```

## Comments
```sql
# This is a comment
let x = 10;  # inline comment
```
