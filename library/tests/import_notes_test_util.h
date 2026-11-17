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
#include "model/time/duration.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"
#include "model/time/time_span.h"

static inline void import_analysis_notes_destroy(AnalysisNote *notes, size_t count);

static inline bool import_notes_build_span(const NoteEvent *event, TimeSpan **out) {
    const MusicalTime *start = temporal_anchor_position(note_event_temporal_anchor(event));
    MusicalTime *end = NULL;
    if (musical_time_add(start, note_event_duration(event), &end) != MUSICAL_TIME_OK) {
        return false;
    }
    TimeSpanResult result = time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, out);
    musical_time_destroy(end);
    return result == TIME_SPAN_OK;
}

static inline bool import_notes_fill_note(AnalysisNote *note, const NoteEvent *event, const PitchNumericValue *pitch,
                                          size_t track) {
    TimeSpan *span = NULL;
    if (event == NULL || pitch == NULL || !import_notes_build_span(event, &span)) {
        return false;
    }
    const VoiceReference *voice = note_event_optional_voice(event);
    note->note_id = entity_id_value(note_event_id(event));
    note->event = event;
    note->span = span;
    note->midi = (int)pitch_numeric_value_position_numerator(pitch);
    note->spelling = note_event_optional_pitch_spelling(event);
    note->voice_id = voice != NULL ? entity_id_value(voice_reference_target_id(voice)) : NULL;
    note->track = track;
    note->voice = ANALYSIS_INDEX_NONE;
    note->voice_confidence = voice != NULL ? 1.0 : 0.0;
    if (!analysis_note_valid(note)) {
        time_span_destroy(span);
        return false;
    }
    return true;
}

static inline bool import_notes_build_notes(NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                            const size_t *tracks, size_t event_count, AnalysisNote **out,
                                            size_t *count) {
    if (out == NULL || count == NULL || (event_count > 0 && (events == NULL || pitch_values == NULL))) {
        return false;
    }
    if (event_count == 0) {
        *out = NULL;
        *count = 0;
        return true;
    }
    AnalysisNote *notes = calloc(event_count, sizeof(AnalysisNote));
    if (notes == NULL) {
        return false;
    }
    for (size_t i = 0; i < event_count; i++) {
        size_t track = tracks != NULL ? tracks[i] : ANALYSIS_INDEX_NONE;
        if (!import_notes_fill_note(&notes[i], events[i], pitch_values[i], track)) {
            import_analysis_notes_destroy(notes, i);
            return false;
        }
    }
    qsort(notes, event_count, sizeof(AnalysisNote), analysis_note_compare);
    *out = notes;
    *count = event_count;
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
 * *out is a newly allocated array and *count its length. Free it with import_analysis_notes_destroy, not free: each
 * note's span is allocated here. Fails (false, *out and *count untouched) on a NULL argument, an allocation failure,
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
    for (size_t i = 0; i < count; i++) {
        time_span_destroy((TimeSpan *)notes[i].span);
    }
    free(notes);
}

#endif
