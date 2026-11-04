#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_MOTIVE_MATCH_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_MOTIVE_MATCH_H

/*
 * Not the motive matching owner (motive_variant_recognize with the motive_relation_apply relations).
 * The transposed, inverted and retrograde matchers switch to the kernel relations in P23
 * (docs/ARCHITECTURE.md, Analytic job owners).
 */

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
 * FRAGMENTS (step 3): a window shorter than the motive that equals its first or last intervals and onset gaps, at least 2 intervals and at
 * least half the motive's, is a FRAGMENTED occurrence (prefix or suffix).
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
    int fragment_part;          /* a fragment: 0 prefix, 1 suffix; else -1 */
    size_t fragment_intervals;  /* a fragment: its intervals */
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

/* The fewest intervals of a fragment of a motive of `intervals` intervals: at least 2, and at least half the motive. */
size_t standard_form_motive_fragment_min_intervals(size_t intervals);
/* The candidate (a shorter window, of the same dimension) is a contiguous PREFIX or SUFFIX of the prototype: its intervals and onset gaps
   equal the prototype's first or last ones, in order, and it has at least standard_form_motive_fragment_min_intervals of them and fewer
   than the prototype. The prefix is tried first. Kind FRAGMENTED, with the part and its length; score 1. A fragment is of the pattern
   itself (no transformation of it is matched). */
bool standard_form_motive_match_fragment(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                         StandardFormMotiveMatch *out);

/* The kind that applies to the candidate by the precedence rule, with its match. False when none applies. */
bool standard_form_motive_classify(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                   size_t varied_max_mismatch, StandardFormMotiveMatch *out);
/* The same, with an injected matcher (StandardFormConfig.relation_matcher) in the place of the pattern, inversion and retrograde
   matchers: its kind and score are used for a comparable pair (same dimension, same interval count); when it refuses, the pair goes on
   to augmentation, diminution and varied. matcher NULL is standard_form_motive_classify. */
bool standard_form_motive_classify_with(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                        size_t varied_max_mismatch, StandardFormRelationMatcher matcher, void *context,
                                        StandardFormMotiveMatch *out);

/* The same with the injected StandardFormConfig.relation_match: for a pair of the same dimension and interval count its kind (not
   FRAGMENTED, not VARIED) and score are used; when it refuses, the pair goes on to varied alone. */
bool standard_form_motive_classify_match(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                         size_t varied_max_mismatch, StandardFormRelationMatchFn match, void *context,
                                         StandardFormMotiveMatch *out);
/* The candidate (shorter than the prototype, at least standard_form_motive_fragment_min_intervals) through the injected matcher: true
   only for a FRAGMENTED result. */
bool standard_form_motive_fragment_match(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                         StandardFormRelationMatchFn match, void *context, StandardFormMotiveMatch *out);

#endif
