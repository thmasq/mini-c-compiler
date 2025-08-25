#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Common compiler constants
#define MAX_IDENTIFIER_LENGTH 256
#define MAX_STRING_LENGTH 1024

// Error codes
#define ERROR_SUCCESS 0
#define ERROR_SYNTAX 1
#define ERROR_SEMANTIC 2
#define ERROR_CODEGEN 3
#define ERROR_IO 4

// Utility functions
char *string_duplicate(const char *str);
void compiler_error(int code, const char *message);

#endif
