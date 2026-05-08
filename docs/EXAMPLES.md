# Synapse — Example Scripts

This file demonstrates the full range of Synapse features through practical, real-world automation examples.

---

## Example 1: Open a Browser and Search

```sql
# Open Firefox and search for something
APP OPEN "Firefox";
WAIT 3s;
KEY PRESS (CTRL + L);     # Focus the address bar
KEY TYPE "https://google.com";
KEY PRESS ENTER;
WAIT 2s;
KEY TYPE "Synapse automation language";
KEY PRESS ENTER;
```

---

## Example 2: Screenshot Every 30 Minutes

```sql
# Recurring screenshot backup
INTERVAL 30m {
    SCREEN CAPTURE INTO "backup.png";
}
```

---

## Example 3: Type in Notepad

```sql
APP OPEN "Notepad";
WAIT 2s;
WINDOW FOCUS "Notepad";
KEY TYPE "Hello from Synapse!";
KEY PRESS ENTER;
KEY TYPE "Automation made simple.";
```

---

## Example 4: Drag and Drop

```sql
# Click and drag a file from one location to another
MOUSE MOVE TO (100, 200);
MOUSE HOLD LEFT;
WAIT 200ms;
MOUSE MOVE TO (500, 400);
MOUSE RELEASE LEFT;
```

---

## Example 5: Variables and Functions

```sql
fn launchAndType(appName, text) {
    APP OPEN appName;
    WAIT 2s;
    WINDOW FOCUS appName;
    KEY TYPE text;
    KEY PRESS ENTER;
}

let app = "Notepad";
let message = "Synapse is working!";

launchAndType(app, message);
```

---

## Example 6: Conditional Window Check

```sql
if (WINDOW "Calculator" EXISTS) {
    WINDOW FOCUS "Calculator";
    println "Calculator is open, focusing it.";
} else {
    APP OPEN "Calculator";
    WAIT 2s;
    println "Calculator was not open, launched it.";
}
```

---

## Example 7: Repeat and Loop

```sql
# Repeat clicking 5 times with a wait
repeat 5 times {
    MOUSE CLICK LEFT AT (300, 400);
    WAIT 500ms;
}

# Loop while user variable is active
let running = true;
let count = 0;

loop while (count < 10) {
    println "Iteration: " + count;
    count += 1;
    WAIT 1s;
}
```

---

## Example 8: User Input

```sql
ASK "Enter the application name:" INTO appName;
ASK "Enter how many times to click:" INTO clicks AS INT;

APP OPEN appName;
WAIT 2s;

repeat clicks times {
    MOUSE CLICK LEFT AT (960, 540);
    WAIT 300ms;
}
```

---

## Example 9: Error Handling

```sql
try {
    WINDOW FOCUS "SomeApp";
    KEY TYPE "Hello!";
} catch (err) {
    println "Something went wrong: " + err;
    println "Make sure SomeApp is running.";
}
```

---

## Example 10: Scheduled Morning Routine

```sql
# Run a morning automation at 9:00 AM
RUN AT "09:00 AM" {
    APP OPEN "Firefox";
    WAIT 3s;
    KEY PRESS (CTRL + L);
    KEY TYPE "https://news.ycombinator.com";
    KEY PRESS ENTER;
    
    APP OPEN "Terminal";
    WAIT 1s;
    KEY TYPE "git pull";
    KEY PRESS ENTER;
}
```
