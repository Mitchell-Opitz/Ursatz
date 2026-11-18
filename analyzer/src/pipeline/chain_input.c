#include "pipeline/chain_input.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/notes/analysis_note_build.h"
#include <theory/tonality/common_practice_materials.h>

static int compare_slots(const void *a, const void *b) {
    return strcmp(((const ChainInputSlot *)a)->id, ((const ChainInputSlot *)b)->id);
}

static int find_slot(const void *key, const void *entry) {
    return strcmp((const char *)key, ((const ChainInputSlot *)entry)->id);
}

static bool build_key_events(const ImportedNotes *notes, const CommonPracticeMaterials *materials, ChainInput *out) {
    size_t count = notes->key_signature_events.count;
    out->key_events = calloc(count > 0 ? count : 1, sizeof(AnalysisKeyEvent));
    if (!out->key_events) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        const MidiKeySignatureEvent *event = &notes->key_signature_events.events[i];
        out->key_events[i].time = event->position;
        /* The file's mode hint never reached the chain: every key is the major one. */
        if (!analysis_key_from_fifths(event->sharps_flats, materials->major,
                                               &out->key_events[i].key)) {
            return false;
        }
    }
    out->key_event_count = count;
    return true;
}

bool chain_input_build(const ImportedNotes *notes, const CommonPracticeMaterials *materials, ChainInput *out) {
    memset(out, 0, sizeof(*out));
    AnalysisNoteSource source = {
        .events = (const NoteEvent *const *)notes->events,
        .pitch_values = notes->pitch_values,
        .tracks = notes->streams,
        .count = notes->count,
        .use_spelling = notes->source.spelling_notated,
        .map = notes->meter.meter_map,
    };
    if (!analysis_note_set_build(&source, &out->set, &out->order)) {
        memset(out, 0, sizeof(*out));
        return false;
    }
    out->count = notes->count;
    if (notes->count > 0) {
        out->by_id = malloc(sizeof(ChainInputSlot) * notes->count);
        if (!out->by_id) {
            chain_input_destroy(out);
            return false;
        }
        for (size_t p = 0; p < notes->count; p++) {
            out->by_id[p] = (ChainInputSlot){out->set.notes[p].note_id, out->order[p]};
        }
        qsort(out->by_id, notes->count, sizeof(ChainInputSlot), compare_slots);
    }
    if (!build_key_events(notes, materials, out)) {
        chain_input_destroy(out);
        return false;
    }
    return true;
}

void chain_input_destroy(ChainInput *input) {
    analysis_note_set_built_destroy(&input->set, input->order);
    free(input->by_id);
    free(input->key_events);
    memset(input, 0, sizeof(*input));
}

size_t chain_input_import_index(const ChainInput *input, const char *note_id) {
    const ChainInputSlot *slot = bsearch(note_id, input->by_id, input->count, sizeof(ChainInputSlot), find_slot);
    return slot ? slot->index : (size_t)-1;
}
