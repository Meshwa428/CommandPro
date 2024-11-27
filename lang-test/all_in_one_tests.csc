-- Test Case 1: Basic Mouse and Key Operations
PRINTLN "Test Case 1: Basic Mouse and Key Operations";
HOLD KEY SHIFT;
MOVE MOUSE TO (300, 400);
PRESS BUTTON LEFT;
WAIT 2s;
MOVE MOUSE TO (500, 600);
RELEASE KEY SHIFT; -- Release SHIFT key
PRINT "\n\n\n\n";





-- Test Case 2: Window Movement and Typing
PRINTLN "Test Case 2: Window Movement and Typing";
OPEN APP "Notepad";
WAIT 3s;
MOVE WINDOW "Notepad" TO (100, 100);
WRITE "This is a test.";
PRESS KEY ENTER;
WRITE "Another line.";
PRINT "\n\n\n\n";






-- Test Case 3: Repeating Actions
PRINTLN "Test Case 3: Repeating Actions";
REPEAT 3 TIMES {
    PRESS KEY SPACE;
    WAIT 1s;
};
PRINT "\n\n\n\n";





-- Test Case 4: Conditional Logic
PRINTLN "Test Case 4: Conditional Logic";
IF WINDOW "Calculator" EXISTS THEN
    FOCUS WINDOW "Calculator";
    MOVE WINDOW "Calculator" TO (150, 150);
END IF;
PRINT "\n\n\n\n";





-- Test Case 5: Scrolling and Holding Keys
PRINTLN "Test Case 5: Scrolling and Holding Keys";
SCROLL MOUSE DOWN 5;
WAIT 1s;
HOLD KEY CTRL;
SCROLL MOUSE UP 3;
RELEASE KEY CTRL; -- Release CTRL key
PRINT "\n\n\n\n";





-- Test Case 6: Opening, Closing, and Focusing Windows
PRINTLN "Test Case 6: Opening, Closing, and Focusing Windows";
OPEN APP "Calculator";
WAIT 2s;
FOCUS WINDOW "Calculator";
MOVE WINDOW "Calculator" TO (200, 200);
WAIT 1s;
CLOSE WINDOW "Calculator";
PRINT "\n\n\n\n";





-- Test Case 7: Typing a URL in a Browser
PRINTLN "Test Case 7: Typing a URL in a Browser";
OPEN APP "Browser";
WAIT 3s;
WRITE "https://example.com";
PRESS KEY ENTER;
PRINT "\n\n\n\n";





-- Test Case 8: Hold and Release Mouse Button
PRINTLN "Test Case 8: Hold and Release Mouse Button";
HOLD BUTTON LEFT;
MOVE MOUSE TO (100, 200);
MOVE MOUSE TO (400, 300);
RELEASE BUTTON LEFT; -- Release LEFT button
PRINT "\n\n\n\n";





-- Test Case 9: Complex Sequence of Window Operations
PRINTLN "Test Case 9: Complex Sequence of Window Operations";
OPEN APP "Terminal";
WAIT 2s;
MOVE WINDOW "Terminal" TO (50, 50);
FOCUS WINDOW "Terminal";
WRITE "ping example.com";
PRESS KEY ENTER;
WAIT 5s;
CLOSE WINDOW "Terminal";
PRINT "\n\n\n\n";





-- Test Case 10: Nested Conditional Logic and Repeating Actions
PRINTLN "Test Case 10: Nested Conditional Logic and Repeating Actions";
IF WINDOW "Editor" EXISTS THEN
    FOCUS WINDOW "Editor";
    REPEAT 2 TIMES {
        WRITE "Automated test.";
        PRESS KEY ENTER;
    };
END IF;
PRINT "\n\n\n\n";






-- Test Case 11: Nested Conditional Logic
PRINTLN "Test Case 11: Nested Conditional Logic";
IF WINDOW "Editor" EXISTS THEN
    FOCUS WINDOW "Editor";
    IF WINDOW "VS" EXISTS THEN {
        WRITE "Automated test.";
        PRESS KEY ENTER;
    };
END IF;
PRINT "\n\n\n\n";






-- Test Case 12: Nested Repeating Actions
PRINTLN "Test Case 12: Nested Repeating Actions";
REPEAT 4 TIMES {
    FOCUS WINDOW "Editor";
    REPEAT 2 TIMES {
        WRITE "Automated test.";
        PRESS KEY ENTER;
    };
};
PRINT "\n\n\n\n";



