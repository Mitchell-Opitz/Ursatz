#ifndef URSATZ_ANALYZER_MISC_UTIL_H
#define URSATZ_ANALYZER_MISC_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A malloc'd copy of text; NULL on allocation failure. */
char *text_copy(const char *text);

/* qsort comparator for size_t values, ascending. */
int size_compare(const void *left, const void *right);

#endif
