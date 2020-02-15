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
 * JUDGMENT CALL: chord times come from each claim's span start, a
 * MusicalTime fraction in MIDIImporter's tick convention (denominator
 * = 2 x PPQN, io/midi_importer.c) -- not inherently meaningful to a
 * reader. This renders it as a beat position (beats = 2 x numerator /
 * denominator, since that convention makes 1 beat a quarter note, half
 * of the unit), labeled explicitly as "beats from the start of the
 * file" so the reader knows what the number means. This relies on the
 * current single NoteEvent producer's fixed convention; a different
 * importer using another time convention would need this revisited.
 */
char *markdown_report_render(const AnalysisDocument *document);

#endif
