#ifndef MUSIC_ANALYSIS_MOTIVE_VARIANT_RECOGNITION_H
#define MUSIC_ANALYSIS_MOTIVE_VARIANT_RECOGNITION_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/motive_relation.h"
#include "analysis/motive_sketch.h"

/*
 * Variant recognizer (L8 analysis): names the chain of motive relations (analysis/motive_relation.h) that turns
 * one motive sketch into another, with its distance and which dimensions the variant kept and changed. The
 * inputs are plain sketches, notes the analysis chain already produces (R-006); nothing is allocated.
 */

typedef enum {
    MOTIVE_DIMENSION_RHYTHM,
    MOTIVE_DIMENSION_INTERVALS,
    MOTIVE_DIMENSION_CONTOUR,
    MOTIVE_DIMENSION_HARMONY,
    MOTIVE_DIMENSION_COUNT
} MotiveDimension;

/*
 * Two sketches compared per dimension (R-134). similarity is 0 to 1, kept is true when it is 1, changed when not.
 *   rhythm     the share of neighbouring note pairs whose inter-onset ratio and duration ratio match (over the
 *              longer sketch); kept when onsets and durations scale by one common factor
 *   intervals  the share of steps that match: scale steps by each sketch's own scale, semitones without one
 *   contour    the share of directions that match
 *   harmony    the share of chords, by position, with the same degree, alteration, inversion and seventh (onsets
 *              are not compared)
 * A dimension neither sketch has (pitches in a rhythm sketch, chords) is 1; one sketch having it and not the
 * other is 0.
 */
typedef struct {
    double similarity[MOTIVE_DIMENSION_COUNT];
    bool kept[MOTIVE_DIMENSION_COUNT];
    bool changed[MOTIVE_DIMENSION_COUNT];
} MotiveDimensions;

void motive_variant_compare(const MotiveSketch *a, const MotiveSketch *b, MotiveDimensions *out);

/* The mean of the four similarities: how near one sketch is to another. */
double motive_variant_nearness(const MotiveDimensions *dimensions);

#endif
