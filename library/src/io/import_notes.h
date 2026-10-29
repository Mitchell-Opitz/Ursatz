#ifndef MUSIC_IO_IMPORT_NOTES_H
#define MUSIC_IO_IMPORT_NOTES_H

#include <stdbool.h>
#include <stddef.h>

#include "io/midi_importer.h"
#include "io/musicxml_importer.h"
#include "semantics/analysis_note.h"

/*
 * Neutral notes from imports (L9, music-io): one AnalysisNote per imported event, in the order the chain and the
 * framework adapters read them (analysis_note_compare). The import is not modified and must outlive the notes.
 *
 * Borrowed from the import: the event, the note id (the event's EntityID value), the spelling and the voice id (the
 * target of the event's VoiceReference, NULL when the note names no voice). Built here: span (the event's onset to
 * onset plus duration, left-closed) and midi (the sounding semitone number from the import's parallel
 * PitchNumericValue array). track is the MIDI track chunk the note came from, or the MusicXML part index; voice is
 * ANALYSIS_INDEX_NONE (nothing here assigns voices); voice_confidence is 1 for a voice the source gave and 0 for none.
 *
 * *out is a newly allocated array and *count its length. Free it with import_analysis_notes_destroy, not free: each
 * note's span is allocated here. Fails (false, *out and *count untouched) on a NULL argument, an allocation failure,
 * or an event that makes no valid AnalysisNote (a zero-length span or a pitch outside 0 to 127). An import with no
 * notes gives a NULL array and a count of 0.
 */
bool midi_import_analysis_notes(const MidiImport *import, AnalysisNote **out, size_t *count);
bool musicxml_import_analysis_notes(const MusicXmlImport *import, AnalysisNote **out, size_t *count);

/* Destroys every span and frees the array. NULL is a no-op. */
void import_analysis_notes_destroy(AnalysisNote *notes, size_t count);

#endif
