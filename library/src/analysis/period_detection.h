#ifndef MUSIC_ANALYSIS_PERIOD_DETECTION_H
#define MUSIC_ANALYSIS_PERIOD_DETECTION_H

#include <stddef.h>

#include "analysis/analyzer.h"
#include "relationship/relationship.h"
#include "structure/phrase_occurrence.h"

/*
 * PeriodDetection (Procedure -> Analyzer implementation): identifies
 * antecedent/consequent phrase pairs (period/sentence theory's Period
 * structure, TDD's Interpretation example) from consecutive
 * PhraseOccurrences' triggering cadence strength.
 *
 * No new Structure/Occurrence type: the System Registry has no named
 * type anywhere for Period -- the TDD's only reference frames it as a
 * plain Interpretation example, not a Structure/Occurrence. This module
 * produces a generic Interpretation whose claim describes the
 * antecedent/consequent relationship, over the relevant
 * PhraseOccurrences, the same "no new named type" pattern
 * CadenceDetection/PhraseSegmentation use for their own generic
 * Interpretations.
 *
 * Linking mechanism: a period pair is TWO PhraseOccurrences, and
 * Interpretation's target is a single EntityID reference -- it cannot
 * hold two targets itself. Relationship (relationship/relationship.h,
 * L6, music-relationship) is the TDD-documented device for exactly
 * this: it exists in code (despite its Registry "Not Started" status --
 * the same gap between Registry status and actual code every prior
 * branch this session has found), and "precedes" is in its documented
 * predicate vocabulary (TDD Sec77). This module builds one Relationship
 * per identified pair (source_id = antecedent PhraseOccurrence id,
 * predicate_name = "precedes", target_id = consequent PhraseOccurrence
 * id), then one Interpretation targeting that Relationship's id (a
 * single EntityID, like every other Interpretation in this codebase)
 * with a PeriodDetectionClaim carrying both phrase ids and cadence
 * types for convenience. This is the first analysis-layer use of
 * Relationship; music-analysis already depends on all four L6 modules
 * (docs/modules/analysis.md), so no new dependency is introduced.
 *
 * cadence_type is a plain copy of PhraseBoundaryDetection's signal
 * Interpretation claim label (analysis/phrase_boundary_detection.h) --
 * the caller (who already ran PhraseBoundaryDetection) associates each
 * PhraseOccurrence with the cadence_type that closed it. "unclassified"
 * is a legitimate value (not re-parsed or guessed at), and an absent
 * cadence_type (empty string or NULL) is likewise valid input, not an
 * error -- both mean "no reliable cadence strength for this phrase",
 * and a pair touching either is skipped entirely, never guessed at
 * (the same unknown-vs-low-confidence discipline
 * PhraseBoundaryDetection/CadenceClassification already use).
 *
 * Cadence-strength ordering (flagged design default, NOT Registry-
 * settled -- a reasonable common-practice ordering, the same
 * flagged-default treatment PhraseBoundaryDetection's
 * max_length_without_cadence and MotifWindowMatching's two-occurrences
 * minimum use), weakest to strongest:
 *   "half cadence (HC)" < "deceptive" < "plagal" <
 *   "authentic (unclassified: PAC/IAC)" < "authentic (IAC)" <
 *   "authentic (PAC)"
 * matching CadenceClassification's exact label strings
 * (analysis/cadence_classification.h). A pair is identified only when
 * the antecedent's rank is STRICTLY weaker than the consequent's --
 * equal strength (including two "unclassified"/absent phrases, already
 * excluded above) is not a period.
 */

typedef struct {
    const PhraseOccurrence *phrase_occurrence;
    /* Borrowed; NULL or empty means no reliable cadence type for this phrase. Otherwise
       one of CadenceClassification's label strings, including "unclassified". */
    const char *cadence_type;
} PeriodDetectionEntry;