-- CLICK command
-- combines move and press
CLICK LEFT (100, 300);



-- Function definition
PRINTLN "Function definition";
DEFFUN MoveAndWait(x, y, waitTime) {
    MOVE MOUSE TO (x, y);
    WAIT waitTime;
};

-- Variable declarations
PRINTLN "Variable declarations";
SET myX = 200;
SET myY = 300;
SET myWaitTime = 2s;

-- Function call using variables
PRINTLN "Function call using variables";
MoveAndWait(myX, myY, myWaitTime);

IF WINDOW "notepad" EXISTS THEN
    MOVE MOUSE TO (myX, myY);
    WAIT 1s;
END IF;
PRINT "\n\n\n\n";





-- Direct time values
PRINTLN "Direct time values";
WAIT 1.5h;
WAIT 30m;
WAIT 45s;
WAIT 500ms;
PRINT "\n\n\n\n";




-- Time variables
PRINTLN "Time variables";
SET longWait = 1.5h;
SET shortWait = 500ms;
WAIT longWait;
WAIT shortWait;
PRINT "\n\n\n\n";




-- In functions
PRINTLN "In functions";
DEFFUN WaitAndMove(waitTime, x, y) {
    WAIT waitTime;
    MOVE MOUSE TO (x, y);
};


DEFFUN WaitAndMove(waitTime, x, y) {
    WAIT waitTime;
    MOVE MOUSE TO (x, y);
};

WaitAndMove(10s, 900, 500);
PRINT "\n\n\n\n";





-- Function to get user info and print it
DEFFUN getUserInfo() {
    INPUT "Enter your name: " TO userName;
    INPUT "Enter your age: " TO userAge;
    PRINTLN "Hello, " + userName + "! You are " + userAge + " years old.";
};

-- Variables for demonstration
SET message = "Welcome to CommandPro!";
PRINTLN message;

-- Function call to get user info
getUserInfo();

-- Example of type-specific input
PRINTLN "Example of type-specific input";
INPUT "Enter a number to multiply by 2: " TO num AS INT;
SET result = num * 2;
PRINTLN "Result: " + result;

-- Try different types
INPUT "Enter a floating point number: " TO float_num AS FLOAT;
PRINTLN "Float value: " + float_num;

INPUT "Enter your name: " TO name AS STR;
PRINTLN "Hello " + name;

-- Basic arithmetic with typed inputs
INPUT "Enter first number: " TO a AS INT;
INPUT "Enter second number: " TO b AS INT;
SET sum = a + b;
PRINTLN "Sum is: " + sum;
PRINT "\n\n\n\n";





-- Demonstrate mixing variables and strings
PRINTLN "Hello " + name + "! The count is " + count;


-- Capture screenshot/partial shots
CAPTURE SCREEN FROM (100, 500) TO (1000, 1500) INTO "screenshot.png"
CAPTURE SCREEN INTO "screenshot.png"

-- Drag and Drop
DRAG MOUSE FROM (10, 10) TO (10, 30);

-- Right / Left Click
MOUSE CLICK RIGHT AT (10, 20);
MOUSE CLICK LEFT AT (10, 20);


-- Clicking Multiple Times
MOUSE CLICK RIGHT TIMES 2 AT (20, 30);


-- Drag Window
DRAG WINDOW "window name" TO (10, 20)
DRAG WINDOW "window name" FROM (10, 30) TO (10, 20);


-- Resize Window
RESIZE WINDOW "window name" TO (width, height);


-- Window States (Maximize, Minimize, Restore)
MINIMIZE WINDOW "window name";
MAXIMIZE WINDOW "window name";
RESTORE WINDOW "window name";


-- While Loop
LOOP WHILE TRUE {
    OPEN APP Minecraft;
}



-- Scheduling Commands
RUN AT "10:45 AM" {
    OPEN APP "Bowser";
    WRITE "https://example.com";
}


-- Recurring Intervals:
INTTERVAL 10m {
    CAPTURE SCREEN INTO "screenshot.png";
}


-- Extended User Interaction Commands
-- Better Prompting (no type hint required)
ASK "Enter filename:" INTO var;
WRITE var;





