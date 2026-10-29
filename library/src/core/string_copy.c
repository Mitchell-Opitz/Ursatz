#include "core/string_copy.h"

#include <stdlib.h>
#include <string.h>

char *ursatz_string_copy(const char *text) {
    if (text == NULL) {
        return NULL;
    }
    size_t length = strlen(text);
    char *copy = (char *)malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}
