#ifndef MUSIC_ANALYSIS_SENTENCE_DETECTION_H
#define MUSIC_ANALYSIS_SENTENCE_DETECTION_H

#include <stddef.h>

#include "analysis/analyzer.h"
#include "analysis/motif_window_matching.h"
#include "identity/entity_id.h"
#include "relationship/relationship.h"

/*
 * SentenceDetection (Procedure -> Analyzer implementation): identifies
 * period/sentence theory's Sentence structure -- a statement, its
 * immediate repetition or transposition, and continuation material
 * leading toward the phrase's cadence -- from MotifDetection's already-
 * detected statement/repetition pair plus caller-supplied continuation
 * candidate material.
 *
 * No new Structure/Occurrence type: the System Registry has no named
 * type for Sentence, same as Period (analysis/period_detection.h). This
 * module produces generic Interpretations whose claims describe the
 * statement/repetition/continuation relationship, the same "no new
 * named type" pattern PeriodDetection/CadenceDetection/PhraseSegmentation
 * already use.
 *
 * Linking mechanism (extends PeriodDetection's exact pattern by one
 * link, per this branch's requirement 1): Sentence is a THREE-part
 * structure, but Interpretation's target is still a single EntityID
 * reference. This module builds TWO chained Relationships instead of
 * PeriodDetection's one:
 *   repetition --"transformed_from"--> statement
 *   continuation --"transformed_from"--> repetition
 * "transformed_from" is in Relationship's documented predicate
 * vocabulary already (relationship/relationship.h) -- reused as-is,
 * not invented, per this branch's requirement 1 guidance. Unlike
 * PeriodDetection's "precedes" (which points chronologically earlier ->
 * later), "transformed_from" points derived -> source: each part
 * points at the part it was transformed from, not at its chronological
 * predecessor -- the predicate's own semantics decide the direction,
 * not a copy of Period's chronological convention.
 *
 * Each Relationship gets its own Interpretation (target = that
 * Relationship's id), both carrying a shared SentenceDetectionClaim
 * recording all three occurrence ids -- the same one-Interpretation-
 * per-Relationship shape PeriodDetection uses, just repeated once more
 * to cover the extra link.
 *
 * statement/repetition occurrence ids are supplied by the caller
 * (already minted by MotifDetection/MotifWindowMatching for the
 * statement and its repeated/transposed match -- this module does not
 * re-detect that pair, only consumes it). continuation_candidate is the
 * caller-sliced remaining phrase material after the repetition, up to
 * (not including) the phrase's cadence -- SentenceDetection has no
 * opinion on where a phrase's cadence falls or how to slice around it,
 * the same no-opinion-on-boundary-placement stance MotifWindowMatching
 * takes toward window cutting; NULL means no candidate continuation
 * material was found for this pair, a legitimate input (requirement 4).
 * Continuation qualification itself is delegated to
 * SentenceContinuationMatching (analysis/sentence_continuation_matching.h).
 *
 * Where no candidate continuation is supplied, or the supplied
 * candidate does not qualify, this entry produces NO output -- a
 * phrase with no detected Sentence structure is a legitimate, common
 * outcome (it may be a Period instead, or neither), never forced.
 */

typedef struct {
    /* Borrowed; the MotifOccurrence id MotifDetection minted for the statement. */
    const EntityID *statement_occurrence_id;
    /* Borrowed; the MotifOccurrence id MotifDetection minted for the repetition/transposition. */
    const EntityID *repetition_occurrence_id;
    /* Borrowed; the statement's own Contour/RhythmicPattern/event window, for continuation matching. */
    const MotifWindow *statement_window;
    /* Borrowed; NULL means no candidate continuation material for this pair. */
    const MotifWindow *continuation_candidate;
} SentenceDetectionEntry;

typedef struct {
    const SentenceDetectionEntry *entries;
    size_t entry_count;
} SentenceDetectionInput;

#define SENTENCE_DETECTION_ID_BUFFER_SIZE 128

typedef struct {
    char statement_occurrence_id[SENTENCE_DETECTION_ID_BUFFER_SIZE];
    char repetition_occurrence_id[SENTENCE_DETECTION_ID_BUFFER_SIZE];
    char continuation_occurrence_id[SENTENCE_DETECTION_ID_BUFFER_SIZE];
} SentenceDetectionClaim;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * SentenceDetectionInput; context and config are accepted per the
 * interface but unused. Every entry's statement_occurrence_id,
 * repetition_occurrence_id, and statement_window MUST be non-NULL
 * (statement_window's contour/rhythm/event_ids must be non-NULL with a
 * positive event_id_count); continuation_candidate MAY be NULL.
 * entry_count == 0 is valid and yields an empty AnalysisResult.
 *
 * For each entry: if continuation_candidate is non-NULL and
 * sentence_continuation_matching_is_continuation(statement_window,
 * continuation_candidate) is true, produces TWO Relationships and TWO
 * Interpretations (per the chained-predicate design above) into the
 * returned AnalysisResult, in input order. An entry not meeting that
 * rule produces no output for that entry (skipped, not a low-confidence
 * guess -- requirement 4). AnalysisResult here holds only the
 * Interpretations (and their backing absent-provenance) -- the
 * Relationship objects themselves are NOT owned by AnalysisResult, the
 * same split PeriodDetection uses; callers wanting the Relationship
 * objects use sentence_detection_build_relationships below instead/in
 * addition. Each pair of Interpretations' target_ids are deterministically
 * derived from the same occurrence-id inputs
 * sentence_detection_build_relationships uses to mint its Relationships'
 * own ids, so a caller running both against the same input gets matching
 * ids without either call constructing the other's objects.
 */
AnalyzerResult sentence_detection_analyze(const void *input, const Context *context, const Value *config,
                                           AnalysisResult **out);

extern const Analyzer SENTENCE_DETECTION_ANALYZER;

typedef enum {
    SENTENCE_DETECTION_REL_OK = 0,
    SENTENCE_DETECTION_REL_ERR_NULL_ENTRIES,
    SENTENCE_DETECTION_REL_ERR_NULL_ENTRY,
    SENTENCE_DETECTION_REL_ERR_ALLOC
} SentenceDetectionRelationshipResult;

/*
 * Builds the Relationship objects backing sentence_detection_analyze's
 * Interpretations, using the identical qualification rule.
 * entries/entry_count validation matches sentence_detection_analyze.
 * entry_count == 0 is valid and yields *out_relationships = NULL,
 * *out_relationship_count = 0.
 *
 * On success, *out_relationships is a freshly allocated array (caller
 * frees with sentence_detection_destroy_relationships) of Relationship
 * pointers, TWO per qualifying entry ("repetition transformed_from
 * statement" immediately followed by "continuation transformed_from
 * repetition"), in input order -- the same ids
 * sentence_detection_analyze's Interpretations reference as their
 * target.
 */
SentenceDetectionRelationshipResult sentence_detection_build_relationships(const SentenceDetectionEntry *entries,
                                                                            size_t entry_count,
                                                                            Relationship ***out_relationships,
                                                                            size_t *out_relationship_count);

/* Destroys each Relationship in relationships and the array itself. */
void sentence_detection_destroy_relationships(Relationship **relationships, size_t relationship_count);

#endif
