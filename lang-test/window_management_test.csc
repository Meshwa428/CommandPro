# Window Operations
OPEN APP "Notepad";
CLOSE WINDOW "Calculator";
FOCUS WINDOW "Terminal";

# Window Positioning
MOVE WINDOW "Notepad" TO (100, 100);
DRAG WINDOW "window name" TO (10, 20);
RESIZE WINDOW "window name" TO (width, height);

# Window States
MINIMIZE WINDOW "window name";
MAXIMIZE WINDOW "window name";
RESTORE WINDOW "window name";

# Window Existence Check
IF WINDOW "Calculator" EXISTS THEN {
    # Do something
    PRINT "Test";
}
