#include "util/misc_util.h"

#include <stdlib.h>
#include <string.h>

#include "model/time/duration.h"
#include "model/time/temporal_anchor.h"

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

EntityID *entity_id_copy(const EntityID *id) {
    EntityID *copy = NULL;
    return entity_id_create(entity_id_value(id), &copy) == ENTITY_ID_OK ? copy : NULL;
}

bool notes_last_end(const NoteEvent *const *events, size_t count, MusicalTime *out) {
    MusicalTime latest = musical_time_of(rational_ratio(0, 1));
    for (size_t i = 0; i < count; i++) {
        MusicalTime end;
        if (musical_time_plus(*temporal_anchor_position(note_event_temporal_anchor(events[i])),
                              *note_event_duration(events[i]), &end) != MUSICAL_TIME_OK) {
            return false;
        }
        if (musical_time_cmp(end, latest) > 0) {
            latest = end;
        }
    }
    *out = latest;
    return true;
}

uint64_t fnv1a_bytes(uint64_t hash, const void *data, size_t length) {
    const unsigned char *bytes = data;
    for (size_t i = 0; i < length; i++) {
        hash = (hash ^ bytes[i]) * FNV1A_PRIME;
    }
    return hash;
}
