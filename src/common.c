#define _POSIX_C_SOURCE 200809L
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *string_duplicate(const char *str)
{
	if (!str)
		return NULL;
	size_t len = strlen(str) + 1;
	char *copy = malloc(len);
	if (!copy) {
		fprintf(stderr, "Memory allocation failed\n");
		exit(1);
	}
	strcpy(copy, str);
	return copy;
}