typedef struct {
    const PeriodDetectionEntry *entries;
    size_t entry_count;
} PeriodDetectionInput;

#define PERIOD_DETECTION_PHRASE_ID_BUFFER_SIZE 128
#define PERIOD_DETECTION_CADENCE_TYPE_BUFFER_SIZE 64

typedef struct {
    char antecedent_phrase_occurrence_id[PERIOD_DETECTION_PHRASE_ID_BUFFER_SIZE];
    char consequent_phrase_occurrence_id[PERIOD_DETECTION_PHRASE_ID_BUFFER_SIZE];
    char antecedent_cadence_type[PERIOD_DETECTION_CADENCE_TYPE_BUFFER_SIZE];
    char consequent_cadence_type[PERIOD_DETECTION_CADENCE_TYPE_BUFFER_SIZE];
} PeriodDetectionClaim;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * PeriodDetectionInput; context and config are accepted per the
 * interface but unused. Every entry's phrase_occurrence MUST be
 * non-NULL. entry_count < 2 is valid and yields an empty AnalysisResult
 * (no consecutive pair exists).
 *
 * For each consecutive pair (entries[i], entries[i+1]): if both
 * cadence_types are present, non-empty, not "unclassified", and the
 * first's rank is strictly weaker than the second's (per the ordering
 * above), produces one Relationship (source = entries[i]'s
 * PhraseOccurrence id, predicate "precedes", target = entries[i+1]'s
 * PhraseOccurrence id) and one Interpretation (targeting that
 * Relationship's id, a PeriodDetectionClaim claim, scalar-confidence
 * Uncertainty 1/1) into the returned AnalysisResult, in input order.
 * A pair not meeting that rule produces no output for that pair
 * (skipped, not a low-confidence guess). AnalysisResult here holds only
 * the Interpretations (and their backing Evidence-less provenance) --
 * the Relationship objects themselves are NOT owned by AnalysisResult
 * (it only ever holds Interpretation/Evidence, per analysis/analyzer.h),
 * so callers wanting the Relationship objects use
 * period_detection_build_relationships below instead/in addition. Each
 * Interpretation's target_id is deterministically derived from the same
 * antecedent/consequent PhraseOccurrence id pair
 * period_detection_build_relationships uses to mint its Relationship's
 * own id, so a caller running both against the same input gets matching
 * ids without this call constructing the Relationship object itself.
 */
AnalyzerResult period_detection_analyze(const void *input, const Context *context, const Value *config,
                                         AnalysisResult **out);

extern const Analyzer PERIOD_DETECTION_ANALYZER;

typedef enum {
    PERIOD_DETECTION_REL_OK = 0,
    PERIOD_DETECTION_REL_ERR_NULL_ENTRIES,
    PERIOD_DETECTION_REL_ERR_NULL_ENTRY,
    PERIOD_DETECTION_REL_ERR_ALLOC
} PeriodDetectionRelationshipResult;

/*
 * Builds the Relationship objects backing period_detection_analyze's
 * Interpretations, using the identical pairing rule. entries/entry_count
 * validation matches period_detection_analyze. entry_count < 2 is valid
 * and yields *out_relationships = NULL, *out_relationship_count = 0.
 *
 * On success, *out_relationships is a freshly allocated array (caller
 * frees with period_detection_destroy_relationships) of Relationship
 * pointers, one per identified pair, in input order -- the same
 * ids period_detection_analyze's Interpretations reference as their
 * target (source_id/target_id are the PhraseOccurrences' own ids,
 * predicate_name "precedes").
 */
PeriodDetectionRelationshipResult period_detection_build_relationships(const PeriodDetectionEntry *entries,
                                                                         size_t entry_count,
                                                                         Relationship ***out_relationships,
                                                                         size_t *out_relationship_count);

/* Destroys each Relationship in relationships and the array itself. */
void period_detection_destroy_relationships(Relationship **relationships, size_t relationship_count);

#endif
