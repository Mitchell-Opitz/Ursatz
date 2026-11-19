#ifndef MUSIC_ANALYSIS_MOTIVE_RELATION_H
#define MUSIC_ANALYSIS_MOTIVE_RELATION_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/motive/motive_sketch.h"
#include "theory/vocabulary/motive_relation_step.h"

/*
 * MotiveRelation (L8 analysis): the catalog of motive relations, one per operation of a composer's variant
 * registry, each with an apply over a MotiveSketch. A relation turns a motive into a variant of it (a
 * transposition, an inversion, a rhythm made shorter, a harmony changed with the melody adapted to it). The
 * variant recognizer searches chains of these steps; a composer's registry maps its rows to them one to one
 * (the kind's name is the row's name, the parameters are in the row's order).
 *
 * A step (the kind, its parameters, its name and its text form) is theory/vocabulary/motive_relation_step.h.
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

/*
 * True when applying step to in succeeds and changes at least one pitch (a harmony step whose chord already holds
 * the notes changes only the chords). False for a refused step. Used to choose harmony steps a reader can hear.
 */
bool motive_relation_harmony_changes_notes(const MotiveSketch *in, const MotiveRelationStep *step);
/* The fixed cost of a step (R-142): exact 0; transpose, augment, diminish 1; change-meter 3; every other 2. */
int motive_relation_cost(MotiveRelationKind kind);

#endif
