#include "pipeline/chain_input.h"

#include <stdlib.h>
#include <string.h>

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

static bool fill_note(AnalysisNote *note, const ImportedNotes *notes, size_t i) {
    const NoteEvent *event = notes->events[i];
    TimeSpan *span = NULL;
    if (!build_span(event, &span)) {
        return false;
    }
    const VoiceReference *voice = note_event_optional_voice(event);
    note->note_id = entity_id_value(note_event_id(event));
    note->event = event;
    note->span = span;
    note->midi = (int)pitch_numeric_value_position_numerator(notes->pitch_values[i]);
    note->spelling = notes->source.spelling_notated ? note_event_optional_pitch_spelling(event) : NULL;
    note->voice_id = voice ? entity_id_value(voice_reference_target_id(voice)) : NULL;
    note->track = notes->streams ? notes->streams[i] : ANALYSIS_INDEX_NONE;
    note->voice = ANALYSIS_INDEX_NONE;
    note->voice_confidence = voice ? 1.0 : 0.0;
    if (!analysis_note_valid(note)) {
        time_span_destroy(span);
        return false;
    }
    return true;
}

/* An AnalysisNote with its ImportedNotes index; the note comes first so analysis_note_compare reads it directly. */
typedef struct {
    AnalysisNote note;
    size_t index;
} IndexedNote;

static int compare_indexed(const void *a, const void *b) {
    return analysis_note_compare(a, b);
}

static int compare_slots(const void *a, const void *b) {
    return strcmp(((const ChainInputSlot *)a)->id, ((const ChainInputSlot *)b)->id);
}

static int find_slot(const void *key, const void *entry) {
    return strcmp((const char *)key, ((const ChainInputSlot *)entry)->id);
}

static bool build_key_events(const ImportedNotes *notes, ChainInput *out) {
    size_t count = notes->key_signature_events.count;
    out->key_events = calloc(count > 0 ? count : 1, sizeof(AnalysisKeyEvent));
    if (!out->key_events) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        const MidiKeySignatureEvent *event = &notes->key_signature_events.events[i];
        out->key_events[i].time = event->position;
        if (!analysis_key_from_fifths(event->sharps_flats, KEY_STRUCTURE_MODE_MAJOR, &out->key_events[i].key)) {
            return false;
        }
    }
    out->key_event_count = count;
    return true;
}

bool chain_input_build(const ImportedNotes *notes, ChainInput *out) {
    memset(out, 0, sizeof(*out));
    out->set.map = notes->meter.meter_map;
    if (notes->count > 0) {
        IndexedNote *indexed = calloc(notes->count, sizeof(IndexedNote));
        out->notes = calloc(notes->count, sizeof(AnalysisNote));
        out->order = malloc(sizeof(size_t) * notes->count);
        out->by_id = malloc(sizeof(ChainInputSlot) * notes->count);
        if (!indexed || !out->notes || !out->order || !out->by_id) {
            free(indexed);
            chain_input_destroy(out);
            return false;
        }
        for (size_t i = 0; i < notes->count; i++) {
            indexed[i].index = i;
            if (!fill_note(&indexed[i].note, notes, i)) {
                for (size_t j = 0; j < i; j++) {
                    time_span_destroy((TimeSpan *)indexed[j].note.span);
                }
                free(indexed);
                chain_input_destroy(out);
                return false;
            }
        }
        qsort(indexed, notes->count, sizeof(IndexedNote), compare_indexed);
        out->count = notes->count;
        out->set.notes = out->notes;
        out->set.count = notes->count;
        for (size_t p = 0; p < notes->count; p++) {
            out->notes[p] = indexed[p].note;
            out->order[p] = indexed[p].index;
            out->by_id[p] = (ChainInputSlot){out->notes[p].note_id, indexed[p].index};
        }
        free(indexed);
        qsort(out->by_id, notes->count, sizeof(ChainInputSlot), compare_slots);
    }
    if (!build_key_events(notes, out)) {
        chain_input_destroy(out);
        return false;
    }
    return true;
}

void chain_input_destroy(ChainInput *input) {
    for (size_t i = 0; input->notes && i < input->count; i++) {
        time_span_destroy((TimeSpan *)input->notes[i].span);
    }
    free(input->notes);
    free(input->order);
    free(input->by_id);
    free(input->key_events);
    memset(input, 0, sizeof(*input));
}

size_t chain_input_import_index(const ChainInput *input, const char *note_id) {
    const ChainInputSlot *slot = bsearch(note_id, input->by_id, input->count, sizeof(ChainInputSlot), find_slot);
    return slot ? slot->index : (size_t)-1;
}
