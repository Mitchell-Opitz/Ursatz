#ifndef URSATZ_ANALYZER_MISC_UTIL_H
#define URSATZ_ANALYZER_MISC_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <common/status.h>

/* A malloc'd copy of text; NULL on allocation failure. */
char *text_copy(const char *text);

/* Fills error (NULL is allowed) for a failed call: "<what> failed: <status word>"; returns status (URSATZ_OK is returned as is). */
UrsatzStatus analyzer_status_error(UrsatzError *error, UrsatzStatus status, const char *what);

/* qsort comparator for size_t values, ascending. */
int size_compare(const void *left, const void *right);

#endif
