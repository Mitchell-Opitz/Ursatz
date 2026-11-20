#include "common/hash.h"

#include <stdio.h>

#define FNV1A64_PRIME 1099511628211ULL

uint64_t common_fnv1a64(const void *data, size_t size, uint64_t seed) {
    const unsigned char *bytes = data;
    uint64_t hash = seed;
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ bytes[i]) * FNV1A64_PRIME;
    }
    return hash;
}

void common_fnv1a64_hex(uint64_t hash, char out[17]) {
    snprintf(out, 17, "%016llx", (unsigned long long)hash);
}
