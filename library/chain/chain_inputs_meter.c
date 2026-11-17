#include "chain/chain_inputs_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- the meter input --- */

void chain_meter_input_destroy(ChainMeterInput *input) {
    if (input == NULL) {
        return;
    }
    standard_meter_input_destroy(&input->input);
    free(input->notes);
    memset(input, 0, sizeof(*input));
}

static int compare_ids(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* The ids of the notes the line takes, sorted. */
static const char **line_ids(const OuterVoiceEntry *entries, size_t count, size_t *out_count) {
    const char **ids = malloc((count == 0 ? 1 : count) * sizeof(char *));
    if (ids == NULL) {
        return NULL;
    }
    size_t n = 0;
    for (size_t s = 0; s < count; s++) {
        if (entries[s].present && entries[s].note_id != NULL) {
            ids[n++] = entity_id_value(entries[s].note_id);
        }
    }
    qsort(ids, n, sizeof(char *), compare_ids);
    *out_count = n;
    return ids;
}

static bool in_ids(const char **ids, size_t count, const char *id) {
    return bsearch(&id, ids, count, sizeof(char *), compare_ids) != NULL;
}

static StandardMeterExpansionKind meter_expansion_of(int kind) {
    switch (kind) {
    case STANDARD_FORM_EXPANSION_INTERNAL:
        return STANDARD_METER_EXPANSION_INTERNAL;
    case STANDARD_FORM_EXPANSION_EXTERNAL:
        return STANDARD_METER_EXPANSION_EXTERNAL;
    default:
        return STANDARD_METER_EXPANSION_CONTRACTION;
    }
}

ChainInputStatus chain_build_meter_input(const AnalysisNoteSet *notes, const OuterVoiceLines *lines, const ChainSegments *segments,
                                         const char *const *chord_labels, int first_measure, int last_measure,
                                         const StandardFormInput *form, const StandardFormPhrases *phrases,
                                         const StandardFormExpansions *expansions, ChainMeterInput *out) {
    if (notes == NULL || lines == NULL || segments == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (!analysis_note_set_valid(notes) || notes->count == 0) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    ChainMeterInput build;
    memset(&build, 0, sizeof(build));
    size_t soprano_count = 0;
    size_t bass_count = 0;
    const char **soprano = line_ids(lines->soprano, lines->count, &soprano_count);
    const char **bass = line_ids(lines->bass, lines->count, &bass_count);
    ChainInputStatus status = soprano != NULL && bass != NULL ? CHAIN_INPUT_OK : CHAIN_INPUT_ERR_ALLOC;
    build.notes = status == CHAIN_INPUT_OK ? (AnalysisNote *)calloc(notes->count, sizeof(AnalysisNote)) : NULL;
    if (status == CHAIN_INPUT_OK && build.notes == NULL) {
        status = CHAIN_INPUT_ERR_ALLOC;
    }
    for (size_t i = 0; i < notes->count && status == CHAIN_INPUT_OK; i++) {
        AnalysisNote note = notes->notes[i];
        int measure = chain_measure_of(notes->map, &note.span.start);
        if (measure < first_measure || measure > last_measure) {
            continue;
        }
        note.voice = in_ids(soprano, soprano_count, note.note_id) ? 0 : (in_ids(bass, bass_count, note.note_id) ? 1 : 2);
        if (note.midi == ANALYSIS_PITCH_UNKNOWN) {
            status = CHAIN_INPUT_ERR_BAD_NOTES;
            break;
        }
        build.notes[build.note_count++] = note;
    }
    free(soprano);
    free(bass);
    /* The library checks what the input must hold (ids, spans) and borrows the notes. */
    if (status == CHAIN_INPUT_OK &&
        standard_meter_input_init(&build.input, notes->map, build.notes, build.note_count) != STANDARD_METER_ANALYSIS_OK) {
        status = CHAIN_INPUT_ERR_BAD_NOTES;
    }
    for (size_t s = 1; chord_labels != NULL && s < segments->count && status == CHAIN_INPUT_OK; s++) {
        if (chord_labels[s] == NULL || chord_labels[s - 1] == NULL || strcmp(chord_labels[s], chord_labels[s - 1]) == 0) {
            continue;
        }
        const MusicalTime *start = time_span_start(sonority_time_window(segments->sonorities.items[s].sonority));
        int measure = chain_measure_of(notes->map, start);
        if (measure < first_measure || measure > last_measure) {
            continue;
        }
        char id[48];
        snprintf(id, sizeof(id), "chord-change-%zu", s);
        if (standard_meter_input_add_chord_change(&build.input, id, start) != STANDARD_METER_ANALYSIS_OK) {
            status = CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    for (size_t i = 0; phrases != NULL && form != NULL && i < phrases->count && status == CHAIN_INPUT_OK; i++) {
        const StandardFormPhrase *phrase = &phrases->items[i];
        const TimeSpan *arrival = NULL;
        if (phrase->claim.cadence_index >= 0) {
            arrival = &form->cadences[phrase->claim.cadence_index].arrival;
        }
        StandardMeterExpansionKind expansion = STANDARD_METER_EXPANSION_NONE;
        int measures = 0;
        for (size_t e = 0; expansions != NULL && e < expansions->count; e++) {
            if (expansions->items[e].claim.phrase != phrase->claim.index) {
                continue;
            }
            expansion = meter_expansion_of(expansions->items[e].claim.kind);
            measures = expansions->items[e].claim.measures_changed;
            break;
        }
        char id[32];
        snprintf(id, sizeof(id), "phrase-%zu", i);
        if (standard_meter_input_add_phrase(&build.input, id, &phrase->span, arrival, expansion, measures, false) !=
            STANDARD_METER_ANALYSIS_OK) {
            status = CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    if (status != CHAIN_INPUT_OK) {
        chain_meter_input_destroy(&build);
        return status;
    }
    *out = build;
    return CHAIN_INPUT_OK;
}
