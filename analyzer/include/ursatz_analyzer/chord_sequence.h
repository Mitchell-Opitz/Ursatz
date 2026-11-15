#ifndef URSATZ_ANALYZER_CHORD_SEQUENCE_H
#define URSATZ_ANALYZER_CHORD_SEQUENCE_H

#include <stddef.h>

#include <ursatz_analyzer/analysis_result.h>

#include "core/rational.h"

/*
 * The joined chord sequence: the chords of an analysis in time order, where consecutive chord entries with the same
 * Roman numeral, key region and root count as one chord (a chord held across several segments). An entry the analysis
 * gave no numeral, root or key region for is not a chord; it ends the run before it. The sequence of a result and the
 * sequence of its stored document are the same (R-220: the Analyzer is the one reader of its document).
 *
 * first_entry and last_entry are indices into the entries of the report, which are the "chords" of the document, both
 * inclusive. The numeral is the root-position numeral in the style label form (style_label_numeral), relative to the
 * key region. start and end are in whole notes, from the first entry's start to the last entry's end.
 */
typedef struct {
    char *numeral;         /* owned */
    int root_pitch_class;  /* 0 to 11 */
    long key_region;
    size_t first_entry;
    size_t last_entry;
    Rational start;
    Rational end;
} AnalyzerChord;

/*
 * On ANALYZER_OK *out is the sequence (NULL when *count is 0), which the caller releases with
 * analyzer_chord_sequence_free. A result whose stages did not collect the report gives an empty sequence. On any other
 * status *out is NULL and *count is 0: ANALYZER_ERR_ARGUMENT for a NULL argument, text that is not an analysis document
 * of the current schema version, or a chord whose time cannot be read; ANALYZER_ERR_ALLOC.
 */
AnalyzerStatus analyzer_chord_sequence(const AnalyzerResult *result, AnalyzerChord **out, size_t *count);
AnalyzerStatus analyzer_chord_sequence_from_document(const char *document_json, AnalyzerChord **out, size_t *count);

/* Frees a sequence from either call. NULL is a no-op. */
void analyzer_chord_sequence_free(AnalyzerChord *chords, size_t count);

#endif
