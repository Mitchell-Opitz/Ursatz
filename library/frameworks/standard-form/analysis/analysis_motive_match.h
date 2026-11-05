#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_MOTIVE_MATCH_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_MOTIVE_MATCH_H

/*
 * Not the motive matching owner: motive_variant_recognize with the motive_relation_apply relations is, and the chain injects it
 * (StandardFormConfig.relation_match). Used alone, the framework matches the pattern, inversion and retrograde kinds and VARIED; it
 * does not match augmentation, diminution or fragments (docs/analytic-job-owners.md).
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
 *   VARIED        for a motive of at least 5 notes: at most `varied_max_mismatch` intervals differ (intervallic manipulation), or none
 *                 differs and the rhythm does. The rhythm may differ, which costs a factor of 0.8; no other kind may
 *
 * The score is (1 - mismatches / intervals) times the rhythm factor (1, or 0.8 for a pitch-only match of a varied motive): 1 for every
 * kind but a varied one. Retrograde inversion is not defined by standard-form's motive rules and is not matched. Displacement and
 * embellishment are not detected.
 *
 * AUGMENTATION (onset gaps times 2), DIMINUTION (times 1/2) and FRAGMENTS (a prefix or suffix of at least 2 intervals and half the
 * motive's) are matched only by the injected matcher (the kernel relations), through standard_form_motive_classify_match and
 * standard_form_motive_fragment_match below.
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
bool standard_form_motive_match_varied(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                       size_t max_mismatch, StandardFormMotiveMatch *out);

/* The fewest intervals of a fragment of a motive of `intervals` intervals: at least 2, and at least half the motive. */
size_t standard_form_motive_fragment_min_intervals(size_t intervals);
/* The kind that applies to the candidate by the framework's own matchers (pattern, inversion, retrograde, varied), with its match. */
bool standard_form_motive_classify(const StandardFormMotiveShape *prototype, const StandardFormMotiveShape *candidate,
                                   size_t varied_max_mismatch, StandardFormMotiveMatch *out);
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
