#ifndef URSATZ_ANALYZER_FILE_UTIL_H
#define URSATZ_ANALYZER_FILE_UTIL_H

#include <stdbool.h>
#include <stddef.h>

/* Reads the whole file into a NUL-terminated string the caller frees. size, when not NULL, receives the byte count.
   False (*out NULL) when the file cannot be opened or memory runs out. */
bool file_read_all(const char *path, char **out, size_t *size);

/* Writes text to path + ".tmp", then renames it over path, so a reader sees the old file or the new one. The
   temporary file is removed on failure. */
bool file_write_atomic(const char *path, const char *text);

#endif
