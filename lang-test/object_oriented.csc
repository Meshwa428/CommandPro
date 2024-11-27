-- Simple browser Automation
CLASS BrowserAutomation {
	OPEN APP "browser";
	WAIT 2s;
	WRITE "https://example.com";
	PRESS KEY ENTER;
};

-- Creating instance of class
INSTANTIATE BrowserAutomation AS myBrowser;


-- Class with Parameters (constructors and destructors)
CLASS BrowserAutomation_1 {
	CONSTRUCTOR(filename) {
		PRINT filename;
	}

	DEFUN test_func_1(app_name, url) {
		OPEN APP app_name;
		WAIT 2s;
		WRITE url;
		PRESS KEY ENTER;
	}

	DESTRUCTOR {
		PRINT "Closing current window";
		PRESS KEY ALT+F4;
	}
};

