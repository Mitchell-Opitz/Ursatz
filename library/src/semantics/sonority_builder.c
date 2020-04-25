#include "semantics/sonority_builder.h"

#include <stdlib.h>

#include "identity/entity_id.h"
#include "time/duration.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"

/*
 * v1 fixed beat unit: quarter note = 1/4 whole note -- see the scope
 * comment in sonority_builder.h. Not a parameter; a different beat unit
 * is new scope, added when a real use case demands it, not now.
 */
#define SONORITY_BUILDER_BEAT_NUMERATOR 1
#define SONORITY_BUILDER_BEAT_DENOMINATOR 4

/*
 * MusicalTime exposes no ordering beyond equality (time/musical_time.h);
 * this compares two positions by cross-multiplying (both denominators
 * are positive, so sign is preserved) -- the same technique
 * time/time_span.c's own musical_time_gte helper uses, without going
 * through float/double (Law 19).
 */
static int musical_time_compare(const MusicalTime *a, const MusicalTime *b) {
    long left = musical_time_numerator(a) * (long)musical_time_denominator(b);
    long right = musical_time_numerator(b) * (long)musical_time_denominator(a);
    if (left < right) {
        return -1;
    }
    if (left > right) {
        return 1;
    }
    return 0;
}

bool sonority_builder_note_active_at(const NoteEvent *note, const MusicalTime *at) {
    const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(note));

    MusicalTime *release;
    if (musical_time_add(onset, note_event_duration(note), &release) != MUSICAL_TIME_OK) {
        return false;
    }

    bool active = musical_time_compare(at, onset) >= 0 && musical_time_compare(at, release) < 0;
    musical_time_destroy(release);
    return active;
}

static EntityID *clone_entity_id(const EntityID *id) {
    EntityID *clone;
    if (entity_id_create(entity_id_value(id), &clone) != ENTITY_ID_OK) {
        return NULL;
    }
    return clone;
}

/*
 * Collects a fresh EntityID clone of every notes[i] active at `at` into a
 * newly allocated array. *out_count may legitimately be 0 (no note
 * active at this sample point) -- that is not an error, the caller skips
 * emitting a Sonority for it. On allocation failure, any clones already
 * made are destroyed and SONORITY_BUILDER_ERR_ALLOC is returned.
 */
static SonorityBuilderResult collect_active_ids_at(NoteEvent *const *notes, size_t note_count, const MusicalTime *at,
                                                     EntityID ***out_ids, size_t *out_count) {
    EntityID **ids = NULL;
    size_t count = 0;
    size_t capacity = 0;

    for (size_t i = 0; i < note_count; i++) {
        if (!sonority_builder_note_active_at(notes[i], at)) {
            continue;
        }
        if (count == capacity) {
            size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
            EntityID **grown = (EntityID **)realloc(ids, sizeof(EntityID *) * new_capacity);
            if (grown == NULL) {
                for (size_t j = 0; j < count; j++) {
                    entity_id_destroy(ids[j]);
                }
                free(ids);
                return SONORITY_BUILDER_ERR_ALLOC;
            }
            ids = grown;
            capacity = new_capacity;
        }
        EntityID *clone = clone_entity_id(note_event_id(notes[i]));
        if (clone == NULL) {
            for (size_t j = 0; j < count; j++) {
                entity_id_destroy(ids[j]);
            }
            free(ids);
            return SONORITY_BUILDER_ERR_ALLOC;
        }
        ids[count++] = clone;
    }

    *out_ids = ids;
    *out_count = count;
    return SONORITY_BUILDER_OK;
}

/*
 * Builds the Sonority for one non-empty sample point at grid_time,
 * taking ownership of active_ids (the array itself is freed here either
 * way; each EntityID it holds is either handed to the new Sonority or,
 * on failure, destroyed).
 */
static SonorityBuilderResult make_sonority_at(const MusicalTime *grid_time, EntityID **active_ids,
                                               size_t active_count, Sonority **out) {
    Duration *beat_duration;
    if (duration_create(SONORITY_BUILDER_BEAT_NUMERATOR, SONORITY_BUILDER_BEAT_DENOMINATOR, &beat_duration) !=
        DURATION_OK) {
        for (size_t i = 0; i < active_count; i++) {
            entity_id_destroy(active_ids[i]);
        }
        free(active_ids);
        return SONORITY_BUILDER_ERR_ALLOC;
    }

    MusicalTime *window_end;
    MusicalTimeResult add_result = musical_time_add(grid_time, beat_duration, &window_end);
    duration_destroy(beat_duration);
    if (add_result != MUSICAL_TIME_OK) {
        for (size_t i = 0; i < active_count; i++) {
            entity_id_destroy(active_ids[i]);
        }
        free(active_ids);
        return SONORITY_BUILDER_ERR_ALLOC;
    }

    /* time_span_create copies start/end rather than taking ownership
     * (time/time_span.c) -- grid_time and window_end are destroyed by
     * the caller/here regardless of what time_span_create does. */
    TimeSpan *window;
    TimeSpanResult span_result = time_span_create(grid_time, window_end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &window);
    musical_time_destroy(window_end);
    if (span_result != TIME_SPAN_OK) {
        for (size_t i = 0; i < active_count; i++) {
            entity_id_destroy(active_ids[i]);
        }
        free(active_ids);
        return SONORITY_BUILDER_ERR_ALLOC;
    }

    Sonority *sonority;
    SonorityResult sonority_result =
        sonority_create(window, active_ids, active_count, NULL, 0, SONORITY_EVENT_DOMAIN_SOUNDING, &sonority);
    /* sonority_create, on success, takes ownership of window and every
     * EntityID* in active_ids -- but not the active_ids array itself. */
    free(active_ids);
    if (sonority_result != SONORITY_OK) {
        time_span_destroy(window);
        return SONORITY_BUILDER_ERR_ALLOC;
    }

    *out = sonority;
    return SONORITY_BUILDER_OK;
}

