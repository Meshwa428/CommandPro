# Synapse Standard Library Reference

> This document covers all built-in commands available in Synapse scripts. These are not user-defined functions but language-level built-ins backed directly by the OS Abstraction Layer.

---

## Mouse Commands

### `MOUSE MOVE TO (x, y)`
Moves the mouse cursor to the absolute screen coordinates `(x, y)`.
```sql
MOUSE MOVE TO (300, 400);

let targetX = 500;
let targetY = 250;
MOUSE MOVE TO (targetX, targetY);
```

---

### `MOUSE CLICK <button> AT (x, y) [TIMES n]`
Clicks the specified mouse button at the given coordinates.

- **button:** `LEFT`, `RIGHT`, `MIDDLE`
- **TIMES n:** optional repeat count (default: 1)

```sql
MOUSE CLICK LEFT AT (100, 200);
MOUSE CLICK RIGHT AT (100, 200);
MOUSE CLICK LEFT AT (100, 200) TIMES 2;   # double-click
MOUSE CLICK LEFT AT (100, 200) TIMES 3;   # triple-click
```

---

### `MOUSE DRAG FROM (x1, y1) TO (x2, y2)`
Holds the left mouse button, moves from `(x1, y1)` to `(x2, y2)`, then releases.
```sql
MOUSE DRAG FROM (50, 50) TO (300, 300);
```

---

### `MOUSE SCROLL UP n` / `MOUSE SCROLL DOWN n`
Scrolls the mouse wheel by `n` units.
```sql
MOUSE SCROLL UP 3;
MOUSE SCROLL DOWN 5;
```

---

### `MOUSE HOLD <button>` / `MOUSE RELEASE <button>`
Holds or releases a mouse button without moving.
- **button:** `LEFT`, `RIGHT`, `MIDDLE`

```sql
MOUSE HOLD LEFT;
MOUSE MOVE TO (400, 300);  # drag effect
MOUSE RELEASE LEFT;
```

---

## Keyboard Commands

### `KEY PRESS <key>`
Simulates a single key press and release. Key names are case-insensitive for named keys.

**Named Keys:**
`ENTER`, `SPACE`, `TAB`, `BACKSPACE`, `DELETE`, `ESCAPE`, `INSERT`,
`UP`, `DOWN`, `LEFT`, `RIGHT`, `HOME`, `END`, `PAGEUP`, `PAGEDOWN`,
`F1`...`F12`, `PRINTSCREEN`, `CAPSLOCK`, `NUMLOCK`, `SCROLLLOCK`

**Modifier Keys:** `CTRL`, `SHIFT`, `ALT`, `WIN` (Windows key / Super key)

```sql
KEY PRESS ENTER;
KEY PRESS SPACE;
KEY PRESS F5;
KEY PRESS "a";        # single character
KEY PRESS "5";        # digit
```

---

### `KEY PRESS (<combo>)`
Simulates a keyboard shortcut using `+` to combine keys.
```sql
KEY PRESS (CTRL + C);        # copy
KEY PRESS (CTRL + V);        # paste
KEY PRESS (CTRL + Z);        # undo
KEY PRESS (ALT + F4);        # close window
KEY PRESS (WIN + S);         # Windows search / Super+S
KEY PRESS (CTRL + SHIFT + T); # re-open tab
```

---

### `KEY HOLD <key>` / `KEY RELEASE <key>`
Holds a key down until explicitly released.
```sql
KEY HOLD SHIFT;
MOUSE CLICK LEFT AT (100, 200);  # shift-click
KEY RELEASE SHIFT;
```

---

### `KEY TYPE "<text>"`
Types a string of text as if typed on a keyboard. Supports Unicode characters.
```sql
KEY TYPE "Hello, World!";
KEY TYPE "https://example.com";
KEY TYPE "Username";
```

---

## Window Commands

