#ifndef TOKEN_H
#define TOKEN_H

#include <string.h>

// Token structure
typedef struct Token {
    char* kind;
    char* value;
    int line;
    struct Token* previous_token;
    struct Token* next_token;
} Token;

// Constructor and destructor
Token* token_create(const char* kind, const char* value, int line, Token* prev, Token* next);
void token_destroy(Token* token);

// Getters
const char* token_get_kind(const Token* token);
const char* token_get_value(const Token* token); 
int token_get_line(const Token* token);
Token* token_get_previous(const Token* token);
Token* token_get_next(const Token* token);

// Setters
void token_set_previous(Token* token, Token* prev);
void token_set_next(Token* token, Token* next);

// String representation 
char* token_to_string(const Token* token);



#endif