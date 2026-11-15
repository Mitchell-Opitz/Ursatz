#ifndef URSATZ_ANALYZER_PATH_UTIL_H
#define URSATZ_ANALYZER_PATH_UTIL_H

/* The last path segment of path (after the last '/' or '\'); points
   into path. */
const char *path_basename(const char *path);

/* input_path with the last segment's extension (from its last '.')
   replaced by suffix, or suffix appended when there is none. Caller
   frees; NULL on allocation failure. */
char *path_with_suffix(const char *input_path, const char *suffix);

#endif
