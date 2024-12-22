# OCR support
IF READ SCREEN FROM (100, 400) TO (400, 100) CONTAINS "text to match" THEN {
	PRESS KEY "Y";
}
SET ocr_test = READ SCREEN FROM (100, 1000) TO (1000, 100);
IF ocr_test CONTAINS "text" THEN {
	PRINT "text found";
}
ELSE IF ocr_test CONTAINS "abc" THEN {
	PRINT "abc found";
}
ELSE {
	PRINT "Nothing found";
}

# DETECT objects on screen (uses yolo+ OCR here)
OPEN APP "Calculator";
SET cords = DETECT OBJECT "7";
CLICK LEFT cords; # click does both the work of MOVE and PRESS



