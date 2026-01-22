#ifndef URSATZ_ANALYZER_CORPUS_INTERNAL_H
#define URSATZ_ANALYZER_CORPUS_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

/* Default output directory name inside the input directory. */
#define CORPUS_DEFAULT_OUTPUT_NAME ".ursatz-corpus"

/* "<dir>/<name>", or just name when dir is empty. Caller frees; NULL on allocation failure. */
char *corpus_join(const char *dir, const char *name);

/* Creates every missing directory in path (like mkdir -p). */
bool corpus_make_dirs(const char *path);

/* Creates the parent directories of file_path. */
bool corpus_make_parent_dirs(const char *file_path);

#endif
