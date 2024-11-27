-- Function Definition
DEFFUN getUserInfo() {
    INPUT "Enter your name: " TO userName;
    INPUT "Enter your age: " TO userAge;
    PRINTLN "Hello, " + userName + "! You are " + userAge + " years old.";
};

DEFFUN MoveAndWait(x, y, waitTime) {
    MOVE MOUSE TO (x, y);
    WAIT waitTime;
};

DEFFUN MoveAndWait_new_syntax(cordinates, waitTime) {
    MOVE MOUSE TO cordinates;
    WAIT waitTime;
};

-- Function Call
MoveAndWait(13, 20, 14s);

SET cordinates = POINT(400, 199);
MoveAndWait_new_syntax(cordinates, 15s);

getUserInfo();