#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_MOTIVE_MATCH_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_MOTIVE_MATCH_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_motive.h"
#include "analysis_motive_shape.h"

/*
 * The transformation matchers of the motive analysis: how a window of notes (the CANDIDATE) is the recurrence of a family's PROTOTYPE.
 * Each takes two shapes (analysis_motive_shape.h) of the same dimension (both generic or both semitone) and says whether the
 * candidate is the prototype under one transformation, with the match's score:
 *
 *   PATTERN       the same interval sequence and the same rhythm: EXACT (the same pitches), REAL (the same semitone intervals from another
 *                 pitch) or TONAL (the same generic intervals, other semitones). The kind of repetition and transposition
 *   INVERSION     every interval negated, the rhythm the same (motive_rules.h: inversion)
 *   RETROGRADE    the notes in reverse order: the intervals reversed AND negated (the first-to-last pitch order backwards), the onset gaps
 *                 reversed (motive_rules.h: retrograde)
 *   AUGMENTATION  the same intervals with every onset gap multiplied by 2 (enlargement); DIMINUTION by 1/2 (contraction). These two
 *                 factors only; standard-form names no others
 *   VARIED        for a motive of at least 5 notes: at most `varied_max_mismatch` intervals differ (intervallic manipulation), or none
 *                 differs and the rhythm does. The rhythm may differ, which costs a factor of 0.8; no other kind may
 *
 * The score is (1 - mismatches / intervals) times the rhythm factor (1, or 0.8 for a pitch-only match of a varied motive): 1 for every
 * kind but a varied one. Retrograde inversion is not defined by standard-form's motive rules and is not matched. Displacement and
 * embellishment are not detected.
 *
 * PRECEDENCE when several kinds apply to one window: exact > transposed (real, tonal) > inverted and retrograde > augmented and
 * diminished > fragmented > varied (standard_form_motive_kind_rank); within a rank the order of the list above.
 */

typedef struct {
    int kind;             /* StandardFormMotiveKind */
    int mismatches;
    size_t intervals;
    double rhythm_factor;
    double score;
} StandardFormMotiveMatch;

bool standard_form_motive_match_pattern(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                        StandardFormMotiveMatch *out);
bool standard_form_motive_match_inversion(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                          StandardFormMotiveMatch *out);
bool standard_form_motive_match_retrograde(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                           StandardFormMotiveMatch *out);
/* The candidate's onset gaps are the prototype's times `numerator / denominator`, with equal intervals. AUGMENTED when the factor is
   above 1, DIMINISHED when below; false for a factor of 1 or less than 0. */
bool standard_form_motive_match_scaled(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                       long long numerator, long long denominator, StandardFormMotiveMatch *out);
bool standard_form_motive_match_augmentation(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                             StandardFormMotiveMatch *out);
bool standard_form_motive_match_diminution(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                           StandardFormMotiveMatch *out);
bool standard_form_motive_match_varied(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                       size_t max_mismatch, StandardFormMotiveMatch *out);

/* The kind that applies to the candidate by the precedence rule, with its match. False when none applies. */
bool standard_form_motive_classify(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                   size_t varied_max_mismatch, StandardFormMotiveMatch *out);

#endif
