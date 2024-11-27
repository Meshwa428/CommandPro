-- Typed Input
INPUT "Enter your name: " INTO name AS STR;
INPUT "Enter a number: " INTO num AS INT;
INPUT "Enter a float: " INTO float_num AS FLOAT;

-- User Prompting
ASK "Enter filename:" INTO var;

-- Output
PRINTLN "Hello, " + name;
PRINT "\n\n\n\n";
PRINTLN message;

-- Screen Capture
CAPTURE SCREEN INTO "screenshot.png";
CAPTURE SCREEN FROM (100, 500) TO (1000, 1500) INTO "partial_screenshot.png";