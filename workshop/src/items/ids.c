#include "ids.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <common/hash.h>

#define WORKSHOP_GETPID() ((unsigned long)getpid())

#define ID_RANDOM_BYTES 8

static uint64_t fnv1a_update(uint64_t hash, uint64_t value) {
    unsigned char bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (unsigned char)((value >> (i * 8)) & 0xffu);
    }
    return common_fnv1a64(bytes, sizeof(bytes), hash);
}

static uint64_t fallback_random(void) {
    static unsigned long counter = 0;
    uint64_t hash = COMMON_FNV1A64_OFFSET;
    hash = fnv1a_update(hash, (uint64_t)time(NULL));
    hash = fnv1a_update(hash, (uint64_t)clock());
    hash = fnv1a_update(hash, (uint64_t)WORKSHOP_GETPID());
    hash = fnv1a_update(hash, (uint64_t)counter++);
    return hash;
}

static uint64_t random_u64(void) {
    unsigned char bytes[ID_RANDOM_BYTES];
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t got = fread(bytes, 1, sizeof(bytes), f);
        fclose(f);
        if (got == sizeof(bytes)) {
            uint64_t value = 0;
            for (size_t i = 0; i < sizeof(bytes); i++) {
                value = (value << 8) | bytes[i];
            }
            return value;
        }
    }
    return fallback_random();
}

char *workshop_new_id(const char *prefix) {
    if (!prefix) {
        return NULL;
    }
    size_t size = strlen(prefix) + 1 + 16 + 1;
    char *id = malloc(size);
    if (!id) {
        return NULL;
    }
    uint64_t value = random_u64();
    snprintf(id, size, "%s-%08x%08x", prefix, (unsigned)(value >> 32), (unsigned)(value & 0xffffffffu));
    return id;
}
