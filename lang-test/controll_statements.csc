DEFUN test_func(numbers) {
	TRY {
		FOR num IN numbers {
			IF (num < 0) {
				PRINTLN f"skipping negative number: {$num}";
				CONTINUE;
			}
			IF num == 0 {
				RAISE ERROR("Zero is not allowed!");
			}
			IF num > 10 {
				PRINTLN f"Breaking on number greater than 10: {num}";
			}
			YIELD num * 2;
		}
	}
	EXCEPT ERROR AS e {
		PRINTLN f"Error encountered: {e}";
		RETURN "error occured";
	}
	FINALLY {
		PRINTLN "Cleaning up resources...";
	}
}

SET numbers = LIST(3, -1, 0, 5, 12, 7);
-- or we can do this
-- SET numbers = [3, -1, 0, 5, 12, 7];
PRINTLN "Output Values:";
FOR result in test_func(numbers) {
	PRINTLN result;
}