#ifndef MUSIC_ANALYSIS_MOTIVE_CHORD_INTERNAL_H
#define MUSIC_ANALYSIS_MOTIVE_CHORD_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/motive_sketch.h"

/*
 * Internal to the motive analysis: how a sketch's chords are looked up and compared, shared by the harmony
 * relations (motive_relation.c) and the tone roles (motive_tone_roles.c). Not part of the public API.
 */

static inline int mch_pitch_class(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

static inline bool mch_has_pc(const int *pcs, size_t count, int pc) {
    for (size_t i = 0; i < count; i++) {
        if (pcs[i] == pc) {
            return true;
        }
    }
    return false;
}

/* The chord in force at an onset: the last one at or before it, NULL before the first. */
static inline const MotiveSketchChord *mch_chord_at(const MotiveSketch *sketch, Rational onset) {
    const MotiveSketchChord *found = NULL;
    for (size_t c = 0; c < sketch->chord_count; c++) {
        if (msk_cmp(sketch->chords[c].onset, onset) <= 0) {
            found = &sketch->chords[c];
        }
    }
    return found;
}

/* Whether two chords hold the same pitch classes in the sketch's scale. */
static inline bool mch_same_tones(const MotiveSketch *sketch, const MotiveSketchChord *a,
                                  const MotiveSketchChord *b) {
    int x[4];
    int y[4];
    size_t nx = motive_sketch_chord_tones(sketch, a, x);
    size_t ny = motive_sketch_chord_tones(sketch, b, y);
    if (nx != ny) {
        return false;
    }
    for (size_t i = 0; i < nx; i++) {
        if (!mch_has_pc(y, ny, x[i])) {
            return false;
        }
    }
    return true;
}

#endif
