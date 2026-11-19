#include "util/misc_util.h"

#include <stdlib.h>
#include <string.h>

char *text_copy(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy) {
        memcpy(copy, text, length);
    }
    return copy;
}

int size_compare(const void *left, const void *right) {
    size_t a = *(const size_t *)left;
    size_t b = *(const size_t *)right;
    return a < b ? -1 : (a > b ? 1 : 0);
}

UrsatzStatus analyzer_status_error(UrsatzError *error, UrsatzStatus status, const char *what) {
    if (status != URSATZ_OK) {
        ursatz_error_set(error, status, "", "%s failed: %s", what, ursatz_status_name(status));
    }
    return status;
}
