#ifndef MUSIC_ANALYSIS_THEMATIC_RETURN_DETECTION_H
#define MUSIC_ANALYSIS_THEMATIC_RETURN_DETECTION_H

#include <stddef.h>

#include "analysis/motive/motif_window_matching.h"
#include "model/identity/entity_id.h"

/*
 * ThematicReturnDetection (Procedure):
 * identifies thematic return -- a later section's Theme material
 * recognizably recurring from an earlier one -- the thematic-return half of this
 * branch's scope (section order is analysis/form_structure_detection.h).
 *
 * No new Structure/Occurrence type: a theme occurrence is an id and
 * its region of event ids that the caller holds; this module produces
 * ThematicReturnPair values only (the index of the earlier and the later entry and the evidence of the match).
 *
 * Matching mechanism (requirement 3): reuses MotifWindowMatching's exact
 * Contour+RhythmicPattern comparison rule DIRECTLY, at Theme granularity
 * rather than inventing new matching logic. MotifWindowMatching's own
 * window-comparison rule is granularity-agnostic -- it compares whatever
 * Contour/RhythmicPattern the caller built for a given window, with no
 * assumption about how long that window is. A ThemeOccurrence's region
 * (region_event_ids, potentially spanning several phrases) is exactly
 * such a window: the caller builds one MotifWindow per ThemeOccurrence
 * from its region's already-observed Contour/RhythmicPattern, the
 * identical obligation every other MotifWindow caller already has. This
 * module reimplements MotifWindowMatching's windows_match comparison
 * (contour_direction equality + duration equality, both required) rather
 * than calling motif_window_matching_find itself, because that function
 * only reports PER-WINDOW "did this match something" flags -- thematic
 * return needs the actual matching PAIR (to know which later
 * ThemeOccurrence resembles which earlier one), which
 * motif_window_matching_find's return shape cannot express. The
 * comparison RULE is reused unchanged; only the pairwise bookkeeping
 * differs. This does not modify or duplicate MotifWindowMatching's own
 * public surface (out of scope per this branch).
 *
 * Pairing mechanism: for every ordered pair of entries (earlier, later) with earlier appearing before later in input order whose
 * windows match, this module gives one ThematicReturnPair. A pair says the later passage recalls the earlier one (similarity with no
 * claimed direction of derivation, not a transformation). Multiple earlier occurrences matching the same later one (or vice versa)
 * each give their own pair -- this module does not collapse or pick a single "best" match.
 *
 * Where no two ThemeOccurrence windows match, this module produces no
 * output at all -- no detected thematic return is a legitimate outcome,
 * never forced (requirement 4).
 *
 * Minimum window length (framework-v1-reference-set triage, Bach BWV
 * Anh. 114): reusing MotifWindowMatching's exact comparison rule
 * unchanged at Theme granularity, with no minimum-length floor, was
 * confirmed to produce near-total pairwise "resembles" matches --
 * short/generic ThemeOccurrence windows (e.g. a 3-event stepwise run)
 * exact-match across unrelated occurrences even though they are not a
 * genuine recurring theme. This module now excludes any window below
 * THEMATIC_RETURN_DETECTION_MIN_WINDOW_LENGTH (4 events) from matching
 * entirely, local to this module -- MotifWindowMatching's own behavior
 * at Motif granularity is unchanged (out of this branch's scope). 4 is
 * a stated default, not Registry-settled, and is expected to move once
 * more of the corpus is checked.
 */

typedef struct {
    /* Borrowed; this ThemeOccurrence's own id (not its target Theme's id). */
    const EntityID *theme_occurrence_id;
    /* Borrowed; this ThemeOccurrence's region, as a Contour/RhythmicPattern/event window. */
    const MotifWindow *window;
} ThematicReturnDetectionEntry;

/*
 * Return matching settings. window_length is the number of events compared at a time inside each theme (the
 * shorter theme's length when that is less; at least THEMATIC_RETURN_DETECTION_MIN_WINDOW_LENGTH, a smaller
 * value is replaced by the default). threshold is the similarity (0 to 1) a stretch pair must reach to count as
 * a return, a value outside (0, 1] is replaced by the default. Similarity is the lower of the contour score
 * (share of equal up/down/same directions) and the rhythm score (share of equal durations) of the best stretch
 * pair, so transposed statements match and a changed rhythm or shape lowers the score.
 */
typedef struct {
    size_t window_length;
    double threshold;
} ThematicReturnDetectionConfig;

/* Defaults: window_length 6, threshold THEMATIC_RETURN_DETECTION_DEFAULT_THRESHOLD. */
ThematicReturnDetectionConfig thematic_return_detection_default_config(void);

#define THEMATIC_RETURN_DETECTION_MIN_WINDOW_LENGTH 4
#define THEMATIC_RETURN_DETECTION_DEFAULT_WINDOW_LENGTH 6
#define THEMATIC_RETURN_DETECTION_DEFAULT_THRESHOLD 0.8

/*
 * One return: the entries it links (indices into the entries the caller gave, earlier before later), the scores of the best stretch
 * pair (see ThematicReturnDetectionConfig) and where the stretches sit, as event offsets inside the earlier and the later theme's
 * window. similarity = lower of contour_score and rhythm_score.
 */
typedef struct {
    size_t earlier;
    size_t later;
    double contour_score;
    double rhythm_score;
    double similarity;
    size_t earlier_offset;
    size_t later_offset;
    size_t window_length;
} ThematicReturnPair;

typedef enum {
    THEMATIC_RETURN_DETECTION_OK = 0,
    THEMATIC_RETURN_DETECTION_ERR_NULL_ENTRIES,
    THEMATIC_RETURN_DETECTION_ERR_NULL_ENTRY,
    THEMATIC_RETURN_DETECTION_ERR_ALLOC
} ThematicReturnDetectionResult;

/*
 * Gives one ThematicReturnPair for every ordered pair (i, j) with i < j whose windows match under the match settings of config
 * (NULL means the defaults), in (i, j) enumeration order. Every entry's theme_occurrence_id and window MUST be non-NULL.
 * entry_count < 2, or no match, is valid and yields *out_pairs = NULL, *out_pair_count = 0. On success *out_pairs is a freshly
 * allocated array the caller frees with free().
 */
ThematicReturnDetectionResult thematic_return_detection_build_pairs_configured(const ThematicReturnDetectionEntry *entries,
                                                                                size_t entry_count,
                                                                                const ThematicReturnDetectionConfig *config,
                                                                                ThematicReturnPair **out_pairs,
                                                                                size_t *out_pair_count);

#endif
