#ifndef URSATZ_ANALYZER_MISC_UTIL_H
#define URSATZ_ANALYZER_MISC_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FNV1A_OFFSET_BASIS 14695981039346656037ULL

/* A malloc'd copy of text; NULL on allocation failure. */
char *text_copy(const char *text);

/* qsort comparator for size_t values, ascending. */
int size_compare(const void *left, const void *right);

/* Folds length bytes into a 64-bit FNV-1a hash; start from FNV1A_OFFSET_BASIS. */
uint64_t fnv1a_bytes(uint64_t hash, const void *data, size_t length);

#endif
