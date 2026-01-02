#ifndef URSATZ_ANALYZER_MARKINGS_PIPELINE_H
#define URSATZ_ANALYZER_MARKINGS_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/imported_notes.h>

/*
 * The tempo and dynamics markings of a notated score, read with Ursatz's standard-markings rules. The analyzer adds no
 * music theory (R-018): the words, the metronome marks, the positions and the spans are the score's (kept by the
 * MusicXML import, ImportedSource); the tempo group and change, the dynamic rank and change, and whether a dynamic is
 * louder or quieter than the one before are the library's. A tempo marking holds to the next one, a dynamic to the next
 * in its part (the importer's span). Articulations are not imported (R-056), so none are read.
 *
 * MIDI input carries no markings: nothing is filled and that is not an error.
 */

/* Fills the report's tempo markings and dynamics. Returns false only on allocation failure. */
bool markings_stage_collect(const ImportedNotes *notes, AnalysisReport *report);

#endif
