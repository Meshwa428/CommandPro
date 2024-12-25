#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Utility function to initialize keyword lists
static const char** init_keywords(const char* keywords[], size_t count) {
    const char** kw = malloc(sizeof(char*) * count);
    for (size_t i = 0; i < count; i++) {
        kw[i] = keywords[i];
    }
    return kw;
}

// Create a new Lexer instance
Lexer* lexer_create(const char* source_code) {
    Lexer* lexer = malloc(sizeof(Lexer));
    lexer->source_code = strdup(source_code);
    lexer->tokens = NULL;
    lexer->token_count = 0;
    lexer->current_pos = 0;
    lexer->line_num = 1;

    // Initialize keyword lists
    static const char* keywords_array[] = {
        "SET", "DEFUN", "IF", "THEN", "ELSE", "ELSEIF", "ENDIF",
        "TIMES", "RETURN", "BREAK", "CONTINUE", "YIELD", "PASS"
    };
    lexer->keywords = init_keywords(keywords_array, sizeof(keywords_array) / sizeof(char*));
    lexer->keywords_count = sizeof(keywords_array) / sizeof(char*);

    static const char* loop_keywords_array[] = { "REPEAT", "WHILE" };
    lexer->loop_keywords = init_keywords(loop_keywords_array, sizeof(loop_keywords_array) / sizeof(char*));
    lexer->loop_keywords_count = sizeof(loop_keywords_array) / sizeof(char*);

    // Initialize other keyword lists similarly...
    // For brevity, not all lists are initialized here. Implement similarly as above.

    // Example for io_keywords
    static const char* io_keywords_array[] = { "PRINTLN", "PRINT", "INPUT", "OPEN", "WRITE", "RUN" };
    lexer->io_keywords = init_keywords(io_keywords_array, sizeof(io_keywords_array) / sizeof(char*));
    lexer->io_keywords_count = sizeof(io_keywords_array) / sizeof(char*);

    // Similarly initialize input_control_keywords, error_keywords, control_keywords, etc.

    return lexer;
}

// Destroy a Lexer instance
void lexer_destroy(Lexer* lexer) {
    if (!lexer) return;
    free(lexer->source_code);
    // Free tokens
    for (size_t i = 0; i < lexer->token_count; i++) {
        token_destroy(lexer->tokens[i]);
    }
    free(lexer->tokens);
    // Free keyword lists
    free((void*)lexer->keywords);
    free((void*)lexer->loop_keywords);
    free((void*)lexer->io_keywords);
    // Free other keyword lists similarly...
    free(lexer);
}

// Check if a word is a keyword in any list
int is_keyword(const Lexer* lexer, const char* word) {
    return is_keyword_in_list(lexer->keywords, lexer->keywords_count, word) ||
           is_keyword_in_list(lexer->loop_keywords, lexer->loop_keywords_count, word) ||
           is_keyword_in_list(lexer->io_keywords, lexer->io_keywords_count, word);
           // Add checks for other keyword lists...
}

// Check if a word is in a specific keyword list
int is_keyword_in_list(const char** list, size_t list_count, const char* word) {
    for (size_t i = 0; i < list_count; i++) {
        if (strcasecmp(list[i], word) == 0) {
            return 1;
        }
    }
    return 0;
}

// Process time with unit
void process_time_with_unit(const char* match_str, double* value, char* unit) {
    sscanf(match_str, "%lf%s", value, unit);
    if (strcmp(unit, "ms") == 0) {
        *value /= 1000.0;
    } else if (strcmp(unit, "m") == 0) {
        *value *= 60.0;
    } else if (strcmp(unit, "h") == 0) {
        *value *= 3600.0;
    }
    // 's' doesn't need conversion
}

