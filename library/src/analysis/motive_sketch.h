#ifndef MUSIC_ANALYSIS_MOTIVE_SKETCH_H
#define MUSIC_ANALYSIS_MOTIVE_SKETCH_H

#include <stdbool.h>
#include <stddef.h>

#include "core/rational.h"
#include "time/time_rational.h"

/*
 * MotiveSketch (L8 analysis value): a motive as plain data, the input of the motive relations
 * (analysis/motive_relation.h) and of the variant recognizer. Notes carry a MIDI pitch or
 * MOTIVE_SKETCH_PITCH_UNKNOWN (a rhythm sketch), exact onsets and durations, and the sketch may carry a
 * scale, a meter, chords, labelled features and a role. It is theory-neutral: nothing here knows a
 * composer's motive type, and nothing is allocated.
 *
 * Times are Rational counts of whole notes from the start of the sketch. bar and beat are the
 * lengths of a bar and a beat; pickup is the length before the first downbeat (0 when the sketch starts
 * on one, below bar otherwise). length is the end of the last note.
 *
 * A time that overflowed is the "bad" Rational {0, 0} (msk_bad): the msk_ helpers below pass it on
 * through every operation, and motive_sketch_validate refuses any sketch holding one.
 */

#define MOTIVE_SKETCH_MAX_NOTES 64
#define MOTIVE_SKETCH_MAX_CHORDS 8
#define MOTIVE_SKETCH_MAX_FEATURES 8
#define MOTIVE_SKETCH_LABEL_SIZE 9
#define MOTIVE_SKETCH_PITCH_UNKNOWN (-1)

typedef struct {
    int pitch;
    Rational onset;
    Rational duration;
} MotiveSketchNote;

/* degree 1 to 7, alteration -1/0/1, inversion 0 to 3. */
typedef struct {
    Rational onset;
    int degree;
    int alteration;
    int inversion;
    bool seventh;
} MotiveSketchChord;

/* label 1 to 8 characters; first and last are 0-based note indices, inclusive. */
typedef struct {
    char label[MOTIVE_SKETCH_LABEL_SIZE];
    size_t first;
    size_t last;
} MotiveSketchFeature;

typedef struct {
    MotiveSketchNote notes[MOTIVE_SKETCH_MAX_NOTES];
    size_t note_count;
    bool has_scale;
    int tonic_pc; /* 0 to 11 */
    bool minor;   /* natural minor steps */
    Rational bar;
    Rational beat;
    Rational pickup;
    Rational length;
    MotiveSketchChord chords[MOTIVE_SKETCH_MAX_CHORDS];
    size_t chord_count;
    MotiveSketchFeature features[MOTIVE_SKETCH_MAX_FEATURES];
    size_t feature_count;
    bool accompaniment;
} MotiveSketch;

/* --- checked rational helpers ------------------------------------------- */

static inline Rational msk_bad(void) {
    Rational r = {0, 0};
    return r;
}

static inline bool msk_ok(Rational r) {
    return r.den != 0;
}

static inline Rational msk_make(unsigned long long n, unsigned long long d) {
    return d == 0 ? msk_bad() : rational_ratio(n, d);
}

static inline Rational msk_add(Rational a, Rational b) {
    Rational out;
    if (!msk_ok(a) || !msk_ok(b) || rational_add(a, b, &out) != RATIONAL_OK) {
        return msk_bad();
    }
    return out;
}

/* a - b; bad when a < b. */
static inline Rational msk_sub(Rational a, Rational b) {
    Rational out;
    if (!msk_ok(a) || !msk_ok(b) || rational_cmp(a, b) < 0 || rational_sub(a, b, &out) != RATIONAL_OK) {
        return msk_bad();
    }
    return out;
}

static inline Rational msk_mul(Rational a, Rational b) {
    Rational out;
    if (!msk_ok(a) || !msk_ok(b) || rational_mul(a, b, &out) != RATIONAL_OK) {
        return msk_bad();
    }
    return out;
}

/* a / b; bad when b is 0. */
static inline Rational msk_div(Rational a, Rational b) {
    if (!msk_ok(a) || !msk_ok(b) || b.num == 0) {
        return msk_bad();
    }
    Rational inverse = rational_ratio(b.den, b.num);
    return msk_mul(a, inverse);
}

static inline int msk_cmp(Rational a, Rational b) {
    return rational_cmp(a, b);
}

