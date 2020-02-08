#include "pipeline/path_util.h"

#include <stdlib.h>
#include <string.h>

static const char *find_last_path_separator(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    if (slash && backslash) {
        return slash > backslash ? slash : backslash;
    }
    return slash ? slash : backslash;
}

const char *path_basename(const char *path) {
    const char *separator = find_last_path_separator(path);
    return separator ? separator + 1 : path;
}

char *path_with_suffix(const char *input_path, const char *suffix) {
    size_t input_len = strlen(input_path);
    const char *dot = strrchr(path_basename(input_path), '.');
    size_t stem_len = dot ? (size_t)(dot - input_path) : input_len;

    size_t suffix_len = strlen(suffix);
    char *result = malloc(stem_len + suffix_len + 1);
    if (!result) {
        return NULL;
    }
    memcpy(result, input_path, stem_len);
    memcpy(result + stem_len, suffix, suffix_len + 1);
    return result;
}
