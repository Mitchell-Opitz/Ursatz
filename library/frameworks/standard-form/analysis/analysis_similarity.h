#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_SIMILARITY_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_SIMILARITY_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_input.h"

/*
 * Idea similarity (the form adapter): whether two ideas are a repetition (a varied one, transposed or not), a return, or a
 * contrast. Stated default, configurable (similarity_min, 0.75); higher-level motive matching waits for L3.7.
 *
 * Two ideas are lists of soprano notes. At each position i the INTERVAL from note i to note i + 1 (in semitones, signed, so
 * a transposition keeps every interval) and the ONSET RHYTHM, the time from the onset of note i to that of note i + 1 (exact), are
 * compared; the position MATCHES when both are equal. The positions are those of the longer idea (its intervals); a position the
 * shorter idea does not reach does not match. So
 *
 *   similarity = matched positions / the longer idea's intervals
 *   match      similarity >= similarity_min
 *   margin     how far the similarity is from the threshold, in [0, 1]: (similarity - min) / (1 - min) for a match (1 when
 *              min is 1 and it matches), (min - similarity) / min for a non-match. The more certain the call, the nearer 1
 *
 * Two ideas of fewer than two notes have no intervals and are not comparable (similarity 0, no match, margin 0).
 */

typedef struct {
    bool comparable;
    size_t positions;
    size_t matched;
    double similarity;
    bool match;
    double margin;
} StandardFormSimilarity;

/* False for a NULL argument, a similarity_min outside (0, 1], or notes with no span. */
bool standard_form_idea_similarity(const StandardFormNote *a, size_t a_count, const StandardFormNote *b, size_t b_count,
                                   double similarity_min, StandardFormSimilarity *out);

/* The uncertainty the similarity adds to a claim: 0.5 * (1 - margin). 0.5 when the ideas are not comparable. */
double standard_form_similarity_uncertainty(const StandardFormSimilarity *similarity);

#endif
