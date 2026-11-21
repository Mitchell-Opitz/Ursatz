#include "util/misc_util.h"

#include <stdlib.h>
#include <string.h>

#include <common/json.h>

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

bool analyzer_fraction_read(const cJSON *value, unsigned forms, long long *numerator, long long *denominator) {
    long long n;
    long long d;
    if (!common_json_rational_read(value, forms, &n, &d) || n < 0) {
        return false;
    }
    *numerator = n;
    *denominator = d;
    return true;
}
