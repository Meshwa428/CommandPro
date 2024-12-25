#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
	const char* source = "SET x = 10\nPRINT x";
	Lexer* lexer = lexer_create(source);
	size_t token_count = 0;
	Token** tokens = lexer_tokenize(lexer, &token_count);

	for (size_t i = 0; i < token_count; i++) {
		char* token_str = token_to_string(tokens[i]);
		printf("%s\n", token_str);
		free(token_str);
	}

	lexer_destroy(lexer);
	return 0;
}

