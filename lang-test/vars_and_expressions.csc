# Variable Declarations
SET message = "Welcome to CommandPro!";
SET a = 200;
SET b = 300;

# Arithmetic and String Operations
SET result = num * 2;
SET name = "User";
PRINTLN "Hello " + name + "!";
PRINTLN message;

# Type Conversions in Inputs
INPUT "Enter a number: " TO a AS INT;
INPUT "Enter another number: " TO b AS INT;
SET sum = a + b;
PRINTLN "Sum is: " + sum;

# Type conversion in variables
SET cords = POINT(100, 300);
SET cords_string = STRING coords;
SET bool_string = STRING TRUE;
SET int_bool = INT TRUE; # should set value as 1 in var
SET int_str = INT "32767";

# Comparison expressions
SET isBig = a > b;
PRINTLN isBig;
PRINTLN a < b;
PRINTLN a == b;
PRINTLN a != b;
PRINTLN a <= b;
PRINTLN a >= b;
PRINTLN a === b;
PRINTLN TYPE a == TYPE b; # checking if both vars are of same datatype
PRINTLN a AND b;
PRINTLN a IS b;
PRINTLN a OR b;
PRINTLN NOT a > b;
PRINTLN a IS NOT b;
PRINTLN "i" IN "string";
PRINTLN "i" NOT IN "string"; 



# Operators with (BODMAS)
SET result = ((15 + 5) * 2 ** 3) / (10 - (3 + 2)) + 4 * (7 - 3);
PRINTLN result; # should print 48.0
PRINTLN TYPE result; # should print datatype float
PRINTLN a + b;
PRINTLN a - b;
PRINTLN a * b;
PRINTLN a / b;
PRINTLN a // b;
PRINTLN a % b;
PRINTLN a | b; # bitwise OR
PRINTLN a & b; # AND falls under bitwise operators
PRINTLN a ^ b; # XOR
PRINTLN ~ a; # NOT, invert all the bits
PRINTLN a >> b; # signed right shift
PRINTLN a << b; # zero fill left shift



# Assignment operators
SET x = 5;
x += 3;
x -= 3;
x *= 3;
x /= 3;
x %= 3;
x //= 3;
x **= 3;
x &= 3;
x |= 3;
x ^= 3;
x >>= 3;
x <<= 3;
PRINTLN x := 3; # will run SET x = 3; first then will run PRINTLN x;