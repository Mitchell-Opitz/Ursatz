#include "model/core/content_hash.h"

#include <stdio.h>

#define FNV1A64_PRIME 1099511628211ULL

uint64_t content_hash_fnv1a64(uint64_t hash, const void *data, size_t size) {
    const unsigned char *bytes = data;
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ bytes[i]) * FNV1A64_PRIME;
    }
    return hash;
}

void content_hash_hex16(uint64_t hash, char out[17]) {
    snprintf(out, 17, "%016llx", (unsigned long long)hash);
}
