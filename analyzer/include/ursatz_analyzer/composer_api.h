#ifndef URSATZ_ANALYZER_COMPOSER_API_H
#define URSATZ_ANALYZER_COMPOSER_API_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "time/meter_map.h"

#include <ursatz_analyzer/analysis_report.h>
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

#endif
