-- OCR support
IF READ SCREEN FROM (100, 400) TO (400, 100) CONTAINS "text to match" THEN {
	PRESS KEY "Y";
}


-- DETECT objects on screen (uses yolo+ OCR here)
OPEN APP "Calculator";
SET cords = DETECT OBJECT "7";
CLICK LEFT cords; -- click does both the work of move and press

