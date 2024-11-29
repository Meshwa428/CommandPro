#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

long file_size(FILE* file) {
	if (!file) { return 0; }
	fpos_t original = 0;
	if (fgetpos(file, &original) != 0) {
		printf("Failed to get file size: %i\n", errno);
		return 0;
	}
	fseek(file, 0, SEEK_END);
	long out = ftell(file);
	if (fsetpos(file, &original) != 0) {
		printf("Failed to get file size: %i\n", errno);
		return 0;
	}
	return out;
}

char* file_contents(char* path) {
	FILE* file = fopen(path, "r");
	if (!file)
	{
		printf("Failed to open file: %s\n", path);
		return NULL;
	}
	long size = file_size(file);
	char* contents = malloc(size + 1);
	char* write_it = contents;
	size_t bytes_read = 0;
	while (bytes_read < size) {
		size_t bytes_read_this_itr = fread(write_it, 1, size - bytes_read, file);
		if (ferror(file)) {
			printf("Error while reading file: %i", errno);
			free(contents);
			return NULL;
		}

		bytes_read += bytes_read_this_itr;
		write_it += bytes_read_this_itr;

		if (feof(file)) {
			break;
		}
	}
	contents[bytes_read] = '\0';

	return contents;
}

void print_usage(char** argv) {
	printf("USAGE: \n%s <code_file_path>\n", argv[0]);
}

typedef struct Error {
	enum ErrorType {
		ERROR_NONE = 0,
		ERROR_TYPE,
		ERROR_ARGUMENTS,
		ERROR_GENERIC,
		ERROR_INVALID_ARGUMENT,
		ERROR_SYNTAX,
		ERROR_TODO,
	} type;
	char* msg;
} Error;

Error ok = { ERROR_NONE, NULL };

void print_error(Error err) {
	if (err.type == ERROR_NONE) {
		return;
	}
	printf("Error: ");
	switch (err.type) {
	default:
		printf("Unknown error type");
		break;
	case ERROR_TODO:
		printf("TODO error");
		break;
	case ERROR_TYPE:
		printf("Mismatched Type error");
		break;
	case ERROR_ARGUMENTS:
		printf("Argument error");
		break;
	case ERROR_INVALID_ARGUMENT:
		printf("Invalid argument error");
		break;
	case ERROR_SYNTAX:
		printf("Invalid Syntax error");
		break;
	case ERROR_NONE:
		break;
	case ERROR_GENERIC:
		break;
	}
	putchar('\n');
	if (err.msg) {
		printf("     : %s\n", err.msg);
	}
}

#define ERROR_CREATE(n, t, msg) Error (n) = { (t), (msg) }
#define ERROR_PREP(n, t, message)\
	(n).type = (t);\
	(n).msg = (message);


// lex the next token from SOURCE, point to it with BEG and END
Error lex(char* source, char** beg, char** end) {
	Error err = ok;
	if (!source || !beg || !end) {
		ERROR_PREP(err, ERROR_ARGUMENTS, "Cannot lex empty source");
		return err;
	}

	*beg = source;
	*end = source;

	return err;
}

Error parse_expr(char* source) {
	char* beg = source;
	char* end = source;

	Error err = lex(source, &beg, &end);
	return err;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		print_usage(argv);
		exit(0);
	}

	char* path = argv[1];
	char* contents = file_contents(path);

	if (contents) {
		printf("Contents of %s:\n---\n\"%s\"\n---\n", path, contents);
		free(contents);
	}

	Error err = parse_expr(contents);
	print_error(err);

	return 0;
}