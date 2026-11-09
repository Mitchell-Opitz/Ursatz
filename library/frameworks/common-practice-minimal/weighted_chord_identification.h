#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_WEIGHTED_CHORD_IDENTIFICATION_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_WEIGHTED_CHORD_IDENTIFICATION_H

/*
 * Owner of chord identification (common_practice_identify_weighted_sequence and
 * common_practice_identify_weighted) (docs/analytic-job-owners.md).
 */

#include <stdbool.h>
#include <stddef.h>

#include "interpretation/chord.h"
#include "non_chord_tones.h"
#include "weighted_identification.h"

/*
 * The weighted identification result and the functions that produce it, over a sequence of segments.
 * It builds on weighted_identification.h (weights and scoring) and non_chord_tones.h (leftover tones).
 *
 * For each segment the result holds the BEST chord and the RUNNER-UP, both scores, the UNCERTAINTY, and
 * the labeled leftover tones of the best chord. The runner-up is the best-scoring candidate on a DIFFERENT
 * ROOT from the best: a triad and the seventh chord built on it are one harmony, not rivals, and counting the
 * seventh of a bare triad as its runner-up would make every plain triad a dead tie. With no other root
 * sounding there is no runner-up (its chord is NULL, its score 0) and the uncertainty is 0.
 *
 *     uncertainty = runner_up_score / best_score, clamped to [0, 1]   (1 when best_score <= 0)
 *
 * so higher means LESS certain, and 1 is a dead tie. A best score below config.min_coverage (default
 * 0.5) is NO MATCH: no chords are built and there are no leftover tones, though the candidates and scores
 * are still reported.
 *
 * The Chords are the existing Chord/Interpretation shape (interpretation/chord.h), built by
 * common_practice_build_{triad,seventh}_chord_with_confidence, over the segment's Sonority id. A seventh
 * Chord carries the seventh QUALITY (chord_vocabulary_seventh.h has no root), so the chord's root is in the
 * ranked candidate beside it. The Interpretation's scalar confidence is the confidence in that chord, as a
 * rational in millionths: the best chord's is 1 - uncertainty, and the runner-up's is the uncertainty
 * (its plausibility relative to the best). The uncertainty itself is also kept exactly as a double here.
 *
 * Leftover tones can only be labeled with the neighbors and the next chord in view, so the labeling is a
 * second pass over the whole sequence: pass 1 scores and chooses every segment, pass 2 labels each matched
 * segment's leftover tones (non_chord_tones.h) using the chosen chords. The single-segment function
 * identifies one segment alone and leaves the tone list empty.
 */

/*
 * APPLIED DIMINISHED SEVENTHS (the sequence function only, config.applied_chords). A diminished seventh's pitch classes fit
 * two applied chords (vii-dim-7/V and vii-dim-7/iii share them), and the vocabulary keeps one. After pass 1 and before
 * CONTEXT, each matched segment whose best is an applied diminished seventh looks at the first later matched segment whose best
 * root differs from its own. When that root is the target of the other chord sharing the pitch classes (major: F# for V or D# for
 * iii, C# for ii or E for IV; minor: F# for V or A for VII, E for iv or G for VI), best takes the other chord's root, degree,
 * applied_target_degree and tonic_offset (the score is unchanged) and both Chords are rebuilt over that root. The runner-up stays.
 */

/*
 * CONTEXT (the sequence function only). After pass 1, a segment whose own reading is INCOMPLETE (no match, or
 * fewer than three of the best chord's pitch classes sounding) takes the chord of a neighbouring segment when
 * every pitch class it sounds is a tone of that chord: the previous segment's chord first (as resolved, so a run of
 * sparse segments carries one chord on), else the next segment's own complete reading (not with config.context_next false: the
 * chain turns it off on harmonic spans, R-379 (3)). The neighbour's best and
 * runner-up, their scores and the uncertainty are copied, and the Chords are built over this segment's id.
 * `context` records where the reading came from; COMMON_PRACTICE_CONTEXT_NONE is the segment's own.
 *
 * Before that, a run of two or more consecutive segments with the same non-zero `beat` (inside one beat), none of
 * them complete on its own, is read as ONE arpeggiated chord when the union of their members is complete: every
 * segment of the run takes the union's reading (COMMON_PRACTICE_CONTEXT_ARPEGGIO).
 */
typedef enum {
    COMMON_PRACTICE_CONTEXT_NONE = 0,
    COMMON_PRACTICE_CONTEXT_PREVIOUS,
    COMMON_PRACTICE_CONTEXT_NEXT,
    COMMON_PRACTICE_CONTEXT_ARPEGGIO
} CommonPracticeContextReading;

typedef struct {
    const char *note_id; /* the leftover note's id, borrowed from the caller's member */
    bool classified;
    NonChordTone tone;   /* meaningful only when classified */
} CommonPracticeLeftoverTone;

typedef struct {
    bool matched;
    CommonPracticeRankedChord best;      /* the top candidate (valid whenever the segment has members) */
    CommonPracticeRankedChord runner_up; /* the second */
    double best_score;
    double runner_up_score;
    double uncertainty;                  /* [0, 1]: higher is less certain */
    Chord *best_chord;                   /* owned; NULL unless matched */
    Chord *runner_up_chord;              /* owned; NULL unless matched */
    CommonPracticeLeftoverTone *leftover; /* owned; NULL when there are none */
    size_t leftover_count;
    CommonPracticeContextReading context; /* the evidence for a reading taken from context */
} CommonPracticeWeightedResult;

typedef struct {
    CommonPracticeWeightedResult *items; /* owned; NULL when count is 0 */
    size_t count;
} CommonPracticeWeightedResults;

/* One segment alone: no leftover tones are labeled. The segment's id names the Sonority the Chords are
   about. On OK, *out owns its Chords (common_practice_weighted_result_destroy). */
CommonPracticeWeightedStatus common_practice_identify_weighted(const CommonPracticeSegment *segment,
                                                                int tonic_pitch_class, CommonPracticeKeyMode mode,
                                                                const CommonPracticeWeightedConfig *config,
                                                                CommonPracticeWeightedResult *out);

/* The whole sequence, with leftover tones labeled. config may be NULL for the defaults. On OK, *out owns
   one result per segment (common_practice_weighted_results_destroy); otherwise *out is untouched. */
CommonPracticeWeightedStatus common_practice_identify_weighted_sequence(const CommonPracticeSegment *segments,
                                                                         size_t segment_count,
                                                                         int tonic_pitch_class,
                                                                         CommonPracticeKeyMode mode,
                                                                         const CommonPracticeWeightedConfig *config,
                                                                         CommonPracticeWeightedResults *out);

void common_practice_weighted_result_destroy(CommonPracticeWeightedResult *result);
void common_practice_weighted_results_destroy(CommonPracticeWeightedResults *results);

#endif