SonorityBuilderResult sonority_builder_build_from_notes(NoteEvent *const *notes, size_t note_count,
                                                          Sonority ***out_sonorities, size_t *out_sonority_count) {
    if (notes == NULL || note_count == 0) {
        return SONORITY_BUILDER_ERR_NULL_NOTES;
    }
    for (size_t i = 0; i < note_count; i++) {
        if (notes[i] == NULL) {
            return SONORITY_BUILDER_ERR_NULL_NOTE;
        }
    }

    /* The grid is sampled from beat 0 up to (but not including) the
     * latest release among all notes. */
    MusicalTime *max_release = NULL;
    for (size_t i = 0; i < note_count; i++) {
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(notes[i]));
        MusicalTime *release;
        if (musical_time_add(onset, note_event_duration(notes[i]), &release) != MUSICAL_TIME_OK) {
            musical_time_destroy(max_release);
            return SONORITY_BUILDER_ERR_ALLOC;
        }
        if (max_release == NULL || musical_time_compare(release, max_release) > 0) {
            musical_time_destroy(max_release);
            max_release = release;
        } else {
            musical_time_destroy(release);
        }
    }

    Sonority **sonorities = NULL;
    size_t sonority_count = 0;
    size_t sonority_capacity = 0;

    for (long k = 0;; k++) {
        MusicalTime *grid_time;
        if (musical_time_create(k * SONORITY_BUILDER_BEAT_NUMERATOR, SONORITY_BUILDER_BEAT_DENOMINATOR,
                                 &grid_time) != MUSICAL_TIME_OK) {
            musical_time_destroy(max_release);
            for (size_t i = 0; i < sonority_count; i++) {
                sonority_destroy(sonorities[i]);
            }
            free(sonorities);
            return SONORITY_BUILDER_ERR_ALLOC;
        }
        if (musical_time_compare(grid_time, max_release) >= 0) {
            musical_time_destroy(grid_time);
            break;
        }

        EntityID **active_ids;
        size_t active_count;
        SonorityBuilderResult collect_result =
            collect_active_ids_at(notes, note_count, grid_time, &active_ids, &active_count);
        if (collect_result != SONORITY_BUILDER_OK) {
            musical_time_destroy(grid_time);
            musical_time_destroy(max_release);
            for (size_t i = 0; i < sonority_count; i++) {
                sonority_destroy(sonorities[i]);
            }
            free(sonorities);
            return collect_result;
        }

        if (active_count == 0) {
            musical_time_destroy(grid_time);
            continue;
        }

        Sonority *sonority;
        SonorityBuilderResult make_result = make_sonority_at(grid_time, active_ids, active_count, &sonority);
        musical_time_destroy(grid_time);
        if (make_result != SONORITY_BUILDER_OK) {
            musical_time_destroy(max_release);
            for (size_t i = 0; i < sonority_count; i++) {
                sonority_destroy(sonorities[i]);
            }
            free(sonorities);
            return make_result;
        }

        if (sonority_count == sonority_capacity) {
            size_t new_capacity = sonority_capacity == 0 ? 4 : sonority_capacity * 2;
            Sonority **grown = (Sonority **)realloc(sonorities, sizeof(Sonority *) * new_capacity);
            if (grown == NULL) {
                sonority_destroy(sonority);
                musical_time_destroy(max_release);
                for (size_t i = 0; i < sonority_count; i++) {
                    sonority_destroy(sonorities[i]);
                }
                free(sonorities);
                return SONORITY_BUILDER_ERR_ALLOC;
            }
            sonorities = grown;
            sonority_capacity = new_capacity;
        }
        sonorities[sonority_count++] = sonority;
    }

    musical_time_destroy(max_release);
    *out_sonorities = sonorities;
    *out_sonority_count = sonority_count;
    return SONORITY_BUILDER_OK;
}
