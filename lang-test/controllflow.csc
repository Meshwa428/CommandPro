-- Conditional Logic
IF (WINDOW "Editor" EXISTS) THEN {
    FOCUS WINDOW "Editor";
    IF (WINDOW "VS" EXISTS) THEN {
        WRITE "Automated test.";
    };
}
ELSE IF (WINDOW "Code" EXISTS) THEN {
    PRINT "Test";
}

-- Loops
REPEAT 3 TIMES {
    PRESS KEY SPACE;
    WAIT 1s;
};

SET i = 2;
WHILE (i > 2) {
    OPEN APP Minecraft;
    i++;
}