static inline bool msk_eq(Rational a, Rational b) {
    return a.num == b.num && a.den == b.den;
}

static inline bool msk_is_zero(Rational r) {
    return msk_ok(r) && r.num == 0;
}

/* The whole number of times b goes into a, rounded down; 0 for a bad value. */
static inline unsigned long long msk_floor_div(Rational a, Rational b) {
    if (!msk_ok(a) || !msk_ok(b) || b.num == 0) {
        return 0;
    }
    Rational q = msk_div(a, b);
    return msk_ok(q) ? q.num / q.den : 0;
}

/* a minus the largest multiple of b not above it. */
static inline Rational msk_mod(Rational a, Rational b) {
    Rational whole = msk_make(msk_floor_div(a, b), 1);
    return msk_sub(a, msk_mul(whole, b));
}

static inline Rational msk_int(unsigned long long n) {
    return rational_ratio(n, 1);
}

/* --- the value ---------------------------------------------------------- */

bool motive_sketch_equal(const MotiveSketch *a, const MotiveSketch *b);

/*
 * FNV-1a 64 over a canonical byte form of every field the sketch carries: the notes, the scale, the meter,
 * the pickup and length, the chords, the features and the role. Equal sketches (motive_sketch_equal) give
 * equal fingerprints; a changed onset, pitch, scale or meter changes it. Only the used entries of the
 * arrays count, and a tonic and mode only when has_scale. Stable across runs and platforms, so a cache can
 * key on it.
 */
unsigned long long motive_sketch_fingerprint(const MotiveSketch *sketch);

/*
 * True when the sketch is well formed: 1 to MAX_NOTES notes with a pitch unknown or 0 to 127, durations above
 * zero, onsets ascending and not overlapping; bar and beat above zero and pickup below bar; length not before
 * the end of the last note; chords ascending inside the length with a degree 1 to 7, alteration -1 to 1 and
 * inversion 0 to 3; features with a 1 to 8 character label, inside the notes; no bad time anywhere.
 */
bool motive_sketch_validate(const MotiveSketch *sketch);

/*
 * The sketch as its contour: the pitches are replaced by dense ranks over the distinct pitches (0 the
 * lowest, equal pitches equal ranks), rank r on the step r of the sketch's scale (C major without one) from
 * middle C, every note a quarter and note i at onset i/4. length is the end of the last note; bar, beat,
 * pickup, scale, chords, features and role are kept. False, out untouched, when a pitch is unknown or over
 * 127 or a rank is 64 or more. in and out may be the same.
 */
bool motive_sketch_contour_normalize(const MotiveSketch *in, MotiveSketch *out);

/* --- scale, harmony and meter helpers ------------------------------------ */

/*
 * The step of the sketch's scale a pitch is on: octaves of 7 from the tonic's pitch class, and the degree in
 * the octave. A pitch outside the scale is on the degree below it, raised (*alteration 1). Without a scale the
 * step is the pitch itself (semitones) and *alteration is 0. alteration may be NULL.
 */
long motive_sketch_step(const MotiveSketch *sketch, int pitch, int *alteration);

/* The inverse: the pitch at a step with an alteration. Not range-checked. */
int motive_sketch_pitch_at_step(const MotiveSketch *sketch, long step, int alteration);

/* The degree 1 to 7 of a step (0 without a scale). */
int motive_sketch_degree_of_step(const MotiveSketch *sketch, long step);

/*
 * The pitch classes of a chord, root first, then third, fifth and, with a seventh, the seventh: the sketch's
 * scale stacked in thirds from the root. An altered chord is a major triad (a dominant seventh) on the altered
 * root; in minor, degree 5 has its third raised. Returns how many (3 or 4), 0 without a scale. The inversion
 * does not reorder them.
 */
size_t motive_sketch_chord_tones(const MotiveSketch *sketch, const MotiveSketchChord *chord, int pcs[4]);

/* A duration a note list can write: w h q e s t, plain or with up to two dots. */
bool motive_sketch_expressible(Rational duration);

/* An onset on the grid: a multiple of a thirty-second. */
bool motive_sketch_on_grid(Rational onset);

/* How firmly an onset sits in the bar: 2 on the downbeat, 1 on another beat, 0 off the beat. */
int motive_sketch_strength(const MotiveSketch *sketch, Rational onset);

#endif
