#ifndef MUSIC_ANALYSIS_MOTIVE_RELATION_H
#define MUSIC_ANALYSIS_MOTIVE_RELATION_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/motive_sketch.h"

/*
 * MotiveRelation (L8 analysis): the catalog of motive relations, one per operation of a composer's variant
 * registry, each with an apply over a MotiveSketch. A relation turns a motive into a variant of it (a
 * transposition, an inversion, a rhythm made shorter, a harmony changed with the melody adapted to it). The
 * variant recognizer searches chains of these steps; a composer's registry maps its rows to them one to one
 * (the kind's name is the row's name, the parameters are in the row's order).
 *
 * A step is a kind and up to 4 parameter values, in the order motive_relation_param_count counts them. A
 * ratio parameter is value over den (value[i] / den[i]); every other parameter uses value only. An optional
 * parameter that is not given is 0, except the invert axis, which is -1.
 *
 * Parameters by kind (value[0], value[1], ...):
 *   exact
 *   transpose          tonal (0/1), by, degree (0 none; moves the first note to the scale degree, tonal)
 *   invert             axis (MIDI pitch, -1 the first note), tonal (0/1)
 *   retrograde
 *   interval-change    interval (1-based, 0 every one), amount (semitones), keep-contour (0/1)
 *   fragment           position (1 leading, 2 trailing, 3 middle), length, min (0 for 2)
 *   liquidate          drop, keep-rhythm (0/1), min (0 for 2)
 *   augment, diminish  ratio (0:0 for 2:1 and 1:2)
 *   note-repeat        note (0-based), count (2 to 4)
 *   rhythm-repeat      first, length, target (0-based note indices)
 *   rhythm-shift       offset (ratio, later in the bar)
 *   add-upbeat         count (1 to 3), approach (+1 from below, -1 from above)
 *   change-meter       num, den (the new meter, den 1, 2, 4, 8 or 16)
 *   add-interval       interval (1-based), steps (scale steps from the note before the interval, not 0)
 *   omit-interval      interval (1-based; the note after it goes)
 *   ancillary-fill     interval (1-based), kind (0 passing notes, 1 neighbor)
 *   condense           first, last (0-based note indices), ratio (below 1)
 *   repeat-feature     feature (0-based), times (1 or 2), steps
 *   shift-feature      feature (0-based), offset (ratio)
 *   harmony-inversion  chord (0-based), inversion (0 to 3)
 *   harmony-append     degree (1 to 7), seventh (0/1)
 *   harmony-insert     chord (0-based), degree
 *   harmony-substitute chord (0-based), degree, then (a second degree, 0 for none)
 *   passing-harmony    chord (0-based)
 *   counter-line
 *   dominant-form      (ch. V: the chords and the melody in the dominant)
 *   liquidate-features steps (1 to 3), shorten (0/1: also diminished 1:2)
 *
 * A harmony relation needs a scale. A sketch without chords is given its best implied chord at 0 first, so its
 * result has chords. A harmony relation then adapts the melody to the chords it changed: a note on a beat whose chord
 * changed and that is not a chord tone moves to the nearest chord tone, in the direction from the previous note when
 * two are equally near, and keeps the directions of the contour it can.
 *
 * motive_relation_apply never changes its input. The result is as long as its notes (length is the end of the
 * last note).
 */

typedef enum {
    MOTIVE_RELATION_EXACT,
    MOTIVE_RELATION_TRANSPOSE,
    MOTIVE_RELATION_INVERT,
    MOTIVE_RELATION_RETROGRADE,
    MOTIVE_RELATION_INTERVAL_CHANGE,
    MOTIVE_RELATION_FRAGMENT,
    MOTIVE_RELATION_LIQUIDATE,
    MOTIVE_RELATION_AUGMENT,
    MOTIVE_RELATION_DIMINISH,
    MOTIVE_RELATION_NOTE_REPEAT,
    MOTIVE_RELATION_RHYTHM_REPEAT,
    MOTIVE_RELATION_RHYTHM_SHIFT,
    MOTIVE_RELATION_ADD_UPBEAT,
    MOTIVE_RELATION_CHANGE_METER,
    MOTIVE_RELATION_ADD_INTERVAL,
    MOTIVE_RELATION_OMIT_INTERVAL,
    MOTIVE_RELATION_ANCILLARY_FILL,
    MOTIVE_RELATION_CONDENSE,
    MOTIVE_RELATION_REPEAT_FEATURE,
    MOTIVE_RELATION_SHIFT_FEATURE,
    MOTIVE_RELATION_HARMONY_INVERSION,
    MOTIVE_RELATION_HARMONY_APPEND,
    MOTIVE_RELATION_HARMONY_INSERT,
    MOTIVE_RELATION_HARMONY_SUBSTITUTE,
    MOTIVE_RELATION_PASSING_HARMONY,
    MOTIVE_RELATION_COUNTER_LINE,
    MOTIVE_RELATION_DOMINANT_FORM,
    MOTIVE_RELATION_LIQUIDATE_FEATURES,
    MOTIVE_RELATION_KIND_COUNT
} MotiveRelationKind;

#define MOTIVE_RELATION_MAX_PARAMS 4

typedef struct {
    MotiveRelationKind kind;
    long value[MOTIVE_RELATION_MAX_PARAMS];
    long den[MOTIVE_RELATION_MAX_PARAMS]; /* only for ratios */
} MotiveRelationStep;

typedef enum {
    MOTIVE_RELATION_OK = 0,
    MOTIVE_RELATION_NOT_APPLICABLE, /* nothing to do: no pitches, one note has no intervals, too short */
    MOTIVE_RELATION_UNCHANGED,      /* the result is the input */
    MOTIVE_RELATION_RANGE,          /* a pitch would fall outside MIDI 0 to 127 */
    MOTIVE_RELATION_NOT_EXPRESSIBLE, /* the rhythm cannot be written in the meter */
    MOTIVE_RELATION_NO_SCALE,       /* a tonal relation needs a scale */
    MOTIVE_RELATION_NO_HARMONY,     /* a harmony relation needs chords the sketch does not have */
    MOTIVE_RELATION_NO_FEATURE,     /* a feature that does not exist */
    MOTIVE_RELATION_INVALID         /* the sketch or the step is not well formed */
} MotiveRelationResult;

/*
 * Applies step to in. *out is written only on MOTIVE_RELATION_OK. A relation other than exact whose result is the
 * input is refused as UNCHANGED.
 */
MotiveRelationResult motive_relation_apply(const MotiveSketch *in, const MotiveRelationStep *step,
                                           MotiveSketch *out);

size_t motive_relation_count(void);
/* The registry name: "transpose", "interval-change". NULL for a kind out of range. */
const char *motive_relation_name(MotiveRelationKind kind);
/* False for a name that is not a relation. */
bool motive_relation_find(const char *name, MotiveRelationKind *kind);
size_t motive_relation_param_count(MotiveRelationKind kind);
/* The fixed cost of a step (R-142): exact 0; transpose, augment, diminish 1; change-meter 3; every other 2. */
int motive_relation_cost(MotiveRelationKind kind);
/* "ok", "not applicable" and so on. */
const char *motive_relation_result_name(MotiveRelationResult result);

#endif
