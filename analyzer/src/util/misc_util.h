#ifndef URSATZ_ANALYZER_MISC_UTIL_H
#define URSATZ_ANALYZER_MISC_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cJSON.h>

#include <common/status.h>

/* A malloc'd copy of text; NULL on allocation failure. */
char *text_copy(const char *text);

/* Fills error (NULL is allowed) for a failed call: "<what> failed: <status word>"; returns status (URSATZ_OK is returned as is). */
UrsatzStatus analyzer_status_error(UrsatzError *error, UrsatzStatus status, const char *what);

/* A rational of a document in one of the forms of common_json_rational_read (COMMON_RATIONAL_OBJECT, _TEXT), not reduced, with the
   denominator above zero and the numerator not below zero. False for anything else. */
bool analyzer_fraction_read(const cJSON *value, unsigned forms, long long *numerator, long long *denominator);

/* qsort comparator for size_t values, ascending. */
int size_compare(const void *left, const void *right);

#endif
