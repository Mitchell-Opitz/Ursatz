#include "semantics/sonority_builder.h"

#include "time/temporal_anchor.h"

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
