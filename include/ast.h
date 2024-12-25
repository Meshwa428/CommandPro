#ifndef AST_H
#define AST_H

#include <stdlib.h>
#include <stdbool.h>

// Forward declarations
struct ASTNode;
typedef struct ASTNode ASTNode;

// Node type enumeration
typedef enum {
    NODE_PROGRAM,
    NODE_FUNCTION_DEFINITION,
    NODE_ASSIGNMENT,
    NODE_PRINT_STATEMENT,
    NODE_WAIT_STATEMENT,
    NODE_MOVE_MOUSE,
    NODE_KEY_OPERATION,
    NODE_BUTTON_OPERATION,
    NODE_BINARY_OPERATION,
    NODE_IDENTIFIER,
    NODE_INTEGER,
    NODE_FLOAT,
    NODE_TIME,
    NODE_STRING,
    NODE_EMPTY_STATEMENT,
    NODE_FUNCTION_CALL,
    NODE_BOOLEAN,
    NODE_WHILE_LOOP,
    NODE_REPEAT_LOOP,
    NODE_CONTROL_STATEMENT,
    NODE_INCREMENT_DECREMENT,
    NODE_IF_STATEMENT
} NodeType;

// Generic node list structure
typedef struct NodeList {
    ASTNode** nodes;
    size_t count;
    size_t capacity;
} NodeList;

// String list structure
typedef struct StringList {
    char** strings;
    size_t count;
    size_t capacity;
} StringList;

// Node structures for each type
typedef struct {
    NodeList* statements;
} ProgramNode;

typedef struct {
    char* name;
    StringList* parameters;
    NodeList* body;
} FunctionDefinitionNode;

typedef struct {
    char* variable_name;
    ASTNode* value;
    char* var_type;  // Optional
} AssignmentNode;

typedef struct {
    char* print_type;  // "PRINT" or "PRINTLN"
    ASTNode* expression;
} PrintStatementNode;

typedef struct {
    ASTNode* expression;
} WaitStatementNode;

typedef struct {
    ASTNode* x;
    ASTNode* y;
} MoveMouseNode;

typedef struct {
    char* operation;
    char* key;
} KeyOperationNode;

typedef struct {
    char* button;
} ButtonOperationNode;

typedef struct {
    char* operator;
    ASTNode* left;
    ASTNode* right;
} BinaryOperationNode;

typedef struct {
    char* name;
} IdentifierNode;

typedef struct {
    int value;
} IntegerNode;

typedef struct {
    double value;
} FloatNode;

typedef struct {
    double value;
    char* unit;
} TimeNode;

typedef struct {
    char* value;
} StringNode;

typedef struct {
    char* function_name;
    NodeList* arguments;
} FunctionCallNode;

typedef struct {
    bool value;
} BooleanNode;

typedef struct {
    ASTNode* condition;
    NodeList* body;
} WhileLoopNode;

typedef struct {
    ASTNode* count;
    NodeList* body;
} RepeatLoopNode;

typedef struct {
    char* statement_type;  // "BREAK", "CONTINUE", "RETURN", "YIELD"
    ASTNode* value;  // Optional, for RETURN and YIELD
} ControlStatementNode;

typedef struct {
    char* variable;
    char* operation;  // "++" or "--"
    bool is_prefix;
} IncrementDecrementNode;

typedef struct {
    ASTNode* condition;
    NodeList* then_body;
    NodeList* else_if_conditions;
    NodeList* else_if_bodies;
    NodeList* else_body;
} IfStatementNode;

// Main AST node structure
struct ASTNode {
    NodeType type;
    union {
        ProgramNode program;
        FunctionDefinitionNode function_def;
        AssignmentNode assignment;
        PrintStatementNode print_stmt;
        WaitStatementNode wait_stmt;
        MoveMouseNode move_mouse;
        KeyOperationNode key_op;
        ButtonOperationNode button_op;
        BinaryOperationNode binary_op;
        IdentifierNode identifier;
        IntegerNode integer;
        FloatNode float_val;
        TimeNode time;
        StringNode string;
        FunctionCallNode function_call;
        BooleanNode boolean;
        WhileLoopNode while_loop;
        RepeatLoopNode repeat_loop;
        ControlStatementNode control_stmt;
        IncrementDecrementNode inc_dec;
        IfStatementNode if_stmt;
    } data;
};

// Function declarations for node creation
ASTNode* create_program_node(NodeList* statements);
ASTNode* create_function_definition_node(const char* name, StringList* parameters, NodeList* body);
ASTNode* create_assignment_node(const char* variable_name, ASTNode* value, const char* var_type);
ASTNode* create_print_statement_node(const char* print_type, ASTNode* expression);
ASTNode* create_wait_statement_node(ASTNode* expression);
ASTNode* create_move_mouse_node(ASTNode* x, ASTNode* y);
ASTNode* create_key_operation_node(const char* operation, const char* key);
ASTNode* create_button_operation_node(const char* button);
ASTNode* create_binary_operation_node(const char* operator, ASTNode* left, ASTNode* right);
ASTNode* create_identifier_node(const char* name);
ASTNode* create_integer_node(int value);
ASTNode* create_float_node(double value);
ASTNode* create_time_node(double value, const char* unit);
ASTNode* create_string_node(const char* value);
ASTNode* create_function_call_node(const char* function_name, NodeList* arguments);
ASTNode* create_boolean_node(bool value);
ASTNode* create_while_loop_node(ASTNode* condition, NodeList* body);
ASTNode* create_repeat_loop_node(ASTNode* count, NodeList* body);
ASTNode* create_control_statement_node(const char* statement_type, ASTNode* value);
ASTNode* create_increment_decrement_node(const char* variable, const char* operation, bool is_prefix);
ASTNode* create_if_statement_node(ASTNode* condition, NodeList* then_body, 
                                NodeList* else_if_conditions, NodeList* else_if_bodies,
                                NodeList* else_body);

// Utility functions
NodeList* create_node_list(size_t initial_capacity);
void add_node_to_list(NodeList* list, ASTNode* node);
StringList* create_string_list(size_t initial_capacity);
void add_string_to_list(StringList* list, const char* str);

// Memory management
void free_node(ASTNode* node);
void free_node_list(NodeList* list);
void free_string_list(StringList* list);

// JSON serialization
char* node_to_json(const ASTNode* node);

#endif // AST_H
