#include "util/misc_util.h"

#include <stdlib.h>
#include <string.h>

#include "time/duration.h"
#include "time/temporal_anchor.h"

#define FNV1A_PRIME 1099511628211ULL

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

int fraction_compare(long long a, long long b, long long c, long long d) {
    long long left = a * d;
    long long right = c * b;
    return left < right ? -1 : (left > right ? 1 : 0);
}

EntityID *entity_id_copy(const EntityID *id) {
    EntityID *copy = NULL;
    return entity_id_create(entity_id_value(id), &copy) == ENTITY_ID_OK ? copy : NULL;
}

MusicalTime *notes_last_end(const NoteEvent *const *events, size_t count) {
    MusicalTime *latest = NULL;
    for (size_t i = 0; i < count; i++) {
        MusicalTime *end = NULL;
        if (musical_time_add(temporal_anchor_position(note_event_temporal_anchor(events[i])),
                             note_event_duration(events[i]), &end) != MUSICAL_TIME_OK) {
            musical_time_destroy(latest);
            return NULL;
        }
        if (!latest || musical_time_numerator(end) * (long)musical_time_denominator(latest) >
                           musical_time_numerator(latest) * (long)musical_time_denominator(end)) {
            musical_time_destroy(latest);
            latest = end;
        } else {
            musical_time_destroy(end);
        }
    }
    if (!latest && musical_time_create(0, 1, &latest) != MUSICAL_TIME_OK) {
        return NULL;
    }
    return latest;
}

uint64_t fnv1a_bytes(uint64_t hash, const void *data, size_t length) {
    const unsigned char *bytes = data;
    for (size_t i = 0; i < length; i++) {
        hash = (hash ^ bytes[i]) * FNV1A_PRIME;
    }
    return hash;
}
