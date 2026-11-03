#include "util/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int util_absolute(int value) {
    return value < 0 ? -value : value;
}

unsigned util_bits_set(unsigned bits) {
    unsigned count = 0;
    for (; bits; bits >>= 1) {
        count += bits & 1u;
    }
    return count;
}

int util_sign(int value) {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

char *util_copy_string(const char *text) {
    size_t size = text ? strlen(text) + 1 : 1;
    char *out = malloc(size);
    if (out) {
        memcpy(out, text ? text : "", size);
    }
    return out;
}

bool util_read_file(const char *path, char **text, size_t *size) {
    *text = NULL;
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    long length = -1;
    if (fseek(file, 0, SEEK_END) == 0) {
        length = ftell(file);
        if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
            length = -1;
        }
    }
    char *buffer = length >= 0 ? malloc((size_t)length + 1) : NULL;
    if (buffer && fread(buffer, 1, (size_t)length, file) == (size_t)length) {
        buffer[length] = '\0';
        *text = buffer;
        if (size) {
            *size = (size_t)length;
        }
    } else {
        free(buffer);
    }
    fclose(file);
    return *text != NULL;
}
