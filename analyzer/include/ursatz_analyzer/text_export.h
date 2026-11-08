#ifndef URSATZ_ANALYZER_TEXT_EXPORT_H
#define URSATZ_ANALYZER_TEXT_EXPORT_H

#include <stddef.h>

#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/imported_notes.h>

/*
 * The text export (docs/text-export.md): what the Score and Analysis tabs show, as plain text a person can paste into a
 * conversation to say "measure 27 is flagged as X, I read it as Y". The notes are read back from the score MusicXML
 * (analyzer_score_export_musicxml), so the text shows exactly what the Score tab draws, wrong or not; the claims come
 * from the analysis document, placed in the measure where each starts.
 *
 * Value of the format version, written as "# Ursatz export v2" on the first line. A change to the line layout bumps it.
 */
#define URSATZ_ANALYZER_TEXT_EXPORT_VERSION 2

typedef struct {
    /* Printed as "title:". NULL or empty: the file name the document records, else "untitled". */
    const char *title;
    /* An inclusive range of measure numbers as written in the score. 0 and 0 export the whole piece. A pickup measure
       (number 0) is included when first_measure is 0 or 1. */
    int first_measure;
    int last_measure;
} AnalyzerTextExportOptions;

/* The whole piece, no title. */
AnalyzerTextExportOptions analyzer_text_export_options_default(void);

/*
 * Exports one analyzed piece. notes is the import the analysis was run on; document_json is its analysis document
 * (analysis_document_render_result or the stored analysis.json).
 *
 * On ANALYZER_OK *text is the export, which the caller frees with free(); otherwise *text is NULL.
 * ANALYZER_ERR_ARGUMENT for a NULL argument, a document that is not JSON or whose schema_version is not
 * URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION, or an empty range (first greater than last, or no measure in range);
 * ANALYZER_ERR_LIBRARY when the score cannot be written or read back; ANALYZER_ERR_ALLOC.
 */
AnalyzerStatus analyzer_text_export_piece(const ImportedNotes *notes, const char *document_json,
                                          const AnalyzerTextExportOptions *options, char **text);

/* A name and its value, printed as "<name>: <value>" on one line (a line break in either becomes a space). */
typedef struct {
    const char *name;
    const char *value;
} AnalyzerTextExportFact;

/*
 * A run of notes in one section that the caller names, printed as "[label]" after the segment's first note and
 * "[/label]" after its last. first and last are 0-based indexes into the section's notes in time order, inclusive; a
 * chord is one note, rests are not counted, and notes that start together go by part, staff and voice.
 */
typedef struct {
    const char *label;
    size_t first;
    size_t last;
} AnalyzerTextExportSegment;

/* One MusicXML score of a piece of material (a motif, one of its variants, a theme, a loop). */
typedef struct {
    const char *label;
    /* The score as MusicXML (or MIDI) bytes, NUL-terminated for MusicXML. NULL: the section is printed as "not
       composed", with its label and facts only. */
    const char *musicxml;
    const AnalyzerTextExportFact *facts;
    size_t fact_count;
    const AnalyzerTextExportSegment *segments;
    size_t segment_count;
} AnalyzerTextExportSection;

/* A piece of store material: a motif and its variants, a theme and its slot motifs, a piece's parts. */
typedef struct {
    const char *kind; /* printed as given: "motif", "theme", "piece" */
    const char *id;
    const char *name;
    int version;
    const AnalyzerTextExportFact *facts;
    size_t fact_count;
    const AnalyzerTextExportSection *sections;
    size_t section_count;
} AnalyzerTextExportMaterial;

/*
 * Exports material: a header (kind, name, id, version, facts), then each section as "## <label>", its facts and its
 * measures, analyzed and formatted as analyzer_text_export_piece does. Every value is printed as the caller passed it;
 * the Analyzer reads no store or Composer format.
 *
 * On ANALYZER_OK *text is the export, which the caller frees with free(); otherwise *text is NULL.
 * ANALYZER_ERR_ARGUMENT for a NULL material, kind or text, a NULL label, or a segment outside its section's notes;
 * ANALYZER_ERR_LIBRARY when a section's bytes cannot be imported or its score cannot be written or read back;
 * ANALYZER_ERR_ALLOC.
 */
AnalyzerStatus analyzer_text_export_material(const AnalyzerTextExportMaterial *material, char **text);

#endif
