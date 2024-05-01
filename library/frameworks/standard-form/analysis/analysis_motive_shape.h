#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_MOTIVE_SHAPE_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_MOTIVE_SHAPE_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_input.h"
#include "analysis_time.h"

/*
 * Internal to the motive analysis: a line of notes prepared for windowing, and the SHAPE of a window (its interval sequences and its
 * rhythm), which the matchers (analysis_motive_match.h) compare. See analysis_motive.h for the dimensions.
 */

#define STANDARD_FORM_MOTIVE_MAX_INTERVALS 31

typedef struct {
    bool generic;                 /* `steps` are generic intervals; else they are the semitones */
    size_t intervals;             /* notes - 1 */
    int steps[STANDARD_FORM_MOTIVE_MAX_INTERVALS];     /* the pattern's intervals, signed */
    int semitones[STANDARD_FORM_MOTIVE_MAX_INTERVALS]; /* always the semitone intervals */
    StandardFormQ ioi[STANDARD_FORM_MOTIVE_MAX_INTERVALS]; /* onset to onset */
    long first_semitone;
} StandardFormMotiveShape;

typedef struct {
    const StandardFormNote *notes; /* borrowed */
    size_t count;
    int line;                      /* 0 soprano, 1 bass */
    StandardFormQ *start;          /* owned arrays, one entry per note */
    StandardFormQ *end;
    int *first_measure;
    bool *can_start;               /* the first note of an idea, or on a beat */
    bool *has_generic;             /* the generic interval to the next note is known */
    int *generic;
} StandardFormMotiveLine;

/* Prepares a line. idea_start, when not NULL, is a count-long array marking the notes that start an idea. False on allocation
   failure or a time the MeterMap cannot place. */
bool standard_form_motive_line_build(StandardFormMotiveLine *line, const StandardFormInput *input, int line_id,
                                     const bool *idea_start);
void standard_form_motive_line_destroy(StandardFormMotiveLine *line);

/* True when the n notes from `first` form a window: they exist, no rest lies inside, and every onset gap is positive. */
bool standard_form_motive_window_ok(const StandardFormMotiveLine *line, size_t first, size_t n);

/* The shape of the window. With want_generic the generic intervals are the steps and false is returned when one is unknown;
   without, the semitones are. */
bool standard_form_motive_shape(const StandardFormMotiveLine *line, size_t first, size_t n, bool want_generic,
                                StandardFormMotiveShape *out);

#endif
