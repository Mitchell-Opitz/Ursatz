#ifndef URSATZ_COMPOSER_UTIL_H
#define URSATZ_COMPOSER_UTIL_H

#include <stdbool.h>
#include <stddef.h>

/* Small helpers the sources share. Private: not installed, not part of the API. */

/* The absolute value of an int. */
int util_absolute(int value);

/* How many bits are set. */
unsigned util_bits_set(unsigned bits);

/* -1, 0 or 1 by the sign of the value. */
int util_sign(int value);

/* A malloc'd copy of the text ("" for NULL), or NULL when out of memory. The caller frees it. */
char *util_copy_string(const char *text);

/*
 * Reads the whole file at path into a NUL-terminated string in *text (the caller frees it); *size, when not NULL, is
 * its length without the terminator. False, with *text NULL, when the file cannot be read.
 */
bool util_read_file(const char *path, char **text, size_t *size);

#endif
