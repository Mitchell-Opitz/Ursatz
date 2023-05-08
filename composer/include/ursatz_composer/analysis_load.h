#ifndef URSATZ_COMPOSER_ANALYSIS_LOAD_H
#define URSATZ_COMPOSER_ANALYSIS_LOAD_H

#include <stddef.h>

#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_json.h>

/*
 * Loading an Ursatz-Analyzer analysis document (schema_version 1) as a plan.
 * The document describes an existing piece, so every node and lane span of
 * the plan is Realized.
 *
 * What maps today:
 *   - a Form root over the extent of the key and chord claims, named for
 *     metadata.source_file (the document has no form tree yet)
 *   - "keys" claims  -> key lane
 *   - "chords" claims -> harmony lane, Roman numeral from the label,
 *     function PLAN_FUNCTION_UNKNOWN (a claim carries none)
 *
 * Nothing is dropped silently. The report lists every element that was left
 * out or changed:
 *   - a "no match" or unreadable key or chord label (skipped)
 *   - a chord that runs past the start of the next one (trimmed to it)
 *   - a section with content the plan has no place for yet: meter, cadences,
 *     phrases, form, motives, melody, rhythm, texture, voice_leading, schemas
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
 * Builds a plan from an analysis document. report may be NULL; otherwise it is
 * filled (possibly with no notes) and the caller destroys it. Fails like
 * plan_from_json, and with PLAN_ERR_FORMAT when no key or chord claim can be
 * mapped, since there is then no timeline to build a plan on.
 */
PlanResult plan_from_analysis_document(const char *text, PiecePlan **out, AnalysisLoadReport *report,
                                       PlanJsonError *error);
PlanResult plan_read_analysis_document_file(const char *path, PiecePlan **out, AnalysisLoadReport *report,
                                            PlanJsonError *error);

#endif
