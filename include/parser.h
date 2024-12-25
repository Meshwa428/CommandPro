#ifndef PARSER_H
#define PARSER_H

#include "token.h"
#include "ast.h"
#include "errors.h"

// Parser structure
typedef struct {
    Token** tokens;
    size_t token_count;
    size_t current_pos;
    // Scope management
    struct Scope* current_scope;
    struct Scope* global_scope;
    // Context tracking
    char** context_stack;
    size_t context_stack_size;
    size_t context_stack_capacity;
} Parser;

// Scope structure for variable and function tracking
typedef struct Scope {
    struct {
        char** names;
        char** types;
        size_t count;
        size_t capacity;
    } variables;
    
    struct {
        char** names;
        StringList** parameters;
        size_t count;
        size_t capacity;
    } functions;
    
    struct Scope* parent;
} Scope;

// Parser creation and destruction
Parser* parser_create(Token** tokens, size_t token_count);
void parser_destroy(Parser* parser);

// Main parsing functions
ASTNode* parser_parse(Parser* parser);
ASTNode* parser_parse_statement(Parser* parser);
ASTNode* parser_parse_expression(Parser* parser);

// Statement parsing functions
ASTNode* parser_parse_function_definition(Parser* parser);
ASTNode* parser_parse_assignment(Parser* parser);
ASTNode* parser_parse_print_statement(Parser* parser);
ASTNode* parser_parse_wait_statement(Parser* parser);
ASTNode* parser_parse_move_statement(Parser* parser);
ASTNode* parser_parse_key_operation(Parser* parser);
ASTNode* parser_parse_button_operation(Parser* parser);
ASTNode* parser_parse_while_loop(Parser* parser);
ASTNode* parser_parse_repeat_loop(Parser* parser);
ASTNode* parser_parse_if_statement(Parser* parser);
ASTNode* parser_parse_control_statement(Parser* parser);
ASTNode* parser_parse_increment_decrement(Parser* parser);

// Expression parsing functions
ASTNode* parser_parse_primary(Parser* parser);
ASTNode* parser_parse_expression_precedence(Parser* parser, int min_precedence);
ASTNode* parser_parse_identifier_usage(Parser* parser);
ASTNode* parser_parse_function_call(Parser* parser);

// Utility functions
Token* parser_peek(Parser* parser);
Token* parser_consume(Parser* parser, const char* expected_kind);
void parser_advance(Parser* parser);
int parser_is_at_end(Parser* parser);

// Scope management
Scope* create_scope(Scope* parent);
void destroy_scope(Scope* scope);
void enter_scope(Parser* parser);
void exit_scope(Parser* parser);
int is_variable_in_scope(Scope* scope, const char* name);
int is_function_in_scope(Scope* scope, const char* name);
void register_variable(Scope* scope, const char* name, const char* type);
void register_function(Scope* scope, const char* name, StringList* parameters);

// Context management
void push_context(Parser* parser, const char* context);
void pop_context(Parser* parser);
int is_in_context(Parser* parser, const char* context);

#endif // PARSER_H
