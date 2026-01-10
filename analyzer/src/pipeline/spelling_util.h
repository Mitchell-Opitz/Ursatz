#ifndef URSATZ_ANALYZER_SPELLING_UTIL_H
#define URSATZ_ANALYZER_SPELLING_UTIL_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/imported_notes.h>

#include "pipeline/segmentation.h"
#include "pitch/pitch_spelling.h"

/*
 * The note's name as the input spells it: "Db4", or "Db" without the octave.
 * Returns false, writing nothing, when the note has no spelling or its
 * spelling is not the pitch that sounds (a transposing part's spelling is the
 * written pitch), so the caller falls back to spelling by rule.
 */
bool spelled_note_name(const NoteEvent *note, long sounding_pitch, bool with_octave, char out[16]);

/*
 * The spelling of a note name such as "F#4", "Bb3" or "E5": a letter, accidentals (# b x) and an octave. Returns 1 and
 * sets *out (the caller releases it with pitch_spelling_destroy), 0 for a name that is not one, -1 on allocation
 * failure.
 */
int spelling_from_name(const char *name, PitchSpelling **out);

/*
 * The spelling of an imported note for the library's rules: the input's own when it names the pitch that sounds, else
 * the pitch class named in flats or sharps (prefer_flats) at the octave of its MIDI number. Returns as spelling_from_name.
 */
int note_pitch_spelling(const ImportedNotes *notes, size_t note, bool prefer_flats, PitchSpelling **out);

/*
 * Renames the root and bass of a reading with the spelling of the segment's
 * notes: the bass by the lowest sounding note, the root by the lowest note
 * that has its pitch class. A name with no spelled note to follow stays as it
 * is. False on allocation failure.
 */
bool respell_chord_reading(AnalysisChordReading *reading, const ImportedNotes *notes, const Segment *segment);

#endif
