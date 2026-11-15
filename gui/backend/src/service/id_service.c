#include "id_service.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define URSATZ_GUI_GETPID() ((unsigned long)_getpid())
#else
#include <unistd.h>
#define URSATZ_GUI_GETPID() ((unsigned long)getpid())
#endif

#define ID_RANDOM_BYTES 8

static uint64_t fnv1a_update(uint64_t hash, uint64_t value) {
    for (int i = 0; i < 8; i++) {
        hash ^= (value >> (i * 8)) & 0xffu;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static uint64_t fallback_random(void) {
    static unsigned long counter = 0;
    uint64_t hash = 14695981039346656037ULL;
    hash = fnv1a_update(hash, (uint64_t)time(NULL));
    hash = fnv1a_update(hash, (uint64_t)clock());
    hash = fnv1a_update(hash, (uint64_t)URSATZ_GUI_GETPID());
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

char *ursatz_gui_new_id(const char *prefix) {
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
