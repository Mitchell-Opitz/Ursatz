#ifndef URSATZ_COMPOSER_MOTIVE_RANDOM_INTERNAL_H
#define URSATZ_COMPOSER_MOTIVE_RANDOM_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_random.h>
#include <ursatz_composer/style.h>

#include "motive/motive_internal.h"

/*
 * What the two random motive strategies share (motive_random.c draws motives, motive_random_segments.c builds them from
 * segments): the limits every random motive keeps, the chord-tone frame and the non-chord tone decorations, the
 * rhythm and melody records, and the steps that read a candidate back (form, style closeness, repeats).
 */

#define MIN_NOTES 2
#define MAX_NOTES 12
#define MAX_RANGE_SEMITONES 19 /* a twelfth */
#define PITCH_LOW 48           /* C3 */
#define PITCH_HIGH 84          /* C6 */
#define BASE_POSITION 35       /* degree 1 of octave 0: the tonic in MIDI 60 to 71 */
#define BIG_LEAP_RUNGS 2       /* a frame move of this many chord tones or more; the next move is one chord tone back */
#define REPEAT_ONE_IN 8        /* a repeated frame pitch in one move of this many */
#define OFF_FRAME_REACH 5      /* scale steps either side of the frame note before, searched for a non-chord tone */
#define ATTEMPTS_PER_MOTIVE 256
#define STYLE_CANDIDATES 8     /* candidates drawn per motive when there is a style */

/*
 * The frame is a walk on the chord tones of I, the scale steps 0, 2 and 4 of each octave (rungs of a ladder, three to an
 * octave). A move is a number of rungs, one most often, and a motive opens on degree 1, 3 or 5.
 */
#define TRIAD_SIZE 3
#define BASE_RUNG ((BASE_POSITION / 7) * (long)TRIAD_SIZE)

/* What a note between two frame notes may be besides a chord tone (motive_random_common.c). */
extern const NonChordTone MOTIVE_RANDOM_DECORATIONS[];
#define DECORATION_KINDS 4

/* Lengths in half-beats (1, 2 and 4: the beat unit's half, itself and double) and how often each is drawn. */
extern const long MOTIVE_RANDOM_DURATIONS[];
extern const unsigned long MOTIVE_RANDOM_DURATION_WEIGHTS[];
#define DURATION_KINDS 3

typedef struct {
    long ticks[MAX_NOTES];
    bool on_beat[MAX_NOTES];
    size_t count;
    long pickup;
    long total;
} Rhythm;

typedef struct {
    int pitches[MAX_NOTES];
    bool decorated[MAX_NOTES]; /* a non-chord tone of types[i] */
    NonChordTone types[MAX_NOTES];
} Melody;

long motive_random_rung_position(long rung);
bool motive_random_is_chord_step(long position);
bool motive_random_in_range(int pitch);

/* The figure a note makes between the pitch before and the pitch after it, by the semitone rules of the tone roles. */
bool motive_random_forms(NonChordTone type, int before, int pitch, int after);

/*
 * The frame: a walk on the chord tones of I from degree 1, 3 or 5, for the first and last note and every note that starts
 * on a beat. The draw fails when a note leaves PITCH_LOW to PITCH_HIGH.
 */
bool motive_random_draw_frame(const AnalysisKey *key, const Rhythm *rhythm, FillRng *rng, long *positions, Melody *melody);

/* Fills the notes between two frame notes with a chord tone or a non-chord tone that makes its figure. */
bool motive_random_draw_between(const AnalysisKey *key, const Rhythm *rhythm, FillRng *rng, long *positions,
                                Melody *melody);

/* The notes, pickup and length of a candidate, in the key. False when a time does not fit a Rational. */
bool motive_random_build(const MotiveRandomOptions *options, const AnalysisKey *key, const Rhythm *rhythm,
                         const Melody *melody, Motive *out);

/* The form of a full motive, read back by the reader of that form. */
bool motive_random_apply_form(const MotiveRandomOptions *options, const Motive *full, Motive *out);

bool motive_random_already_made(const Motive *made, size_t count, const Motive *motive);

/* How close a full candidate is to the style, of the category the form keeps. False on allocation failure. */
bool motive_random_candidate_closeness(const MotiveRandomOptions *options, const Motive *full, ComposerRatio *out);

/* -1, 0 or 1 as a is less than, equal to or more than b. */
int motive_random_ratio_compare(const ComposerRatio *a, const ComposerRatio *b);

/* One candidate of the SEGMENTS strategy (motive_random_segments.c), as the DRAWN one: *found is false when the draw did
   not pass (try again); the result is false only when the check itself could not run. */
bool motive_random_segments_candidate(const MotiveRandomOptions *options, const AnalysisKey *key, FillRng *rng, Motive *out,
                                      bool *found);

#endif
