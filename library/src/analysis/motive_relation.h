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
 *   liquidate          drop, keep-rhythm (0/1: the kept notes keep their onsets and each runs to the next kept
 *                      note, the last to the end, so a dropped note leaves no rest), min (0 for 2)
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
 *   reduce-to-frame    (every non-chord and unclassified note goes; its time to the note before it, a leading
 *                      one's to the next kept note)
 *   add-non-chord-tone after (0-based index of a chord tone with a note after it), type (a NonChordTone value,
 *                      semantics/non_chord_tone.h): the figure goes between that note and the next, taking half of
 *                      the earlier note's duration (a double neighbor or changing tone, two notes, half of it split
 *                      in two)
 *   remove-non-chord-tone note (0-based index of a note that is not a chord tone; its time to the note before it,
 *                      a first note's to the next)
 *   change-non-chord-tone note (0-based index of a note that is not a chord tone), type (a NonChordTone value):
 *                      the note, and for a double neighbor or changing tone its partner, move to form that type
 *                      between the same chord tones
 *   reharmonize-frame  degree (1 to 7), seventh (0/1): every chord is replaced by that one chord at 0, the melody
 *                      adapted as a harmony relation does
 *
 * A harmony relation needs a scale. A sketch without chords is given its best implied chord at 0 first, so its
 * result has chords. A harmony relation then adapts the melody to the chords it changed, through the tone roles
 * (analysis/motive_tone_roles.h) of the melody before: a note that was a chord tone and is not one of its new chord
 * moves to the nearest tone of the new chord, in the direction from the previous note when two are equally near, and
 * keeps the directions of the contour it can; a non-chord tone whose resolution target moved moves by the same scale
 * steps, so it still resolves by step; one whose figure no longer fits is left, and is unclassified in the result.
 *
 * The frame relations work from the tone roles too. A figure they add or change is formed from the notes either side of
 * it, by the rules of motive_tone_roles: the pitch is the first of a fixed list of candidates (the scale steps next to
 * the notes either side, then those notes' own pitches, then two steps away) for which the tone roles of the result
 * label the note with that type; a type that no candidate forms there is NOT_APPLICABLE. A figure the rules cannot
 * tell from another (a suspension needs a chord change at its onset) is formed only where they can.
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
    MOTIVE_RELATION_REDUCE_TO_FRAME,
    MOTIVE_RELATION_ADD_NON_CHORD_TONE,
    MOTIVE_RELATION_REMOVE_NON_CHORD_TONE,
    MOTIVE_RELATION_CHANGE_NON_CHORD_TONE,
    MOTIVE_RELATION_REHARMONIZE_FRAME,
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
/*
 * True when applying step to in succeeds and changes at least one pitch (a harmony step whose chord already holds
 * the notes changes only the chords). False for a refused step. Used to choose harmony steps a reader can hear.
 */
bool motive_relation_harmony_changes_notes(const MotiveSketch *in, const MotiveRelationStep *step);
/* The fixed cost of a step (R-142): exact 0; transpose, augment, diminish 1; change-meter 3; every other 2. */
int motive_relation_cost(MotiveRelationKind kind);
/* "ok", "not applicable" and so on. */
const char *motive_relation_result_name(MotiveRelationResult result);

/*
 * The name of a kind's parameter at index (0-based, below motive_relation_param_count), as the header comment
 * above names it: "tonal", "by", "ratio". NULL when the kind or the index is out of range.
 */
const char *motive_relation_param_name(MotiveRelationKind kind, size_t index);

typedef enum {
    MOTIVE_RELATION_PARAM_INT,
    MOTIVE_RELATION_PARAM_BOOL,
    MOTIVE_RELATION_PARAM_RATIO,
    MOTIVE_RELATION_PARAM_CHOICE
} MotiveRelationParamKind;

/*
 * What a parameter is, for a caller that builds steps without a transform table of its own. min and max are the
 * values the relation accepts at all (an index is also limited by the sketch it is applied to; a ratio's min and
 * max bound its numerator and denominator). A choice has choice_count names in choices, the name of value v at
 * choices[v - min]. default_value (and default_den for a ratio) is the value of a parameter left out, as in
 * motive_relation_step_text. optional is true when that default is accepted as it stands (the relation applies with
 * it); a parameter that is not optional must be given. description is one lower-case phrase.
 */
typedef struct {
    const char *name;
    MotiveRelationParamKind kind;
    long min;
    long max;
    long default_value;
    long default_den;
    bool optional;
    const char *const *choices;
    size_t choice_count;
    const char *description;
} MotiveRelationParamInfo;

/* Fills *out for the parameter at index (0-based, below motive_relation_param_count). False, out untouched, otherwise. */
bool motive_relation_param_info(MotiveRelationKind kind, size_t index, MotiveRelationParamInfo *out);

/*
 * A step as text: the relation name, then name=value for each parameter, a ratio as n:d. An optional
 * parameter at its default (0, the invert axis -1) is left out, so "transpose tonal=1 by=-2", "augment
 * ratio=3:2", "exact". False when it does not fit in size (the buffer is still terminated, if size is above
 * 0) or the kind is out of range.
 */
bool motive_relation_step_text(const MotiveRelationStep *step, char *buffer, size_t size);

/*
 * The inverse of motive_relation_step_text. Parameters not given take their default. False, out untouched,
 * for an unknown relation or parameter, a parameter given twice, or a value out of form (a ratio needs n:d
 * above zero, any other parameter a whole number).
 */
bool motive_relation_step_parse(const char *text, MotiveRelationStep *out);

#endif
