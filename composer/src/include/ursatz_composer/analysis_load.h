#ifndef URSATZ_COMPOSER_ANALYSIS_LOAD_H
#define URSATZ_COMPOSER_ANALYSIS_LOAD_H

#include <stddef.h>

#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/imported_notes.h>

#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_json.h>

/*
 * Loading an analyzed piece as a plan (R-019, R-220): the Composer reads the Analyzer's result, never its document.
 * The result describes an existing piece, so every node and lane span of the plan is Realized.
 *
 *   - a Form root over the piece, from time 0 to the end of the last note or chord
 *   - the meter lane from the imported notes' meter map (regions with the same signature are one span)
 *   - the key lane from the chain's key regions (analysis_chain_key_regions)
 *   - the harmony lane from analyzer_chord_sequence: the joined chords, Roman numeral, no function (a result records none)
 *   - a Section node over the piece holding a Phrase node for each phrase of the chain (analysis_chain_phrases)
 *   - the melody lane from the soprano line and the bass lane from the bass line of the chain's outer voices, a note for
 *     each run of segments the same note sounds in
 *
 * The plan records the Analyzer's version (plan_analyzer_version, ursatz_analyzer_version()).
 *
 * Nothing is dropped silently. The report lists every element that was left out or changed:
 *   - a pickup measure (its signature is the meter of the span it is joined to)
 *   - a phrase that overlaps the one before it or does not fit the piece (skipped)
 *   - a key region or chord that cannot be placed on the timeline (skipped)
 *   - an area of the result the plan has no place for yet: cadences, themes, motives, texture, voice leading, schemas
 *     (listed once, when the result has claims for it)
 */

#define ANALYSIS_LOAD_NOTE_MAX 160

typedef struct {
    char path[PLAN_JSON_PATH_MAX];
    char message[ANALYSIS_LOAD_NOTE_MAX];
} AnalysisLoadNote;

typedef struct {
    AnalysisLoadNote *notes;
    size_t count;
} AnalysisLoadReport;

/* Releases the notes; safe on a zeroed report. */
void analysis_load_report_destroy(AnalysisLoadReport *report);

/*
 * Builds a plan from a result and the notes it was made from. report may be NULL; otherwise it is filled (possibly with
 * no notes) and the caller destroys it. *out is the plan on PLAN_OK, NULL otherwise. Fails with PLAN_ERR_NULL for a NULL
 * result, notes or out, PLAN_ERR_FORMAT when the result has no notes or no key region or chord to build a timeline on,
 * PLAN_ERR_ALLOC.
 */
PlanResult plan_from_analyzer_result(const AnalyzerResult *result, const ImportedNotes *notes, PiecePlan **out,
                                     AnalysisLoadReport *report);

#endif
