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
#define VARIETY_POOL_MAX 8     /* the most candidates a variety pool draws (COMPOSER_PREFERENCES_POOL_MAX) */
#define VARIETY_MAX 20         /* the largest variety, the weight of distance against closeness to the style */

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
    long ticks_per_unit; /* ticks in the beat unit; 0 is the drawn strategy's 2 (half-beats) */
} Rhythm;

typedef struct {
    int pitches[MAX_NOTES];
    bool decorated[MAX_NOTES]; /* a non-chord tone of types[i] */
    NonChordTone types[MAX_NOTES];
    bool run[MAX_NOTES]; /* a note of a run between two frame notes (segments strategy): a chord tone or a non-chord tone */
} Melody;

long motive_random_rung_position(long rung);
bool motive_random_is_chord_step(long position);
/* Whether every color suffix of the given plan names a type that fits its chord in the key (R-727). */
bool motive_random_segments_plan_colors_fit(const MotiveRandomOptions *options, const SpelledKey *key);
/* True when the step of `position` (from the chord's root) is one of a color chord's added members: bit s of `mask` is step s. */
bool motive_random_is_color_step(unsigned mask, long position);
bool motive_random_in_range(int pitch);

/* The figure a note makes between the pitch before and the pitch after it, by the semitone rules of the tone roles. */
bool motive_random_forms(NonChordTone type, int before, int pitch, int after);

/*
 * The frame: a walk on the chord tones of I from degree 1, 3 or 5, for the first and last note and every note that starts
 * on a beat. The draw fails when a note leaves PITCH_LOW to PITCH_HIGH.
 */
bool motive_random_draw_frame(const SpelledKey *key, const Rhythm *rhythm, FillRng *rng, long *positions, Melody *melody);

/* Fills the notes between two frame notes with a chord tone or a non-chord tone that makes its figure. */
bool motive_random_draw_between(const SpelledKey *key, const Rhythm *rhythm, FillRng *rng, long *positions,
                                Melody *melody);

/* The notes, pickup and length of a candidate, in the key. False when a time does not fit a Rational. */
bool motive_random_build(const MotiveRandomOptions *options, const SpelledKey *key, const Rhythm *rhythm,
                         const Melody *melody, Motive *out);

/* The form of a full motive, read back by the reader of that form. */
bool motive_random_apply_form(const MotiveRandomOptions *options, const Motive *full, Motive *out);

bool motive_random_already_made(const Motive *made, size_t count, const Motive *motive);

/* How close a full candidate is to the style, of the category the form keeps. False on allocation failure. */
bool motive_random_candidate_closeness(const MotiveRandomOptions *options, const Motive *full, Rational *out);

/*
 * How far apart two motives sound (R-673): 1 - (rhythm similarity + contour similarity) / 2, from the library's similarity
 * scores of their durations and of the directions of their intervals; 0 for the same, up to 1. A motive of fewer than two
 * notes has no contour: two such motives agree on it, one such and one longer do not.
 */
double motive_random_distance(const Motive *a, const Motive *b);

/* One candidate of the SEGMENTS strategy (motive_random_segments.c), as the DRAWN one: *found is false when the draw did
   not pass (try again); the result is false only when the check itself could not run. */
bool motive_random_segments_candidate(const MotiveRandomOptions *options, const SpelledKey *key, FillRng *rng, Motive *out,
                                      bool *found);

/* True when a given chord plan (preferences->plan_count > 0) can be drawn in the meter (motive_random_segments.c); false means
   the plan never fits, whatever the seed. */
bool motive_random_segments_plan_fits(const MotiveRandomPreferences *preferences, const PlanMeter *meter);


/* --- the SEGMENTS strategy, shared by its files (motive_random_segments.c draws the motive, _rhythm.c the cells, _layout.c the layout, _pitch.c the pitches) --- */

#define SEGMENT_MIN_CELL_BEATS 2 /* a cell must hold a feature: the shortest cell is two beats */
#define TICKS_PER_UNIT 12        /* ticks in the meter's beat unit: halves, thirds and quarters of it are whole */
#define SEGMENT_MAX 3
#define CHORD_MAX MOTIVE_RANDOM_PLAN_MAX /* the chords of a motive: one per segment, or a given plan's */
#define SEGMENT_TRIES_PER_SETUP 8 /* notes drawn within one bars, shape and plan before another is drawn (a candidate) */
#define SEGMENT_CELL_TRIES 32 /* a cell that does not fit is drawn again, so a failed cell does not change the shape's odds */
#define SEGMENT_START_REACH 4 /* rungs either side of the note before, searched for where a new segment starts */
#define SEGMENT_START_DISTANCE 5 /* the most semitones from the note before to where a new segment starts */
#define LEAP_SEMITONES 5 /* an interval this large or larger is a leap (leaps_per_motif, leap_recovery) */
#define SKIP_SEMITONES 3 /* an interval of 3 or 4 semitones is a skip (skips_per_motif) */

