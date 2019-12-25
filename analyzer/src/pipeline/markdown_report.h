#ifndef URSATZ_ANALYZER_MARKDOWN_REPORT_H
#define URSATZ_ANALYZER_MARKDOWN_REPORT_H

#include "pipeline/analysis_report.h"

/*
 * Renders report as a readable Markdown document titled after
 * source_filename (e.g. the input MIDI path -- used for display only,
 * not opened). Returns a newly allocated, NUL-terminated string the
 * caller owns (release with free()), or NULL on allocation failure.
 *
 * JUDGMENT CALL: each entry's time_numerator/time_denominator is a
 * MusicalTime fraction in MIDIImporter's tick convention (denominator
 * = 2 x PPQN, io/midi_importer.c) -- not inherently meaningful to a
 * reader. This renders it as a beat position (beats = 2 x numerator /
 * denominator, since that convention makes 1 beat a quarter note, half
 * of the unit), labeled explicitly as "beats from the start of the
 * file" so the reader knows what the number means. This relies on the
 * current single NoteEvent producer's fixed convention; a different
 * importer using another time convention would need this revisited.
 */
char *markdown_report_render(const char *source_filename, const AnalysisReport *report);

#endif
