#ifndef ERRORS_H
#define ERRORS_H

#include <stdlib.h>

// Error type enumeration
typedef enum {
    ERROR_NONE,
    ERROR_SYNTAX,
    ERROR_RUNTIME,
    ERROR_TYPE,
    ERROR_INVALID_NUMBER,
    ERROR_CONTROL_FLOW,
    ERROR_CONTINUE
} ErrorType;

// Error structure
typedef struct {
    ErrorType type;
    char* message;
    int line;
    // For control flow errors
    char* statement_type;  // BREAK, CONTINUE, RETURN, YIELD
    void* value;          // For RETURN and YIELD values
} Error;

// Error handling functions
Error* create_error(ErrorType type, const char* message, int line);
Error* create_control_flow_error(const char* statement_type, void* value, int line);
void free_error(Error* error);

// Error checking functions
int is_error(Error* error);
int is_control_flow(Error* error);
const char* error_type_to_string(ErrorType type);

// Global error state
extern Error* current_error;

// Error setting and getting
void set_error(Error* error);
Error* get_error(void);
void clear_error(void);

#endif // ERRORS_H 