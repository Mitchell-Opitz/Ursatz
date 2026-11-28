#ifndef URSATZ_ANALYSIS_DIMENSION_DIMENSION_H
#define URSATZ_ANALYSIS_DIMENSION_DIMENSION_H

#include <stdbool.h>
#include <stddef.h>

#include "model/core/rational.h"
#include "model/pitch/pitch_material.h"
#include "theory/vocabulary/contrast_aspect.h"

/*
 * The dimensions of music a span can be measured in, and how far two spans contrast (R-709, target architecture 5.3). A
 * dimension is a name and a measure over a span of notes; the table in dimension.c is the one place that lists them, so a new one
 * (harmonic tension, dynamics, intensity: later phases) is a row and a function. It has register (the mean MIDI pitch), density (notes
 * per beat), brightness (R-730: from 1, locrian, to 7, lydian, read from the notes against a center, see measure_brightness) and tension
 * (R-764: Hindemith's chord group of each sounding chord, ranked 1 to 12, weighted by length, see measure_tension). Every value is an exact Rational.
 *
 * A DimensionSpan is the notes of a span in onset order, each with its onset from the span's start, its duration (whole notes) and
 * its MIDI pitch, the span's length in whole notes and the length of its beat. An empty span, a length or beat that is not positive,
 * or a note with a duration that is not positive is invalid input.
 *
 * Two spans contrast in each ContrastAspect by a distance of 0 (alike) to 1 or more:
 *   rhythm    1 minus the similarity of the duration ratios of the two spans (analysis/similarity), so a uniform augmentation or
 *             diminution does not contrast;
 *   contour   1 minus the similarity of the directions between successive pitches (up, down, same); two spans of one note have
 *             distance 0, a span of one note against a longer one 1;
 *   register  the absolute difference of the mean pitches, in semitones;
 *   density   the absolute difference of the notes per beat.
 */

typedef enum { DIMENSION_REGISTER = 0, DIMENSION_DENSITY, DIMENSION_BRIGHTNESS, DIMENSION_TENSION, DIMENSION_COUNT } Dimension;

typedef struct {
    Rational onset;    /* from the start of the span */
    Rational duration; /* in whole notes */
    int pitch;         /* MIDI */
} DimensionNote;

typedef struct {
    const DimensionNote *notes;
    size_t count;
    Rational length; /* of the span, in whole notes */
    Rational beat;   /* the length of one beat, in whole notes (1/4 in 4/4, 3/8 in 6/8) */
    bool has_center;                /* brightness only: the pitch class (0 to 11) the span is measured against */
    int center;
    const PitchMaterial *material;  /* brightness only: the span's material, which answers for a degree that does not sound; NULL for none */
} DimensionSpan;

typedef enum {
    DIMENSION_OK = 0,
    DIMENSION_ERR_NULL_ARGUMENT,
    DIMENSION_ERR_INVALID_INPUT, /* an empty span, a length, beat or duration that is not positive, an unknown dimension */
    DIMENSION_ERR_OVERFLOW,
    DIMENSION_ERR_ALLOC
} DimensionResult;

/* The distance of two spans in each aspect. */
typedef struct {
    Rational distance[CONTRAST_ASPECT_COUNT];
} ContrastVector;

/* The name of a dimension ("register", "density"); NULL for an invalid value. */
const char *dimension_name(Dimension dimension);

/* The measure of the span in the dimension. *out is untouched on any result other than DIMENSION_OK. */
DimensionResult dimension_measure(Dimension dimension, const DimensionSpan *span, Rational *out);

/* How far the two spans contrast in each aspect. *out is untouched on any result other than DIMENSION_OK. */
DimensionResult dimension_contrast(const DimensionSpan *a, const DimensionSpan *b, ContrastVector *out);

#endif
