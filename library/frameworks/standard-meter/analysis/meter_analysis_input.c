#include "meter_analysis_input.h"

#include <stdlib.h>
#include <string.h>

StandardMeterConfig standard_meter_config_default(void) {
    StandardMeterConfig config = {false, 2, 32, 256, 64};
    return config;
}

bool standard_meter_config_valid(const StandardMeterConfig *config) {
    return config != NULL && config->hemiola_min_evidence >= 1 && config->hemiola_min_evidence <= 3 &&
           config->onset_grid_denominator >= 1 && config->onset_tolerance_denominator >= 2 * config->onset_grid_denominator &&
           config->release_tolerance_denominator >= 2 * config->onset_grid_denominator;
}

StandardMeterAnalysisStatus standard_meter_input_init(StandardMeterInput *input, const MeterMap *map) {
    if (input == NULL || map == NULL) {
        return STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    memset(input, 0, sizeof(*input));
    input->map = map;
    return STANDARD_METER_ANALYSIS_OK;
}

static bool valid_id(const char *id) {
    return id != NULL && id[0] != '\0' && strlen(id) < STANDARD_METER_ID_MAX;
}

static bool grow(void **items, size_t *capacity, size_t count, size_t size) {
    if (count < *capacity) {
        return true;
    }
    size_t next = *capacity == 0 ? 64 : *capacity * 2;
    void *grown = realloc(*items, next * size);
    if (grown == NULL) {
        return false;
    }
    *items = grown;
    *capacity = next;
    return true;
}

StandardMeterAnalysisStatus standard_meter_input_add_note(StandardMeterInput *input, const char *id, const TimeSpan *span,
                                                          long pitch, int line) {
    if (input == NULL) {
        return STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if (!valid_id(id) || !time_span_is_valid(span)) {
        return STANDARD_METER_ANALYSIS_ERR_INVALID;
    }
    if (!grow((void **)&input->notes, &input->note_capacity, input->note_count, sizeof(StandardMeterNote))) {
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    StandardMeterNote *note = &input->notes[input->note_count++];
    memset(note, 0, sizeof(*note));
    strcpy(note->id, id);
    note->span = span;
    note->pitch = pitch;
    note->line = line;
    return STANDARD_METER_ANALYSIS_OK;
}

StandardMeterAnalysisStatus standard_meter_input_add_phrase(StandardMeterInput *input, const char *id, const TimeSpan *span,
                                                            const TimeSpan *cadence_arrival,
                                                            StandardMeterExpansionKind expansion, int expansion_measures,
                                                            bool elided_with_next) {
    if (input == NULL) {
        return STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if (!valid_id(id) || !time_span_is_valid(span) || (cadence_arrival != NULL && !time_span_is_valid(cadence_arrival)) ||
        expansion < STANDARD_METER_EXPANSION_NONE || expansion > STANDARD_METER_EXPANSION_CONTRACTION ||
        expansion_measures < 0 || (expansion == STANDARD_METER_EXPANSION_NONE && expansion_measures != 0)) {
        return STANDARD_METER_ANALYSIS_ERR_INVALID;
    }
    if (!grow((void **)&input->phrases, &input->phrase_capacity, input->phrase_count, sizeof(StandardMeterPhrase))) {
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    StandardMeterPhrase *phrase = &input->phrases[input->phrase_count++];
    memset(phrase, 0, sizeof(*phrase));
    strcpy(phrase->id, id);
    phrase->span = span;
    phrase->cadence_arrival = cadence_arrival;
    phrase->expansion = expansion;
    phrase->expansion_measures = expansion_measures;
    phrase->elided_with_next = elided_with_next;
    return STANDARD_METER_ANALYSIS_OK;
}

StandardMeterAnalysisStatus standard_meter_input_add_chord_change(StandardMeterInput *input, const char *id,
                                                                  const MusicalTime *time) {
    if (input == NULL) {
        return STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if (!valid_id(id) || time == NULL) {
        return STANDARD_METER_ANALYSIS_ERR_INVALID;
    }
    if (!grow((void **)&input->chord_changes, &input->chord_change_capacity, input->chord_change_count,
              sizeof(StandardMeterChordChange))) {
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    StandardMeterChordChange *change = &input->chord_changes[input->chord_change_count++];
    memset(change, 0, sizeof(*change));
    strcpy(change->id, id);
    change->time = time;
    return STANDARD_METER_ANALYSIS_OK;
}

void standard_meter_input_destroy(StandardMeterInput *input) {
    if (input == NULL) {
        return;
    }
    free(input->notes);
    free(input->phrases);
    free(input->chord_changes);
    memset(input, 0, sizeof(*input));
}
