#ifndef URSATZ_COMMON_FILES_H
#define URSATZ_COMMON_FILES_H

#include <stdbool.h>
#include <stddef.h>

#include <common/status.h>
#include <common/strings.h>

/*
 * Files and folders (POSIX). Every function that can fail fills the UrsatzError it is given (NULL is allowed): the status says
 * what kind of failure it was (NOT_FOUND for an absent path, IO otherwise) and the error's path is the path involved.
 */

/* True when path exists (a file or a folder). */
bool common_path_exists(const char *path);
/* True when path is an existing folder. */
bool common_path_is_dir(const char *path);
/* "<a>/<b>" into out; false (out unchanged) when it does not fit. A trailing "/" of a is not doubled. */
bool common_path_join(char *out, size_t capacity, const char *a, const char *b);

/* The whole file as a NUL-terminated buffer the caller frees. size, when not NULL, receives the byte count. */
UrsatzStatus common_file_read(const char *path, char **data, size_t *size, UrsatzError *error);
/*
 * Writes data to "<path>.tmp-<pid>", flushes and fsyncs it, renames it over path and fsyncs the folder, so a reader sees the old
 * file or the new one whole, and a stop leaves one of the two. The temporary is removed on failure.
 */
UrsatzStatus common_file_write_atomic(const char *path, const void *data, size_t size, UrsatzError *error);
/* Appends data to path (created when absent) and fsyncs. */
UrsatzStatus common_file_append_sync(const char *path, const void *data, size_t size, UrsatzError *error);

/* Makes the folder and its parents; an existing folder is OK. */
UrsatzStatus common_dir_make(const char *path, UrsatzError *error);
/* Removes the folder and everything under it; an absent path is OK. */
UrsatzStatus common_dir_remove(const char *path, UrsatzError *error);
/* The names in the folder, without "." and "..", sorted bytewise. out is filled empty first; free it with common_names_free. */
UrsatzStatus common_dir_list(const char *path, CommonNames *out, UrsatzError *error);
/* fsyncs the folder, so a rename or a new file in it is durable. */
UrsatzStatus common_dir_sync(const char *path, UrsatzError *error);

#endif
