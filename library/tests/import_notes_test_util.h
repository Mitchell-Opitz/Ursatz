#ifndef TEST_IMPORT_NOTES_TEST_UTIL_H
#define TEST_IMPORT_NOTES_TEST_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "model/core/voice_reference.h"
#include "model/identity/entity_id.h"
#include "io/midi_importer.h"
#include "io/musicxml_importer.h"
#include "model/pitch/pitch_numeric_value.h"
#include "analysis/notes/analysis_note.h"
#include "analysis/notes/analysis_note_build.h"
#include "model/time/duration.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"
#include "model/time/time_span.h"

static inline bool import_notes_build_notes(NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                            const size_t *tracks, size_t event_count, AnalysisNote **out,
                                            size_t *count) {
    if (out == NULL || count == NULL) {
        return false;
    }
    AnalysisNoteSource source = {
        .events = (const NoteEvent *const *)events,
        .pitch_values = pitch_values,
        .tracks = tracks,
        .count = event_count,
        .use_spelling = true,
        .map = NULL,
    };
    AnalysisNoteSet set;
    if (!analysis_note_set_build(&source, &set, NULL)) {
        return false;
    }
    *out = (AnalysisNote *)set.notes;
    *count = set.count;
    return true;
}

/*
 * Test helper, neutral notes from imports: one AnalysisNote per imported event, in the order the chain and the
 * framework adapters read them (analysis_note_compare). The import is not modified and must outlive the notes.
 *
 * Borrowed from the import: the event, the note id (the event's EntityID value), the spelling and the voice id (the
 * target of the event's VoiceReference, NULL when the note names no voice). Built here: span (the event's onset to
 * onset plus duration, left-closed) and midi (the sounding semitone number from the import's parallel
 * PitchNumericValue array). track is the MIDI track chunk the note came from, or the MusicXML part index; voice is
 * ANALYSIS_INDEX_NONE (nothing here assigns voices); voice_confidence is 1 for a voice the source gave and 0 for none.
 *
 * *out is a newly allocated array and *count its length. Free it with import_analysis_notes_destroy. Fails (false, *out and *count untouched) on a NULL argument, an allocation failure,
 * or an event that makes no valid AnalysisNote (a zero-length span or a pitch outside 0 to 127). An import with no
 * notes gives a NULL array and a count of 0.
 */
static inline bool midi_import_analysis_notes(const MidiImport *import, AnalysisNote **out, size_t *count) {
    if (import == NULL) {
        return false;
    }
    return import_notes_build_notes(import->events, import->pitch_values, import->note_tracks, import->count, out,
                                    count);
}

static inline bool musicxml_import_analysis_notes(const MusicXmlImport *import, AnalysisNote **out, size_t *count) {
    if (import == NULL) {
        return false;
    }
    return import_notes_build_notes(import->events, import->pitch_values, import->part_index, import->count, out,
                                    count);
}

static inline void import_analysis_notes_destroy(AnalysisNote *notes, size_t count) {
    if (notes == NULL) {
        return;
    }
    (void)count;
    free(notes);
}

#endif
