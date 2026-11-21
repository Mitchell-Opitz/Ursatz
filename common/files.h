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

/* A malloc'd "<a>/<b>" (common_path_join's rule: a trailing "/" of a is not doubled), or a copy of b when a is empty (a stored relative
   path has no folder in front of it). NULL when either is NULL or on allocation failure. */
char *common_path_join_alloc(const char *a, const char *b);

/* The whole file as a NUL-terminated buffer the caller frees. size, when not NULL, receives the byte count. */
UrsatzStatus common_file_read(const char *path, char **data, size_t *size, UrsatzError *error);
/*
 * Writes data to "<path>.tmp-<pid>", flushes and fsyncs it, renames it over path and fsyncs the folder, so a reader sees the old
 * file or the new one whole, and a stop leaves one of the two. The temporary is removed on failure.
 */
UrsatzStatus common_file_write_atomic(const char *path, const void *data, size_t size, UrsatzError *error);
/* Appends data to path (created when absent) and fsyncs. */
UrsatzStatus common_file_append_sync(const char *path, const void *data, size_t size, UrsatzError *error);

/* Copies the file `from` to `to` whole: read, then common_file_write_atomic (so `to` is the old file or the new one). NOT_FOUND when `from`
   is absent. */
UrsatzStatus common_file_copy(const char *from, const char *to, UrsatzError *error);

/* Makes the folder that holds `path` (and its parents); OK for a path with no folder part ("a.txt") or whose folder exists. */
UrsatzStatus common_dir_make_parent(const char *path, UrsatzError *error);

/* Creates the folder, whose parent exists; CONFLICT when the path already exists (a new id must not reuse a folder). */
UrsatzStatus common_dir_create(const char *path, UrsatzError *error);
/* Makes the folder and its parents; an existing folder is OK. */
UrsatzStatus common_dir_make(const char *path, UrsatzError *error);
/* Removes the folder and everything under it; an absent path is OK. */
UrsatzStatus common_dir_remove(const char *path, UrsatzError *error);
/* The names in the folder, without "." and "..", sorted bytewise. out is filled empty first; free it with common_names_free. */
UrsatzStatus common_dir_list(const char *path, CommonNames *out, UrsatzError *error);
/* fsyncs the folder, so a rename or a new file in it is durable. */
UrsatzStatus common_dir_sync(const char *path, UrsatzError *error);

#endif
