#ifndef URSATZ_COMPOSER_MOTIVE_RANDOM_H
#define URSATZ_COMPOSER_MOTIVE_RANDOM_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/style.h>

/*
 * Random motives. The caller names a mode and a meter, and optionally a key, a form and a style; the motive's length,
 * pickup, number of notes, rhythm and range are not options (R-120). They are drawn inside the defaults named at the top
 * of motive_random.c (DRAWN) and motive_random_segments.c (SEGMENTS), which are meant to be tuned there.
 *
 * Every motive keeps standard-counterpoint's hard melodic rules, the ones motive invention keeps. The same options and
 * seed give the same motives: all the random numbers come from one FillRng seeded with `seed`.
 *
 * Key. Without a key the motive is made and sounded in C major (major) or A minor (minor) and carries that key, marked
 * MOTIVE_SKETCH_KEY_GIVEN, so it can be stored. A key must be of the mode asked for.
 *
 * Form. A full motive is pitches and rhythm. A rhythm motive is the rhythm of a full one, and a contour motive its
 * contour, read back as motive_from_rhythm_text and motive_from_contour_text read them (a contour has no pickup).
 *
 * Style. With a style, 8 candidates are made for each motive asked for and the one closest to the style is kept (ties
 * broken by the seed): composer_style_closeness on the candidate's melody labels, only the labels the form keeps
 * (duration for a rhythm, contour for a contour).
 */

#define MOTIVE_RANDOM_MAX_COUNT 16

/*
 * Strategy. SEGMENTS (the zero value, so the default) builds a motive from a harmony plan and segments, the way a motive is
 * made: a rhythm cell, a repeat of it on the next chord, a contrasting cell, an ending that slows down. DRAWN is the
 * earlier strategy: a rhythm drawn note by note over a frame of chord tones of I.
 */
typedef enum {
    MOTIVE_RANDOM_STRATEGY_SEGMENTS = 0,
    MOTIVE_RANDOM_STRATEGY_DRAWN
} MotiveRandomStrategy;

/* "segments" or "drawn". */
const char *motive_random_strategy_name(MotiveRandomStrategy strategy);

/* Reads "segments" or "drawn" (as written, any case). False for any other text. */
bool motive_random_strategy_parse(const char *name, MotiveRandomStrategy *out);

typedef enum {
    MOTIVE_RANDOM_OK = 0,
    MOTIVE_RANDOM_ERR_NULL,
    MOTIVE_RANDOM_ERR_ARGUMENT,      /* a mode, form or count that is not allowed */
    MOTIVE_RANDOM_ERR_METER,         /* beats per bar from 1 to 64 over a beat unit of 1, 2, 4, 8 or 16 */
    MOTIVE_RANDOM_ERR_KEY,           /* a key that is not of the mode asked for */
    MOTIVE_RANDOM_ERR_UNSATISFIABLE, /* no motive passed the melodic rules in the attempts allowed */
    MOTIVE_RANDOM_ERR_ALLOC
} MotiveRandomResult;

/*
 * What shapes a SEGMENTS motive (R-411). Every weighted list is whole numbers; a list whose entries sum to 0 draws its
 * first entry, with no random number. The defaults are the values the generator was tuned to; with them (or with no
 * preferences) a seed gives the motives it always gave. The DRAWN strategy does not read these.
 *
 *   bars              a motive of one bar, of two bars (one bar only in a meter of an even number of beats, four or more)
 *   pickup_one_in     a pickup (half a beat or a beat) in one motive of this many; 0 never draws one
 *   shapes_one_bar    a a' (repeat), a b (contrast), for a one-bar motive
 *   shapes_two_bars   a a', a b, a a' b (repeat_contrast), for a two-bar motive (the third only with an even meter)
 *   plans_two         the chords of a two-segment motive: I-V, I-IV, I-I
 *   plans_three       the chords of a three-segment motive: I-V-I, I-IV-I
 *   features          the distinctive feature of segment a: dotted figure, leap, repeated note
 *   leap_low/high     the size of a leap, in semitones (a fourth to a sixth)
 *   rung_moves        a walk on the chord tones moves one rung (step) or two (skip)
 *   decorations       a note between frame notes: chord tone, passing, neighbor, appoggiatura, escape
 *   durations         half beat, beat, two beats
 */
typedef struct {
    unsigned long bars[2];
    unsigned pickup_one_in;
    unsigned long shapes_one_bar[2];
    unsigned long shapes_two_bars[3];
    unsigned long plans_two[3];
    unsigned long plans_three[2];
    unsigned long features[3];
    int leap_low, leap_high;
    unsigned long rung_moves[2];
    unsigned long decorations[5];
    unsigned long durations[3];
} MotiveRandomPreferences;

/* The defaults. */
void motive_random_preferences_default(MotiveRandomPreferences *out);

typedef struct {
    KeyStructureMode mode;
    PlanMeter meter;
    bool has_key;
    AnalysisKey key;
    MotiveSketchForm form;
    /* NULL for no style. Not owned. */
    const ComposerStyle *style;
    /* How the motive is made; the zero value is SEGMENTS. */
    MotiveRandomStrategy strategy;
    /* How many motives, 1 to MOTIVE_RANDOM_MAX_COUNT. */
    size_t count;
    unsigned long seed;
    /* The SEGMENTS strategy's settings. NULL for the defaults; not owned. */
    const MotiveRandomPreferences *preferences;
} MotiveRandomOptions;

/* "ok", "meter" and so on. */
const char *motive_random_result_name(MotiveRandomResult result);

/*
 * Makes options->count different motives into out, which holds at least that many, and writes how many to *out_count
 * (all of them on success, 0 on failure).
 */
MotiveRandomResult motive_random(const MotiveRandomOptions *options, Motive *out, size_t *out_count);

#endif
