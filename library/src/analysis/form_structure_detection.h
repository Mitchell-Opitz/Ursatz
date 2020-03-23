#ifndef MUSIC_ANALYSIS_FORM_STRUCTURE_DETECTION_H
#define MUSIC_ANALYSIS_FORM_STRUCTURE_DETECTION_H

#include <stddef.h>

#include "analysis/analyzer.h"
#include "containers/section.h"
#include "relationship/relationship.h"
#include "structure/phrase_occurrence.h"

/*
 * FormStructureDetection (Procedure -> Analyzer implementation):
 * groups consecutive PhraseOccurrences into formal Sections (L4
 * containers/section.h) at the strongest, section-closing cadences, and
 * orders the identified Sections via a "precedes" Relationship chain --
 * the section-order half of this branch's scope (thematic return is
 * analysis/thematic_return_detection.h).
 *
 * No new Structure/Occurrence type: Section already exists as a real,
 * non-transient L4 container (containers/section.h) -- unlike
 * PhraseOccurrence's precedent (a transient structure minted only to
 * hold an id), Section is minted once per identified group and used
 * directly, no different from any other caller of section_create.
 *
 * Structural (section-closing) cadence default (flagged design default,
 * NOT Registry-settled, per this branch's requirement 2): only
 * "authentic (PAC)" and "authentic (IAC)" count as structural --
 * they are the only cadence types that actually achieve tonic
 * resolution. "authentic (unclassified: PAC/IAC)", "plagal",
 * "deceptive", and "half cadence (HC)" all either substitute for or
 * avoid landing on tonic -- they work as internal phrase punctuation,
 * not section-ending closure. An ambiguous PAC/IAC call
 * ("unclassified") is deliberately NOT promoted to structural: the same
 * unknown-vs-low-confidence discipline used throughout this codebase.
 * Matches CadenceClassification's exact label strings
 * (analysis/cadence_classification.h), the same strings
 * PeriodDetection's cadence_type already uses.
 *
 * Boundary rule: a structural cadence at entries[i] (i.e. not the last
 * entry) closes a Section spanning entries since the previous boundary
 * (or the start) through i, inclusive. If NO entry (other than possibly
 * the last) carries a structural cadence, NO boundary is found and this
 * analyzer produces no output at all (requirement 4 -- no detected form
 * structure is a legitimate outcome, never forced). A trailing run of
 * entries after the last boundary that never reaches another structural
 * cadence is not turned into a Section of its own -- only spans that are
 * actually closed by a structural cadence become Sections, the same
 * discipline PeriodDetection/SentenceDetection apply to their own
 * pairs/chains (skip, don't guess).
 *
 * Linking mechanism: reuses PeriodDetection's exact pattern. Each
 * identified Section gets minted via section_create (id + phrase_ids =
 * the underlying L4 Phrase ids of its member PhraseOccurrences, via
 * phrase_occurrence_phrase_id). Consecutive Sections (in input order)
 * are linked by one Relationship each (source = earlier Section id,
 * predicate "precedes", target = later Section id -- the identical
 * chronological semantics PeriodDetection already uses "precedes" for),
 * then one Interpretation targeting that Relationship's id, carrying a
 * FormStructureDetectionClaim with both Sections' ids and closing
 * cadence types.
 */

typedef struct {
    const PhraseOccurrence *phrase_occurrence;
    /* Borrowed; NULL or empty means no reliable cadence type for this phrase. Otherwise
       one of CadenceClassification's label strings, including "unclassified". */
    const char *cadence_type;
} FormStructureDetectionEntry;

typedef struct {
    const FormStructureDetectionEntry *entries;
    size_t entry_count;
} FormStructureDetectionInput;

#define FORM_STRUCTURE_DETECTION_ID_BUFFER_SIZE 128
#define FORM_STRUCTURE_DETECTION_CADENCE_TYPE_BUFFER_SIZE 64

typedef struct {
    char earlier_section_id[FORM_STRUCTURE_DETECTION_ID_BUFFER_SIZE];
    char later_section_id[FORM_STRUCTURE_DETECTION_ID_BUFFER_SIZE];
    char earlier_section_closing_cadence_type[FORM_STRUCTURE_DETECTION_CADENCE_TYPE_BUFFER_SIZE];
} FormStructureDetectionClaim;

