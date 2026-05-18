#ifndef URSATZ_ANALYZER_COMPOSER_API_H
#define URSATZ_ANALYZER_COMPOSER_API_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "time/meter_map.h"

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/imported_notes.h>

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

#endif
