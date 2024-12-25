#ifndef LEXER_H
#define LEXER_H

#include "token.h"

// Lexer structure
typedef struct Lexer {
    char* source_code;
    Token** tokens;
    size_t token_count;
    size_t current_pos;
    int line_num;

    // Define keyword arrays
    const char** keywords;
    size_t keywords_count;

    const char** loop_keywords;
    size_t loop_keywords_count;

    const char** io_keywords;
    size_t io_keywords_count;

    const char** input_control_keywords;
    size_t input_control_keywords_count;

    const char** error_keywords;
    size_t error_keywords_count;

    const char** control_keywords;
    size_t control_keywords_count;

    const char** generator_keywords;
    size_t generator_keywords_count;

    const char** type_keywords;
    size_t type_keywords_count;

    const char** target_keywords;
    size_t target_keywords_count;

    const char** assertion_keywords;
    size_t assertion_keywords_count;

    const char** keyboard_keys;
    size_t keyboard_keys_count;

    const char** mouse_keys;
    size_t mouse_keys_count;

    const char** boolean_values;
    size_t boolean_values_count;
} Lexer;

// Function prototypes
Lexer* lexer_create(const char* source_code);
void lexer_destroy(Lexer* lexer);
Token** lexer_tokenize(Lexer* lexer, size_t* out_token_count);
Token** connect_tokens(Token** tokens, size_t token_count);
int is_keyword(const Lexer* lexer, const char* word);
int is_keyword_in_list(const char** list, size_t list_count, const char* word);
void process_time_with_unit(const char* match_str, double* value, char* unit);

#endif // LEXER_H