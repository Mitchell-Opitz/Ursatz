#ifndef URSATZ_ANALYZER_MARKDOWN_REPORT_H
#define URSATZ_ANALYZER_MARKDOWN_REPORT_H

#include <ursatz_analyzer/analysis_document.h>

/*
 * Renders the analysis document as a readable Markdown report. Every
 * fact comes from the document; sections follow the document's section
 * order, and a section whose data is empty or null is omitted. Returns
 * a newly allocated, NUL-terminated string the caller owns (release
 * with free()), or NULL on allocation failure.
 *
 * Chord times come from each claim's span start, a rational time in
 * whole notes (MIDIImporter's tick denominator is 4 x PPQN,
 * io/midi_importer.c). The report renders it as a quarter-note beat
 * position (beats = 4 x numerator / denominator), labeled "beats from
 * the start of the file".
 */
char *markdown_report_render(const AnalysisDocument *document);

#endif
