#ifndef URSATZ_ANALYZER_COMPOSER_API_H
#define URSATZ_ANALYZER_COMPOSER_API_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "time/meter_map.h"

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/imported_notes.h>
#include <ursatz_analyzer/style_profile.h>

/*
 * The in-memory API the Composer calls to analyze a fragment it generated. The fragment never touches a file: its
 * events are borrowed into an ImportedNotes and the same pipeline the command line runs analyzes them.
 */

/*
 * The hearing entry (B-C-9): the Composer's way to have a fragment analyzed. The events are borrowed, never copied, and
 * the result is an AnalyzerResult (analysis_result.h); the analysis document is rendered only when asked for.
 *
 * Everything the hearing points to is borrowed and must outlive the result; none of it is changed (Law 20). events has
 * count entries; midi, when not NULL, has count parallel MIDI note numbers 0 to 127. The library has no call that reads
 * a MIDI number from an event's pitch (a pitch identity is opaque by design), so midi is required for count > 0:
 * ANALYZER_ERR_ARGUMENT when it is NULL. A NULL meter is 4/4. has_key gives a key signature event at time 0 from
 * key_tonic_pitch_class (0 to 11, C is 0) and key_minor. streams, when not NULL, has count entries: the stream (voice
 * line or part) each event belongs to; NULL puts every event in stream 0. streams is read, not kept.
 */
typedef struct {
    const NoteEvent *const *events;
    size_t count;
    const int *midi;
    const MeterMap *meter;
    bool has_key;
    int key_tonic_pitch_class;
    bool key_minor;
    const size_t *streams;
} AnalyzerHearing;

typedef struct {
    AnalyzerOptions analysis;
    bool render_document; /* render the analysis document into the result; needs every stage */
} AnalyzerHearingOptions;

/* All stages, no cancel callback, no document. */
AnalyzerHearingOptions analyzer_hearing_options_default(void);

/*
 * Hears the events into *out, which the caller releases with analyzer_result_destroy. options may be NULL for the
 * defaults. ANALYZER_ERR_ARGUMENT for a missing argument, a NULL event, a MIDI number outside 0 to 127, a key tonic
 * outside 0 to 11, no midi for count > 0, or render_document without every stage; the other statuses are those of
 * analyzer_analyze. *out is NULL on failure and nothing is kept.
 */
AnalyzerStatus analyzer_hear(const AnalyzerHearing *hearing, const AnalyzerHearingOptions *options,
                             AnalyzerResult **out);

/* The analysis document text of a result heard with render_document. Valid until the result is destroyed; NULL
   otherwise. */
const char *analyzer_result_document_json(const AnalyzerResult *result);

/*
 * A style the fragments are measured against, prepared once: the model's statistics with the named categories
 * dropped. One reference serves any number of results.
 */
typedef struct AnalyzerStyleReference AnalyzerStyleReference;

/*
 * Prepares model for distances. without names the categories left out (the list the model was read without,
 * style_model_file_read_without); they appear nowhere in a distance. ANALYZER_ERR_ARGUMENT for a NULL model or out, an
 * unknown category name, or count > 0 with NULL without. *out is NULL on failure. The reference copies what it needs:
 * model need not outlive it.
 */
AnalyzerStatus analyzer_style_reference_create(const StyleModel *model, const char *const *without,
                                               size_t without_count, AnalyzerStyleReference **out);

/* NULL is a no-op. */
void analyzer_style_reference_destroy(AnalyzerStyleReference *reference);

/*
 * How far the result is from the reference: the one-piece profile of the result (read from the result, not from its
 * document text) compared with the reference's statistics, with the categories the result has no data for
 * left out of the means and kept in out->categories. Release out with style_comparison_destroy.
 * ANALYZER_ERR_ARGUMENT for a NULL argument or a result that did not run every chain stage.
 */
AnalyzerStatus analyzer_result_style_distance(const AnalyzerResult *result, const AnalyzerStyleReference *reference,
                                              StyleComparison *out);

/*
 * One rule the fragment breaks. rule is one of "parallel-fifths", "parallel-octaves", "direct-fifths",
 * "direct-octaves", "unresolved-seventh", "unresolved-leading-tone" (counterpoint is checked with
 * standard-counterpoint, not here). hard is true for parallel fifths and octaves. The place is the measure and beat
 * (1 is the first beat, a fraction when between beats) where the fault arrives. measure is 0 and beat 0 when the meter cannot
 * place it. note_ids are indices into the events the result was heard from.
 */
typedef struct {
    char *rule;
    bool hard;
    int measure;
    double beat;
    size_t *note_ids;
    size_t note_id_count;
} AnalyzerViolation;

/*
 * Every voice-leading fault the result's report holds (parallels, directs, unresolved sevenths and leading tones, in
 * that order). The rules, places and
 * hardness are those of AnalyzerViolation; note_ids are indices into the events the result was heard from. *out is a
 * new array the caller releases with analyzer_violations_free; it is NULL when *count is 0. ANALYZER_ERR_ARGUMENT for
 * a NULL argument, ANALYZER_ERR_ALLOC when the list cannot be built (*out NULL, *count 0).
 */
AnalyzerStatus analyzer_result_violations(const AnalyzerResult *result, AnalyzerViolation **out, size_t *count);

void analyzer_violations_free(AnalyzerViolation *violations, size_t count);

#endif
