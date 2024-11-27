-- Nested conditions Break and Continue statement
IF TRUE THEN {
    BREAK;
}
ELSE IF FALSE {
    PASS;
}
ELSE {
    CONTINUE
}


-- complex example
IF (KEY SHIFT IS HELD) THEN {
    RELEASE KEY SHIFT;
}
ELSE IF (KEY CTRL IS HELD) THEN {
    RELEASE KEY CTRL;
}
ELSE {
    PRESS KEY ENTER;
}