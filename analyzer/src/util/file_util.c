#include "util/file_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool file_read_all(const char *path, char **out, size_t *size) {
    *out = NULL;
    if (size) {
        *size = 0;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    size_t capacity = 1 << 16, length = 0;
    char *text = malloc(capacity);
    while (text) {
        size_t got = fread(text + length, 1, capacity - length - 1, file);
        length += got;
        if (length < capacity - 1) {
            break;
        }
        char *grown = realloc(text, capacity * 2);
        if (!grown) {
            free(text);
            text = NULL;
            break;
        }
        text = grown;
        capacity *= 2;
    }
    fclose(file);
    if (!text) {
        return false;
    }
    text[length] = '\0';
    *out = text;
    if (size) {
        *size = length;
    }
    return true;
}

bool file_write_atomic(const char *path, const char *text) {
    char *temporary = malloc(strlen(path) + 5);
    if (!temporary) {
        return false;
    }
    sprintf(temporary, "%s.tmp", path);
    FILE *file = fopen(temporary, "wb");
    size_t length = strlen(text);
    bool wrote = file && fwrite(text, 1, length, file) == length;
    wrote = (file ? fclose(file) == 0 : false) && wrote && rename(temporary, path) == 0;
    if (!wrote) {
        remove(temporary);
    }
    free(temporary);
    return wrote;
}
