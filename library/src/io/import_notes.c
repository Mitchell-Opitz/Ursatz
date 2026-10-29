#include "io/import_notes.h"

#include <stdlib.h>

#include "core/voice_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_numeric_value.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"

static bool build_span(const NoteEvent *event, TimeSpan **out) {
    const MusicalTime *start = temporal_anchor_position(note_event_temporal_anchor(event));
    MusicalTime *end = NULL;
    if (musical_time_add(start, note_event_duration(event), &end) != MUSICAL_TIME_OK) {
        return false;
    }
    TimeSpanResult result = time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, out);
    musical_time_destroy(end);
    return result == TIME_SPAN_OK;
}

static bool fill_note(AnalysisNote *note, const NoteEvent *event, const PitchNumericValue *pitch, size_t track) {
    TimeSpan *span = NULL;
    if (event == NULL || pitch == NULL || !build_span(event, &span)) {
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

static bool build_notes(NoteEvent *const *events, PitchNumericValue *const *pitch_values, const size_t *tracks,
                        size_t event_count, AnalysisNote **out, size_t *count) {
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
        if (!fill_note(&notes[i], events[i], pitch_values[i], tracks != NULL ? tracks[i] : ANALYSIS_INDEX_NONE)) {
            import_analysis_notes_destroy(notes, i);
            return false;
        }
    }
    qsort(notes, event_count, sizeof(AnalysisNote), analysis_note_compare);
    *out = notes;
    *count = event_count;
    return true;
}

bool midi_import_analysis_notes(const MidiImport *import, AnalysisNote **out, size_t *count) {
    if (import == NULL) {
        return false;
    }
    return build_notes(import->events, import->pitch_values, import->note_tracks, import->count, out, count);
}

bool musicxml_import_analysis_notes(const MusicXmlImport *import, AnalysisNote **out, size_t *count) {
    if (import == NULL) {
        return false;
    }
    return build_notes(import->events, import->pitch_values, import->part_index, import->count, out, count);
}

void import_analysis_notes_destroy(AnalysisNote *notes, size_t count) {
    if (notes == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        time_span_destroy((TimeSpan *)notes[i].span);
    }
    free(notes);
}
