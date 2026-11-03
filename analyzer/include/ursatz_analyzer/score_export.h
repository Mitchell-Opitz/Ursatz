#ifndef URSATZ_ANALYZER_SCORE_EXPORT_H
#define URSATZ_ANALYZER_SCORE_EXPORT_H

#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/imported_notes.h>

/*
 * The score of an import as MusicXML (R-218), written the way the GUI wrote it before the Analyzer owned it: from the
 * imported notes, their parts, staves and General MIDI programs, the source's own meter, and its key signatures in
 * part 0, with times quantized to notated values (score_quantize_options_default). Tempo markings and dynamics are left
 * out, and so are the title and composer: the caller keeps those with the piece and they are not in an import.
 *
 * On ANALYZER_OK *xml is a MusicXML 3.1 string the caller frees with free(); otherwise *xml is NULL.
 * ANALYZER_ERR_ARGUMENT for a NULL argument or an import with a note count but no events; ANALYZER_ERR_LIBRARY when the
 * library cannot write the score (overlapping or unwritable times, a missing spelling); ANALYZER_ERR_ALLOC.
 */
AnalyzerStatus analyzer_score_export_musicxml(const ImportedNotes *notes, char **xml);

#endif
