#include "util/util.h"

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