### `WINDOW OPEN "<name>"`
Opens an application by its name. Behavior is OS-dependent.
```sql
WINDOW OPEN "Notepad";
WINDOW OPEN "Firefox";
```
> **Note:** Identical to `APP OPEN`. Both are valid.

---

### `WINDOW CLOSE "<name>"`
Sends a close signal to the named window.
```sql
WINDOW CLOSE "Calculator";
```

---

### `WINDOW FOCUS "<name>"`
Brings the named window to the foreground and gives it focus.
```sql
WINDOW FOCUS "Terminal";
```

---

### `WINDOW MOVE "<name>" TO (x, y)`
Moves the named window's top-left corner to `(x, y)`.
```sql
WINDOW MOVE "Notepad" TO (100, 100);
```

---

### `WINDOW RESIZE "<name>" TO (width, height)`
Resizes the named window.
```sql
WINDOW RESIZE "Notepad" TO (800, 600);
```

---

### `WINDOW MINIMIZE "<name>"`
Minimizes the named window.
```sql
WINDOW MINIMIZE "Browser";
```

---

### `WINDOW MAXIMIZE "<name>"`
Maximizes the named window.
```sql
WINDOW MAXIMIZE "Terminal";
```

---

### `WINDOW RESTORE "<name>"`
Restores a minimized or maximized window to its previous state.
```sql
WINDOW RESTORE "Notepad";
```

---

## App Commands

### `APP OPEN "<name>"`
Opens an application by its display name or executable name.
```sql
APP OPEN "Firefox";
APP OPEN "notepad.exe";   # Windows
APP OPEN "gedit";         # Linux
```

### `APP CLOSE "<name>"`
Closes an application.
```sql
APP CLOSE "Firefox";
```

---

## Screen Commands

### `SCREEN CAPTURE INTO "<path>"`
Captures the entire screen and saves it to the specified file path.
```sql
SCREEN CAPTURE INTO "screenshot.png";
SCREEN CAPTURE INTO "/home/user/screens/shot.png";
```

### `SCREEN CAPTURE FROM (x1, y1) TO (x2, y2) INTO "<path>"`
Captures a specific screen region.
```sql
SCREEN CAPTURE FROM (0, 0) TO (1920, 1080) INTO "full.png";
SCREEN CAPTURE FROM (100, 100) TO (500, 400) INTO "region.png";
```

---

## Timing

### `WAIT <duration>`
Pauses script execution for the given time duration.
```sql
WAIT 2s;
WAIT 500ms;
WAIT 1.5h;
WAIT 30m;

let delay = 3s;
WAIT delay;
```

---

## Scheduling

### `RUN AT "<time>" { ... }`
Executes a block once at the specified wall-clock time.
```sql
RUN AT "09:00 AM" {
    APP OPEN "Browser";
    KEY TYPE "https://example.com";
    KEY PRESS ENTER;
}
```

### `INTERVAL <duration> { ... }`
Repeatedly executes a block at the specified interval, running indefinitely until the script is stopped.
```sql
INTERVAL 30m {
    SCREEN CAPTURE INTO "backup.png";
}
```

---

## I/O

### `print <value>`
Prints a value to standard output without a trailing newline.
```sql
print "Hello";
print count;
```

### `println <value>`
Prints a value to standard output with a trailing newline.
```sql
println "Hello, World!";
println "Count: " + count;
```

### `ASK "<prompt>" INTO <var> [AS <type>]`
Prompts the user for input and stores it in a variable.
```sql
ASK "Enter your name:" INTO name;
ASK "Enter a number:" INTO n AS INT;
ASK "Enter a decimal:" INTO d AS FLOAT;
```

---

## Conditions

### `WINDOW "<name>" EXISTS`
Returns `true` if a window with the given name is currently open.

Used inside `if` conditions:
```sql
if (WINDOW "Calculator" EXISTS) {
    WINDOW FOCUS "Calculator";
}
```