/*
 * Conforms to AnalyzerFn (analysis/analyzer.h): input MUST point to a
 * FormStructureDetectionInput; context and config are accepted per the
 * interface but unused. Every entry's phrase_occurrence MUST be
 * non-NULL. entry_count == 0, or an input with no structural cadence
 * among entries[0..entry_count-2], is valid and yields an empty
 * AnalysisResult (no Section boundary found -- requirement 4).
 *
 * On finding N >= 2 Sections (N - 1 >= 1 "precedes" links), produces
 * N - 1 Interpretations, one per consecutive Section pair, in
 * chronological order. AnalysisResult here holds only the
 * Interpretations (and their backing absent-provenance) -- neither the
 * Section nor Relationship objects themselves are owned by
 * AnalysisResult (it only ever holds Interpretation/Evidence, per
 * analysis/analyzer.h); callers wanting those objects use
 * form_structure_detection_build_sections /
 * form_structure_detection_build_relationships below instead/in
 * addition. Ids are deterministically derived from the same
 * PhraseOccurrence ids those calls use, so a caller running all three
 * against the same input gets matching ids without this call
 * constructing the Section/Relationship objects itself.
 */
AnalyzerResult form_structure_detection_analyze(const void *input, const Context *context, const Value *config,
                                                 AnalysisResult **out);

extern const Analyzer FORM_STRUCTURE_DETECTION_ANALYZER;

typedef enum {
    FORM_STRUCTURE_DETECTION_OK = 0,
    FORM_STRUCTURE_DETECTION_ERR_NULL_ENTRIES,
    FORM_STRUCTURE_DETECTION_ERR_NULL_ENTRY,
    FORM_STRUCTURE_DETECTION_ERR_ALLOC
} FormStructureDetectionSectionResult;

/*
 * Builds the Section objects FormStructureDetection identifies, using
 * the identical boundary rule. entries/entry_count validation matches
 * form_structure_detection_analyze. No boundary found is valid and
 * yields *out_sections = NULL, *out_section_count = 0.
 *
 * On success, *out_sections is a freshly allocated array (caller frees
 * with form_structure_detection_destroy_sections) of one Section per
 * identified boundary-closed span, in chronological order.
 */
FormStructureDetectionSectionResult form_structure_detection_build_sections(
    const FormStructureDetectionEntry *entries, size_t entry_count, Section ***out_sections,
    size_t *out_section_count);

/* Destroys each Section in sections and the array itself. */
void form_structure_detection_destroy_sections(Section **sections, size_t section_count);

typedef enum {
    FORM_STRUCTURE_DETECTION_REL_OK = 0,
    FORM_STRUCTURE_DETECTION_REL_ERR_NULL_ENTRIES,
    FORM_STRUCTURE_DETECTION_REL_ERR_NULL_ENTRY,
    FORM_STRUCTURE_DETECTION_REL_ERR_ALLOC
} FormStructureDetectionRelationshipResult;

/*
 * Builds the Relationship objects backing form_structure_detection_
 * analyze's Interpretations, using the identical boundary rule and the
 * identical Section ids form_structure_detection_build_sections mints.
 * entries/entry_count validation matches form_structure_detection_
 * analyze. Fewer than 2 Sections found is valid and yields
 * *out_relationships = NULL, *out_relationship_count = 0.
 *
 * On success, *out_relationships is a freshly allocated array (caller
 * frees with form_structure_detection_destroy_relationships) of one
 * Relationship per consecutive Section pair (predicate "precedes"), in
 * chronological order.
 */
FormStructureDetectionRelationshipResult form_structure_detection_build_relationships(
    const FormStructureDetectionEntry *entries, size_t entry_count, Relationship ***out_relationships,
    size_t *out_relationship_count);

/* Destroys each Relationship in relationships and the array itself. */
void form_structure_detection_destroy_relationships(Relationship **relationships, size_t relationship_count);

#endif
