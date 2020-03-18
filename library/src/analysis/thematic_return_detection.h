#ifndef MUSIC_ANALYSIS_THEMATIC_RETURN_DETECTION_H
#define MUSIC_ANALYSIS_THEMATIC_RETURN_DETECTION_H

#include <stddef.h>

#include "analysis/analyzer.h"
#include "analysis/motif_window_matching.h"
#include "identity/entity_id.h"
#include "relationship/relationship.h"

/*
 * ThematicReturnDetection (Procedure -> Analyzer implementation):
 * identifies thematic return -- a later section's Theme material
 * recognizably recurring from an earlier one (structure/theme.h,
 * structure/theme_occurrence.h) -- the thematic-return half of this
 * branch's scope (section order is analysis/form_structure_detection.h).
 *
 * No new Structure/Occurrence type: Theme/ThemeOccurrence already exist
 * as L5 shells (Registry "No, example only", same non-normative status
 * Motif carries); this module produces a generic Interpretation the
 * same "no new named type" pattern every other analysis module in this
 * pass uses.
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
 * Linking mechanism: for every ordered pair of entries (earlier, later)
 * with earlier appearing before later in input order whose windows
 * match, this module builds one Relationship (source = later
 * ThemeOccurrence id, predicate "resembles", target = earlier
 * ThemeOccurrence id) and one Interpretation targeting that
 * Relationship's id. "resembles" (TDD Sec77's documented predicate
 * vocabulary) is used rather than SentenceDetection's "transformed_from"
 * because thematic return is recurrence/recognition, not derivation --
 * "transformed_from" claims a transformation lineage (X was transformed
 * from Y); "resembles" claims similarity with no claimed direction of
 * derivation, the correct fit for "this later passage recalls that
 * earlier one" (requirement 1). Multiple earlier occurrences matching
 * the same later one (or vice versa) each produce their own
 * Relationship/Interpretation pair -- this module does not collapse or
 * pick a single "best" match.
 *
 * Where no two ThemeOccurrence windows match, this module produces no
 * output at all -- no detected thematic return is a legitimate outcome,
 * never forced (requirement 4).
 */

typedef struct {
    /* Borrowed; this ThemeOccurrence's own id (not its target Theme's id). */
    const EntityID *theme_occurrence_id;
    /* Borrowed; this ThemeOccurrence's region, as a Contour/RhythmicPattern/event window. */
    const MotifWindow *window;
} ThematicReturnDetectionEntry;

typedef struct {
    const ThematicReturnDetectionEntry *entries;
    size_t entry_count;
} ThematicReturnDetectionInput;

#define THEMATIC_RETURN_DETECTION_ID_BUFFER_SIZE 128

typedef struct {
    char later_theme_occurrence_id[THEMATIC_RETURN_DETECTION_ID_BUFFER_SIZE];
    char earlier_theme_occurrence_id[THEMATIC_RETURN_DETECTION_ID_BUFFER_SIZE];
} ThematicReturnDetectionClaim;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * ThematicReturnDetectionInput; context and config are accepted per the
 * interface but unused. Every entry's theme_occurrence_id and window
 * MUST be non-NULL (window's contour/rhythm/event_ids must be non-NULL
 * with a positive event_id_count). entry_count < 2 is valid and yields
 * an empty AnalysisResult (no pair to compare).
 *
 * For every ordered pair (i, j) with i < j whose windows match (equal
 * Contour directions AND equal RhythmicPattern durations, element for
 * element -- MotifWindowMatching's exact rule), produces one
 * Relationship (source = entries[j]'s id, predicate "resembles", target
 * = entries[i]'s id) and one Interpretation into the returned
 * AnalysisResult, in (i, j) enumeration order. A pair not matching
 * produces no output for that pair. AnalysisResult here holds only the
 * Interpretations (and their backing absent-provenance) -- the
 * Relationship objects themselves are NOT owned by AnalysisResult, the
 * same split PeriodDetection/SentenceDetection use; callers wanting the
 * Relationship objects use thematic_return_detection_build_relationships
 * below instead/in addition. Each Interpretation's target_id is
 * deterministically derived from the same entries[i]/entries[j] id pair
 * thematic_return_detection_build_relationships uses to mint its
 * Relationship's own id, so a caller running both against the same
 * input gets matching ids without this call constructing the
 * Relationship object itself.
 */
AnalyzerResult thematic_return_detection_analyze(const void *input, const Context *context, const Value *config,
                                                  AnalysisResult **out);

extern const Analyzer THEMATIC_RETURN_DETECTION_ANALYZER;

typedef enum {
    THEMATIC_RETURN_DETECTION_REL_OK = 0,
    THEMATIC_RETURN_DETECTION_REL_ERR_NULL_ENTRIES,
    THEMATIC_RETURN_DETECTION_REL_ERR_NULL_ENTRY,
    THEMATIC_RETURN_DETECTION_REL_ERR_ALLOC
} ThematicReturnDetectionRelationshipResult;

/*
 * Builds the Relationship objects backing thematic_return_detection_
 * analyze's Interpretations, using the identical pairwise matching rule.
 * entries/entry_count validation matches thematic_return_detection_
 * analyze. entry_count < 2 is valid and yields *out_relationships =
 * NULL, *out_relationship_count = 0.
 *
 * On success, *out_relationships is a freshly allocated array (caller
 * frees with thematic_return_detection_destroy_relationships) of one
 * Relationship per matching pair (predicate "resembles"), in (i, j)
 * enumeration order -- the same ids thematic_return_detection_analyze's
 * Interpretations reference as their target.
 */
ThematicReturnDetectionRelationshipResult thematic_return_detection_build_relationships(
    const ThematicReturnDetectionEntry *entries, size_t entry_count, Relationship ***out_relationships,
    size_t *out_relationship_count);

/* Destroys each Relationship in relationships and the array itself. */
void thematic_return_detection_destroy_relationships(Relationship **relationships, size_t relationship_count);

#endif
