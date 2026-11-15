#ifndef URSATZ_COMPOSER_TEST_SUPPORT_H
#define URSATZ_COMPOSER_TEST_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Helpers the tests share. The whole file at `path` as a NUL-terminated string the caller frees; asserts when the file
 * cannot be read. When size is not NULL it receives the length without the terminator.
 */
char *test_read_file(const char *path, size_t *size);

/* Whether a file exists at the path. */
bool test_exists(const char *path);

/* Writes the text to a new file at the path; asserts when it cannot. */
void test_write_file(const char *path, const char *text);

#endif
