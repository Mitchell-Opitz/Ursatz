#ifndef URSATZ_ANALYZER_SPELLING_UTIL_H
#define URSATZ_ANALYZER_SPELLING_UTIL_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/imported_notes.h>

#include "pipeline/segmentation.h"

/*
 * The note's name as the input spells it: "Db4", or "Db" without the octave.
 * Returns false, writing nothing, when the note has no spelling or its
 * spelling is not the pitch that sounds (a transposing part's spelling is the
 * written pitch), so the caller falls back to spelling by rule.
 */
bool spelled_note_name(const NoteEvent *note, long sounding_pitch, bool with_octave, char out[16]);

/*
 * Renames the root and bass of a reading with the spelling of the segment's
 * notes: the bass by the lowest sounding note, the root by the lowest note
 * that has its pitch class. A name with no spelled note to follow stays as it
 * is. False on allocation failure.
 */
bool respell_chord_reading(AnalysisChordReading *reading, const ImportedNotes *notes, const Segment *segment);

#endif
