# Contains command
SET test = "This is string";
PRINT test CONTAINS "string";

# IN command (same as contains but is generalized)
PRINT "string" IN test;


# represents coordinates in (x, y) format
PRINT POINT(123, 123);


# represents color
PRINT COLOR(255, 0, 0); # RGB color
PRINT COLOR("#FF5733"); # HEX color


# represents list/array
PRINT LIST(1, 2, 3, 4, 5);
SET var = [1, 2, 3, 4, 5];


# represents range of numbers (returns a generator) (start, stop, step)
PRINT RANGE(1, 20);
PRINT RANGE(1, 20, 3);


# represents filepath
FILEPATH("path/to/file");


# date and time
DATE("2024-11-27"); # yyyy-mm-dd
TIME("14:30:00"); # 24hr format
TIME("12:11:00 AM") # 12hr format


# enumerator
ENUM("LEFT", "CENTER", "RIGHT");


# defining vars with datatype manually
INT integerValue = 10; # integer
FLOAT pi = 3.14159; # float (normally 32 bit precision)
DOUBLE largeNumber = 1.23456789012; # float with 64 bit precision
LONG bigValue = 3287467823462738167826; # large integer values
SHORT smallValue = 32767; # small integer (limited range)
BYTE flag = 255; # useful for memory efficient operations
BOOLEAN isActive = FALSE; # boolean datatype

COMPLEX complex_value = 3 + 4i; # complex datatype
FIXED INT constantInteger = 123; # constant datatype, cannot be changed
POINT cordinate = (123, 456); # cordinates datatype

# unsigned variables
UNSIGNED INT var = 10;
UNSIGNED FLOAT pi = 3.14159;
UNSIGNED DOUBLE largeNumber = 1.23456789012;
UNSIGNED LONG bigValue = 3287467823462738167826;
UNSIGNED SHORT smallValue = 32767;
UNSIGNED BYTE flag = 255;

# signed variables
SIGNED INT var = 10;
SIGNED FLOAT pi = 3.14159;
SIGNED DOUBLE largeNumber = 1.23456789012;
SIGNED LONG bigValue = 3287467823462738167826;
SIGNED SHORT smallValue = 32767;
SIGNED BYTE flag = 255;


# getting Datatype of Variable
PRINT TYPE integerValue; # should print boolean data type
PRINT TYPE pi; # should print float
PRINT TYPE (111, 100); # should print cordinates data type 
PRINT TYPE "this is string"; # should print string datatype
PRINT TYPE 45m; # should print time datatype
PRINT TYPE 123; # integer datatype
PRINT TYPE 123.4; # float datatype
PRINT TYPE COMPLEX();


