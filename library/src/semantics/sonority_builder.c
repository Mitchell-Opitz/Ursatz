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
 * True iff note's onset falls within the half-open beat window
 * [window_start, window_end) -- each note is sampled into exactly the
 * one beat it is struck in. A note that is still sounding (per
 * sonority_builder_note_active_at's onset-release interval) into a
 * LATER beat is deliberately not re-included in that later beat's
 * Sonority: in an arpeggiated texture a note's own duration commonly
 * exceeds one beat (e.g. a held bass tone under faster-moving upper
 * voices), and carrying it forward by sustain would accumulate it into
 * every later beat's Sonority for as long as it rings, contaminating
 * each with a stale tone rather than that beat's own attack content.
 * Grouping by onset keeps each Sonority to what is actually STRUCK on
 * that beat, which is what chord identification needs to read off the
 * beat's own harmony.
 */
static bool note_onset_in_window(const NoteEvent *note, const MusicalTime *window_start,
                                  const MusicalTime *window_end) {
    const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(note));
    return musical_time_compare(onset, window_start) >= 0 && musical_time_compare(onset, window_end) < 0;
}

/*
 * Collects a fresh EntityID clone of every notes[i] whose onset falls in
 * [window_start, window_end) into a newly allocated array. *out_count
 * may legitimately be 0 (no note struck in this beat) -- that is not an
 * error, the caller skips emitting a Sonority for it. On allocation
 * failure, any clones already made are destroyed and
 * SONORITY_BUILDER_ERR_ALLOC is returned.
 */
static SonorityBuilderResult collect_active_ids_in_window(NoteEvent *const *notes, size_t note_count,
                                                            const MusicalTime *window_start,
                                                            const MusicalTime *window_end, EntityID ***out_ids,
                                                            size_t *out_count) {
    EntityID **ids = NULL;
    size_t count = 0;
    size_t capacity = 0;

    for (size_t i = 0; i < note_count; i++) {
        if (!note_onset_in_window(notes[i], window_start, window_end)) {
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
 * Builds the Sonority for one non-empty beat window [window_start,
 * window_end), taking ownership of active_ids (the array itself is
 * freed here either way; each EntityID it holds is either handed to the
 * new Sonority or, on failure, destroyed).
 */
static SonorityBuilderResult make_sonority_for_window(const MusicalTime *window_start, const MusicalTime *window_end,
                                                        EntityID **active_ids, size_t active_count, Sonority **out) {
    /* time_span_create copies start/end rather than taking ownership
     * (time/time_span.c) -- window_start and window_end stay owned by
     * the caller either way. */
    TimeSpan *window;
    TimeSpanResult span_result =
        time_span_create(window_start, window_end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &window);
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

    Duration *beat_duration;
    if (duration_create(SONORITY_BUILDER_BEAT_NUMERATOR, SONORITY_BUILDER_BEAT_DENOMINATOR, &beat_duration) !=
        DURATION_OK) {
        musical_time_destroy(max_release);
        return SONORITY_BUILDER_ERR_ALLOC;
    }

    Sonority **sonorities = NULL;
    size_t sonority_count = 0;
    size_t sonority_capacity = 0;

    for (long k = 0;; k++) {
        MusicalTime *window_start;
        if (musical_time_create(k * SONORITY_BUILDER_BEAT_NUMERATOR, SONORITY_BUILDER_BEAT_DENOMINATOR,
                                 &window_start) != MUSICAL_TIME_OK) {
            duration_destroy(beat_duration);
            musical_time_destroy(max_release);
            for (size_t i = 0; i < sonority_count; i++) {
                sonority_destroy(sonorities[i]);
            }
            free(sonorities);
            return SONORITY_BUILDER_ERR_ALLOC;
        }
        if (musical_time_compare(window_start, max_release) >= 0) {
            musical_time_destroy(window_start);
            break;
        }

        MusicalTime *window_end;
        if (musical_time_add(window_start, beat_duration, &window_end) != MUSICAL_TIME_OK) {
            musical_time_destroy(window_start);
            duration_destroy(beat_duration);
            musical_time_destroy(max_release);
            for (size_t i = 0; i < sonority_count; i++) {
                sonority_destroy(sonorities[i]);
            }
            free(sonorities);
            return SONORITY_BUILDER_ERR_ALLOC;
        }

        EntityID **active_ids;
        size_t active_count;
        SonorityBuilderResult collect_result =
            collect_active_ids_in_window(notes, note_count, window_start, window_end, &active_ids, &active_count);
        if (collect_result != SONORITY_BUILDER_OK) {
            musical_time_destroy(window_start);
            musical_time_destroy(window_end);
            duration_destroy(beat_duration);
            musical_time_destroy(max_release);
            for (size_t i = 0; i < sonority_count; i++) {
                sonority_destroy(sonorities[i]);
            }
            free(sonorities);
            return collect_result;
        }

        if (active_count == 0) {
            musical_time_destroy(window_start);
            musical_time_destroy(window_end);
            continue;
        }

        Sonority *sonority;
        SonorityBuilderResult make_result =
            make_sonority_for_window(window_start, window_end, active_ids, active_count, &sonority);
        musical_time_destroy(window_start);
        musical_time_destroy(window_end);
        if (make_result != SONORITY_BUILDER_OK) {
            duration_destroy(beat_duration);
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
                duration_destroy(beat_duration);
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

    duration_destroy(beat_duration);
    musical_time_destroy(max_release);
    *out_sonorities = sonorities;
    *out_sonority_count = sonority_count;
    return SONORITY_BUILDER_OK;
}