// Tokenize the source code
Token** lexer_tokenize(Lexer* lexer, size_t* out_token_count) {
    size_t capacity = 128;
    lexer->tokens = malloc(sizeof(Token*) * capacity);
    lexer->token_count = 0;

    size_t length = strlen(lexer->source_code);
    char current_char;
    char buffer[256];
    size_t buffer_pos = 0;

    while (lexer->current_pos < length) {
        current_char = lexer->source_code[lexer->current_pos];

        // Handle whitespace
        if (isspace(current_char)) {
            if (current_char == '\n') {
                lexer->line_num++;
            }
            lexer->current_pos++;
            continue;
        }

        // Handle comments
        if (current_char == '#') {
            // Single-line comment
            if (lexer->source_code[lexer->current_pos + 1] != '*') {
                while (current_char != '\n' && lexer->current_pos < length) {
                    current_char = lexer->source_code[++lexer->current_pos];
                }
                continue;
            }
            // Multi-line comment (#* *#)
            lexer->current_pos += 2;
            while (!(lexer->source_code[lexer->current_pos] == '*' &&
                     lexer->source_code[lexer->current_pos + 1] == '#') &&
                   lexer->current_pos < length) {
                if (lexer->source_code[lexer->current_pos] == '\n') {
                    lexer->line_num++;
                }
                lexer->current_pos++;
            }
            lexer->current_pos += 2;
            continue;
        }

        // Handle identifiers and keywords
        if (isalpha(current_char) || current_char == '_') {
            buffer_pos = 0;
            while (isalnum(current_char) || current_char == '_') {
                buffer[buffer_pos++] = current_char;
                lexer->current_pos++;
                current_char = lexer->source_code[lexer->current_pos];
            }
            buffer[buffer_pos] = '\0';
            if (is_keyword(lexer, buffer)) {
                Token* token = token_create("KEYWORD", buffer, lexer->line_num, NULL, NULL);
                // Add token to list
                if (lexer->token_count >= capacity) {
                    capacity *= 2;
                    lexer->tokens = realloc(lexer->tokens, sizeof(Token*) * capacity);
                }
                lexer->tokens[lexer->token_count++] = token;
            } else {
                Token* token = token_create("ID", buffer, lexer->line_num, NULL, NULL);
                // Add token to list
                if (lexer->token_count >= capacity) {
                    capacity *= 2;
                    lexer->tokens = realloc(lexer->tokens, sizeof(Token*) * capacity);
                }
                lexer->tokens[lexer->token_count++] = token;
            }
            continue;
        }

        // Handle numbers (integers and floats)
        if (isdigit(current_char)) {
            buffer_pos = 0;
            int is_float = 0;
            while (isdigit(current_char) || current_char == '.') {
                if (current_char == '.') {
                    is_float = 1;
                }
                buffer[buffer_pos++] = current_char;
                lexer->current_pos++;
                current_char = lexer->source_code[lexer->current_pos];
            }
            buffer[buffer_pos] = '\0';
            if (is_float) {
                Token* token = token_create("FLOAT", buffer, lexer->line_num, NULL, NULL);
                if (lexer->token_count >= capacity) {
                    capacity *= 2;
                    lexer->tokens = realloc(lexer->tokens, sizeof(Token*) * capacity);
                }
                lexer->tokens[lexer->token_count++] = token;
            } else {
                Token* token = token_create("INTEGER", buffer, lexer->line_num, NULL, NULL);
                if (lexer->token_count >= capacity) {
                    capacity *= 2;
                    lexer->tokens = realloc(lexer->tokens, sizeof(Token*) * capacity);
                }
                lexer->tokens[lexer->token_count++] = token;
            }
            continue;
        }

        // Handle string literals
        if (current_char == '"') {
            lexer->current_pos++; // Skip the opening quote
            buffer_pos = 0;
            current_char = lexer->source_code[lexer->current_pos];
            while (current_char != '"' && lexer->current_pos < length) {
                buffer[buffer_pos++] = current_char;
                lexer->current_pos++;
                current_char = lexer->source_code[lexer->current_pos];
            }
            buffer[buffer_pos] = '\0';
            lexer->current_pos++; // Skip the closing quote
            Token* token = token_create("STRING", buffer, lexer->line_num, NULL, NULL);
            if (lexer->token_count >= capacity) {
                capacity *= 2;
                lexer->tokens = realloc(lexer->tokens, sizeof(Token*) * capacity);
            }
            lexer->tokens[lexer->token_count++] = token;
            continue;
        }

        // Handle operators and punctuations
        // Example: +, -, *, /, ==, !=, etc.
        // This section can be expanded based on the operators defined in the Python lexer

        // For simplicity, handle single-character operators
        buffer[0] = current_char;
        buffer[1] = '\0';
        Token* token = token_create("OPERATOR", buffer, lexer->line_num, NULL, NULL);
        lexer->current_pos++;
        if (lexer->token_count >= capacity) {
            capacity *= 2;
            lexer->tokens = realloc(lexer->tokens, sizeof(Token*) * capacity);
        }
        lexer->tokens[lexer->token_count++] = token;
    }

    // Append EOF token
    Token* eof_token = token_create("EOF", NULL, lexer->line_num, NULL, NULL);
    if (lexer->token_count >= capacity) {
        capacity += 1;
        lexer->tokens = realloc(lexer->tokens, sizeof(Token*) * capacity);
    }
    lexer->tokens[lexer->token_count++] = eof_token;

    // Connect tokens
    lexer->tokens = connect_tokens(lexer->tokens, lexer->token_count);

    *out_token_count = lexer->token_count;
    return lexer->tokens;
}

// Connect tokens for bidirectional traversal
Token** connect_tokens(Token** tokens, size_t token_count) {
    for (size_t i = 0; i < token_count; i++) {
        if (i > 0) {
            tokens[i]->previous_token = tokens[i - 1];
        }
        if (i < token_count - 1) {
            tokens[i]->next_token = tokens[i + 1];
        }
    }
    return tokens;
}