/* Syncopation figures (R-454): the two inside a cell, then the two across a segment boundary. */
typedef enum { SYNC_OFF_BEAT = 0, SYNC_TIE_BEAT, SYNC_TIE_BAR, SYNC_ANTICIPATION, SYNC_TYPES } Sync;
#define SYNC_LEVELS 2 /* eighth, sixteenth */

/* Shapes: a a' (a cell and its repeat), a b (a cell and a new one), a a' b (two half-bar cells, then a bar). */
typedef enum { SHAPE_REPEAT = 0, SHAPE_CONTRAST, SHAPE_REPEAT_CONTRAST, SHAPE_KINDS } Shape;

/* Harmony plans, one chord (a degree) per segment, and how often each is drawn (ch. V). */
typedef struct {
    size_t count;
    int degrees[SEGMENT_MAX];
} SegmentPlan;
extern const SegmentPlan PLANS_TWO[3];
extern const SegmentPlan PLANS_THREE[2];
#define PLAN_KINDS_TWO 3
#define PLAN_KINDS_THREE 2

/* The distinctive feature of segment a. */
typedef enum { FEATURE_DOTTED = 0, FEATURE_LEAP, FEATURE_REPEAT, FEATURE_KINDS } Feature;

/* How the last segment ends (the endings list, R-440). */
typedef enum { ENDING_FREE = 0, ENDING_CLOSED, ENDING_OPEN, ENDING_LEAD_IN, ENDING_KINDS } Ending;

/*
 * The tick grid of a meter: the bar's beats, the ticks in a beat and in half of it, and whether the beat is a dotted unit.
 * Simple meters beat once per beat unit (12 ticks); a compound meter with compound_beats beats once per three. It also
 * carries the ending drawn for the motive and `last_offset`, the ticks from its barline to where the last segment's cell
 * starts (draw_setup sets both).
 */
typedef struct {
    long beats;
    long beat;
    long half;
    bool compound;
    Ending ending;
    long last_offset;
} Grid;


typedef struct {
    long ticks[MAX_NOTES];
    size_t count;
} Cell;

typedef struct {
    Rhythm rhythm;
    size_t segment_count;
    size_t first[SEGMENT_MAX];
    size_t last[SEGMENT_MAX];
    bool repeats[SEGMENT_MAX]; /* the segment repeats segment a's rhythm */
    SegmentPlan plan;
    size_t cell_start; /* the first note of segment a's cell: after the pickup note, if any */
    Grid grid;
    long start[SEGMENT_MAX];     /* ticks from the motive's start, the pickup included, to where each segment's cell starts */
    Sync boundary[SEGMENT_MAX];  /* SYNC_TIE_BAR or SYNC_ANTICIPATION at the start of segment s >= 1, else SYNC_TYPES */
    size_t chord_count;
    int chord_degree[CHORD_MAX];
    long chord_onset[CHORD_MAX]; /* ticks from the motive's start, the pickup included, to where each chord starts */
    const ChordType *chord_color[CHORD_MAX]; /* the color type of each chord, NULL for a plain triad (R-727) */
    unsigned chord_color_mask[CHORD_MAX];    /* the steps above its root a chord's color members sit on (bit 0 to 6) */
    int degree[MAX_NOTES];       /* the chord at each note's onset (set_note_chords) */
    unsigned color_mask[MAX_NOTES]; /* chord_color_mask of the chord at each note's onset */
    bool chord_first[MAX_NOTES]; /* the first note on a chord after the first */
} Layout;

typedef struct {
    Grid grid;
    long bars;
    long pickup;
    Shape shape;
    SegmentPlan plan;
    bool placed; /* a given plan whose chords are at a bar and beat, not one to a segment */
    const ChordType *chord_color[CHORD_MAX]; /* the color type of each chord of the plan, drawn or given (NULL: none) */
    unsigned chord_color_mask[CHORD_MAX];
} Setup;

Grid motive_random_grid_of(const MotiveRandomPreferences *preferences, const PlanMeter *meter);
bool motive_random_simple_meter(const Grid *grid);
long motive_random_sync_unit(const Grid *grid, size_t level);
size_t motive_random_pick_weighted(FillRng *rng, const unsigned long *weights, size_t count);
long motive_random_chord_shift(int degree);
long motive_random_step_of(long position);
int motive_random_chord_alteration(const SpelledKey *key, int degree, long position);
bool motive_random_draw_cell_a(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long length, Feature feature, bool long_end, Cell *cell);
bool motive_random_draw_cell_b(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long length, const Cell *not_like, Cell *cell);
void motive_random_draw_setup(const MotiveRandomPreferences *preferences, const PlanMeter *meter, FillRng *rng, Setup *setup);
bool motive_random_draw_layout(const MotiveRandomPreferences *preferences, const Setup *setup, FillRng *rng, Layout *layout, Feature *feature_out);
bool motive_random_has_halves(const Grid *grid);
bool motive_random_plan_bars_fit(const MotiveRandomPreferences *preferences, bool halves, long bars);
bool motive_random_draw_melody(const MotiveRandomPreferences *preferences, const SpelledKey *key, const Layout *layout, FillRng *rng, Feature feature, Melody *melody);

#endif
