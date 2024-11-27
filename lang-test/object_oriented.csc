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
CLASS OpenURL {
    CONSTRUCTOR(self, url) {
	    OPEN APP "Browser";
	    WAIT 2s;
	    WRITE url;
	    PRESS KEY ENTER;
    }
};

INSTANTIATE OpenURL WITH ("https://openai.com");


CLASS TextEditor {
    OPEN APP "Editor"; -- will be executed on creating an instance without a constructor

    DEFFUN SaveFile(self) {
        PRESS KEY CTRL+S;
        WAIT 1s;
    };

    DEFFUN CloseEditor(self) {
        PRESS KEY ALT+F4;
    };
};

INSTANTIATE TextEditor AS editor;
editor.SaveFile();
editor.CloseEditor();


CLASS CalculatorAutomation {
    CONSTRUCTOR(self, operation, value1, value2) {
    	self.operation = operation;
    	self.value1 = value1;
    	self.value2 = value2;
    }


    OPEN APP "Calculator";
    WAIT 1s;

    DEFFUN PerformOperation(self) {
        WRITE self.value1;
        PRESS KEY self.operation;
        WRITE self.value2;
        PRESS KEY ENTER;
    };
};

INSTANTIATE CalculatorAutomation WITH ("+", 123, 456) AS calc;
calc.PerformOperation();


CLASS TextEditorAutomation {
    CONSTRUCTOR(self, filename) {
        PRINT "Opening file: " + filename;
        OPEN APP "TextEditor";
        WAIT 1s;
        WRITE filename;
        PRESS KEY ENTER;
    };

    DEFFUN SaveFile(self) {
        PRESS KEY CTRL+S;
    };

    DESTRUCTOR { -- if not defined, a default destructor is created
        PRINT "Closing TextEditor.";
        PRESS KEY ALT+F4;
    };
};

INSTANTIATE TextEditorAutomation("notes.txt") AS editor;
editor.SaveFile();
DESTROY editor;

