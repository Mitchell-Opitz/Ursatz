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
 * events are copied into an ImportedNotes, the same pipeline the command line runs analyzes them, and the report and
 * the analysis document are kept for the caller to read.
 */

/*
 * A fragment of generated material. The caller keeps ownership of everything the fragment points to; none of it is
 * changed (Law 20). Every event's pitch must be a MIDI note number 0 to 127 made the way the MIDI importer makes it
 * (a pitch identity of the "midi-note-number" tuning); anything else is ANALYZER_FRAGMENT_ERR_PITCH.
 *
 * meter may be NULL: the fragment is then in 4/4. has_key gives a key signature event at time 0 from
 * key_tonic_pitch_class (0 to 11, C is 0) and key_minor. streams, when not NULL, has count entries: the stream
 * (voice line or part) each event belongs to; NULL puts every event in stream 0.
 */
typedef struct {
    const NoteEvent *const *events;
    size_t count;
    const MeterMap *meter;
    bool has_key;
    int key_tonic_pitch_class;
    bool key_minor;
    const size_t *streams;
} AnalyzerFragment;

typedef enum {
    ANALYZER_FRAGMENT_OK = 0,
    ANALYZER_FRAGMENT_ERR_ARGUMENT, /* a missing argument, a NULL event, or a key tonic outside 0 to 11 */
    ANALYZER_FRAGMENT_ERR_PITCH,    /* an event whose pitch is not a MIDI note number */
    ANALYZER_FRAGMENT_ERR_ALLOC
} AnalyzerFragmentResult;

/*
 * What the analysis of one fragment holds. notes is the copy of the fragment's events; report is the pipeline's
 * report; document_json is the analysis document (input path "<fragment>"). Read them through the accessors.
 */
typedef struct {
    ImportedNotes notes;
    AnalysisReport report;
    char *document_json;
    bool valid;
} AnalyzerFragmentAnalysis;

/*
 * The hearing entry (B-C-9): the Composer's way to have a fragment analyzed. The events are borrowed, never copied, and
 * the result is an AnalyzerResult (analysis_result.h); the analysis document is rendered only when asked for.
 *
 * Everything the hearing points to is borrowed and must outlive the result; none of it is changed (Law 20). events has
 * count entries; midi, when not NULL, has count parallel MIDI note numbers 0 to 127. The library has no call that reads
 * a MIDI number from an event's pitch (a pitch identity is opaque by design), so midi is required for count > 0:
 * ANALYZER_ERR_ARGUMENT when it is NULL. meter, has_key, key_tonic_pitch_class, key_minor and streams are as for
 * AnalyzerFragment (a NULL meter is 4/4, a NULL streams puts every event in stream 0; streams is read, not kept).
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
 * document text) compared as analyzer_fragment_style_distance does, with the categories the result has no data for
 * left out of the means and kept in out->categories. Release out with style_comparison_destroy.
 * ANALYZER_ERR_ARGUMENT for a NULL argument or a result that did not run every chain stage.
 */
AnalyzerStatus analyzer_result_style_distance(const AnalyzerResult *result, const AnalyzerStyleReference *reference,
                                              StyleComparison *out);

/*
 * Analyzes the fragment into *out, which the caller releases with analyzer_fragment_analysis_destroy. *out is left
 * empty on failure. A fragment of no events is analyzed too (an empty report).
 */
AnalyzerFragmentResult analyzer_analyze_fragment(const AnalyzerFragment *fragment, AnalyzerFragmentAnalysis *out);

/* Releases everything the analysis holds. NULL, or an analysis that was never filled, is a no-op. */
void analyzer_fragment_analysis_destroy(AnalyzerFragmentAnalysis *analysis);

/* The report. Valid until the analysis is destroyed; NULL for NULL or an analysis that was not filled. */
const AnalysisReport *analyzer_fragment_report(const AnalyzerFragmentAnalysis *analysis);

/* The analysis document text. Valid until the analysis is destroyed; NULL as above. */
const char *analyzer_fragment_document_json(const AnalyzerFragmentAnalysis *analysis);

/*
 * Counterpoint check. Runs the species check (1 to 5, the cantus firmus in the upper voice or the lower) over the
 * fragment, which must be two voices (see pipeline/counterpoint_pipeline.h). Its violations then appear in
 * analyzer_fragment_violations. Returns ANALYZER_FRAGMENT_ERR_ARGUMENT for a fragment that is not two voices.
 */
AnalyzerFragmentResult analyzer_fragment_check_counterpoint(AnalyzerFragmentAnalysis *analysis, int species,
                                                            bool cantus_upper);

/*
 * One rule the fragment breaks. rule is one of "parallel-fifths", "parallel-octaves", "direct-fifths",
 * "direct-octaves", "unresolved-seventh", "unresolved-leading-tone" or "counterpoint:<group>" (the library's rule
 * group of the species check). hard is true for parallel fifths and octaves and for a counterpoint hard rule. The
 * place is the measure and beat (1 is the first beat, a fraction when between beats) where the fault arrives; a
 * counterpoint violation is placed at the start of the exercise. measure is 0 and beat 0 when the meter cannot
 * place it. note_ids are indices into the fragment's events.
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
 * Every voice-leading fault the report holds (parallels, directs, unresolved sevenths and leading tones, in that
 * order) and, when the counterpoint check ran, every counterpoint violation. *out is a new array the caller releases
 * with analyzer_violations_free; it is NULL when *count is 0.
 */
AnalyzerFragmentResult analyzer_fragment_violations(const AnalyzerFragmentAnalysis *analysis, AnalyzerViolation **out,
                                                    size_t *count);

void analyzer_violations_free(AnalyzerViolation *violations, size_t count);

/*
 * How far the fragment is from a style. A one-piece profile of the fragment (style_profile_add_document_json) is
 * compared with the model (style_profile_compare), except that the categories the fragment has no data for (a
 * fragment is short) are left out of the dimension and overall means. They stay in out->categories, with in_a false
 * (the fragment has none) and in_b true, so a caller lists them as missing. Release out with
 * style_comparison_destroy. ANALYZER_FRAGMENT_ERR_ARGUMENT for a missing argument or an invalid analysis.
 */
AnalyzerFragmentResult analyzer_fragment_style_distance(const AnalyzerFragmentAnalysis *analysis,
                                                        const StyleModel *model, StyleComparison *out);

/*
 * The same distance with the named categories left out: they are dropped from the fragment's statistics and from the
 * model's before the comparisons, so they appear nowhere in *out (not in the means, not in out->categories). Pass the
 * list the model was read without (style_model_file_read_without); without it a category the fragment has and the
 * model lacks counts as distance 1. ANALYZER_FRAGMENT_ERR_ARGUMENT for an unknown category name, or for count > 0
 * with NULL categories, besides the cases of analyzer_fragment_style_distance.
 */
AnalyzerFragmentResult analyzer_fragment_style_distance_without(const AnalyzerFragmentAnalysis *analysis,
                                                                const StyleModel *model,
                                                                const char *const *categories, size_t count,
                                                                StyleComparison *out);

#endif
