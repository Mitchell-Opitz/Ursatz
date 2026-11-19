#ifndef MUSIC_ANALYSIS_FORM_STRUCTURE_DETECTION_H
#define MUSIC_ANALYSIS_FORM_STRUCTURE_DETECTION_H

#include <stddef.h>

#include "model/containers/section.h"
#include "theory/vocabulary/cadence_type.h"
#include "model/structure/phrase_occurrence.h"

/*
 * FormStructureDetection (Procedure):
 * groups consecutive PhraseOccurrences into formal Sections (L4
 * containers/section.h) at the strongest, section-closing cadences, and
 * orders the identified Sections as pairs of indices (each section before the next) --
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
 * CADENCE_TYPE_PERFECT_AUTHENTIC and CADENCE_TYPE_IMPERFECT_AUTHENTIC
 * count as structural -- they are the only cadence types that actually
 * achieve tonic resolution. AUTHENTIC_UNSPECIFIED, PLAGAL, DECEPTIVE,
 * and HALF all either substitute for or avoid landing on tonic -- they
 * work as internal phrase punctuation, not section-ending closure. An
 * ambiguous PAC/IAC call (AUTHENTIC_UNSPECIFIED) is deliberately NOT
 * promoted to structural: the same unknown-vs-low-confidence discipline
 * used throughout this codebase. cadence_type is a CadenceType
 * (semantics/cadence_type.h).
 *
 * Boundary rule: a structural cadence at entries[i] (i.e. not the last
 * entry) closes a Section spanning entries since the previous boundary
 * (or the start) through i, inclusive. If NO entry (other than possibly
 * the last) carries a structural cadence, NO boundary is found and this
 * procedure produces no output at all (requirement 4 -- no detected form
 * structure is a legitimate outcome, never forced). A trailing run of
 * entries after the last boundary that never reaches another structural
 * cadence is not turned into a Section of its own -- only spans that are
 * actually closed by a structural cadence become Sections (skip, don't
 * guess).
 *
 * Section minting: each identified Section gets minted via section_create (id + phrase_ids =
 * the underlying L4 Phrase ids of its member PhraseOccurrences, via
 * phrase_occurrence_phrase_id). Consecutive Sections (in input order)
 * are ordered by one FormStructureOrder each (the index of the earlier and the later Section). Nothing
 * else is built for them.
 */

typedef struct {
    const PhraseOccurrence *phrase_occurrence;
    CadenceType cadence_type; /* CADENCE_TYPE_UNKNOWN: no reliable cadence for this phrase */
} FormStructureDetectionEntry;

typedef enum {
    FORM_STRUCTURE_DETECTION_OK = 0,
    FORM_STRUCTURE_DETECTION_ERR_NULL_ENTRIES,
    FORM_STRUCTURE_DETECTION_ERR_NULL_ENTRY,
    FORM_STRUCTURE_DETECTION_ERR_ALLOC
} FormStructureDetectionSectionResult;

/*
 * Builds the Section objects FormStructureDetection identifies, using
 * the identical boundary rule. entries/entry_count validation matches
 * form_structure_detection_order. No boundary found is valid and
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

/* The order of two consecutive Sections: indices into the array form_structure_detection_build_sections gives. */
typedef struct {
    size_t earlier;
    size_t later;
} FormStructureOrder;

/*
 * Gives the order of the identified Sections, using the identical boundary rule (the same Sections
 * form_structure_detection_build_sections mints): one FormStructureOrder {k, k + 1} per consecutive Section pair, in
 * chronological order. entries/entry_count validation matches form_structure_detection_build_sections. Fewer than 2
 * Sections found is valid and yields *out_pairs = NULL, *out_pair_count = 0. On success *out_pairs is a freshly allocated
 * array the caller frees with free().
 */
FormStructureDetectionSectionResult form_structure_detection_order(const FormStructureDetectionEntry *entries, size_t entry_count,
                                                                    FormStructureOrder **out_pairs, size_t *out_pair_count);

#endif
