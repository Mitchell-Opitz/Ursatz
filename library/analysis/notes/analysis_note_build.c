#include "analysis/notes/analysis_note_build.h"

#include <stdlib.h>
#include <string.h>

#include "model/core/voice_reference.h"
#include "model/identity/entity_id.h"
#include "model/time/duration.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"

static bool build_span(const NoteEvent *event, TimeSpan *out) {
    MusicalTime start = *temporal_anchor_position(note_event_temporal_anchor(event));
    MusicalTime end;
    if (musical_time_plus(start, *note_event_duration(event), &end) != MUSICAL_TIME_OK) {
        return false;
    }
    return time_span_make(start, end, out) == TIME_SPAN_OK;
}

static bool fill_note(AnalysisNote *note, const AnalysisNoteSource *source, size_t i) {
    const NoteEvent *event = source->events[i];
    const PitchNumericValue *pitch = source->pitch_values[i];
    TimeSpan span;
    if (event == NULL || pitch == NULL || !build_span(event, &span)) {
        return false;
    }
    const VoiceReference *voice = note_event_optional_voice(event);
    note->note_id = entity_id_value(note_event_id(event));
    note->event = event;
    note->span = span;
    note->midi = (int)pitch_numeric_value_position_numerator(pitch);
    note->spelling = source->use_spelling ? note_event_optional_pitch_spelling(event) : NULL;
    note->voice_id = voice != NULL ? entity_id_value(voice_reference_target_id(voice)) : NULL;
    note->track = source->tracks != NULL ? source->tracks[i] : ANALYSIS_INDEX_NONE;
    note->voice = ANALYSIS_INDEX_NONE;
    note->voice_confidence = voice != NULL ? 1.0 : 0.0;
    return analysis_note_valid(note);
}

/* An AnalysisNote with its source index; the note comes first so analysis_note_compare reads it directly. */
typedef struct {
    AnalysisNote note;
    size_t index;
} IndexedNote;

static int compare_indexed(const void *a, const void *b) {
    return analysis_note_compare(a, b);
}

bool analysis_note_set_build(const AnalysisNoteSource *source, AnalysisNoteSet *out, size_t **order) {
    if (source == NULL || out == NULL || (source->count > 0 && (source->events == NULL || source->pitch_values == NULL))) {
        return false;
    }
    if (source->count == 0) {
        out->notes = NULL;
        out->count = 0;
        out->map = source->map;
        if (order != NULL) {
            *order = NULL;
        }
        return true;
    }
    IndexedNote *indexed = calloc(source->count, sizeof(IndexedNote));
    AnalysisNote *notes = calloc(source->count, sizeof(AnalysisNote));
    size_t *sorted_order = order != NULL ? malloc(sizeof(size_t) * source->count) : NULL;
    if (indexed == NULL || notes == NULL || (order != NULL && sorted_order == NULL)) {
        free(indexed);
        free(notes);
        free(sorted_order);
        return false;
    }
    for (size_t i = 0; i < source->count; i++) {
        indexed[i].index = i;
        if (!fill_note(&indexed[i].note, source, i)) {
            free(indexed);
            free(notes);
            free(sorted_order);
            return false;
        }
    }
    qsort(indexed, source->count, sizeof(IndexedNote), compare_indexed);
    for (size_t p = 0; p < source->count; p++) {
        notes[p] = indexed[p].note;
        if (sorted_order != NULL) {
            sorted_order[p] = indexed[p].index;
        }
    }
    free(indexed);
    out->notes = notes;
    out->count = source->count;
    out->map = source->map;
    if (order != NULL) {
        *order = sorted_order;
    }
    return true;
}

void analysis_note_set_built_destroy(AnalysisNoteSet *set, size_t *order) {
    if (set != NULL) {
        free((void *)set->notes);
        memset(set, 0, sizeof(*set));
    }
    free(order);
}
