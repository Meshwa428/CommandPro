#include <stdio.h>
#include "token.h"
#include <stdlib.h>
#include <string.h>

// Create a new token
Token* token_create(const char* kind, const char* value, int line, Token* prev, Token* next) {
    Token* token = (Token*)malloc(sizeof(Token));
    token->kind = strdup(kind);
    token->value = value ? strdup(value) : NULL;
    token->line = line;
    token->previous_token = prev;
    token->next_token = next;
    return token;
}

// Destroy a token
void token_destroy(Token* token) {
    if (!token) return;
    free(token->kind);
    if (token->value) free(token->value);
    free(token);
}

// Getters
const char* token_get_kind(const Token* token) {
    return token->kind;
}

const char* token_get_value(const Token* token) {
    return token->value;
}

int token_get_line(const Token* token) {
    return token->line;
}

Token* token_get_previous(const Token* token) {
    return token->previous_token;
}

Token* token_get_next(const Token* token) {
    return token->next_token;
}

// Setters
void token_set_previous(Token* token, Token* prev) {
    token->previous_token = prev;
}

void token_set_next(Token* token, Token* next) {
    token->next_token = next;
}

// String representation
char* token_to_string(const Token* token) {
    if (token->value) {
        size_t len = strlen(token->kind) + (token->value ? strlen(token->value) : 0) + 100;
        char* str = malloc(len);
        snprintf(str, len, "Token(kind='%s', value='%s', line=%d)", 
                token->kind, 
                token->value, 
                token->line);
        return str;
    } else {
        size_t len = strlen(token->kind) + 100;
        char* str = malloc(len);
        snprintf(str, len, "Token(kind='%s', value=NULL, line=%d)", 
                token->kind, 
                token->line);
        return str;
    }
}