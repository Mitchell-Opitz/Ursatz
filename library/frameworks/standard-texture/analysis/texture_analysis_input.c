#include "texture_analysis_input.h"

#include <stdlib.h>
#include <string.h>

StandardTextureConfig standard_texture_config_default(void) {
    StandardTextureConfig config = {32, 256, 64, 0.75, 0.6, 0.5, 1.5, 3.0};
    return config;
}

bool standard_texture_config_valid(const StandardTextureConfig *config) {
    return config != NULL && config->onset_grid_denominator >= 1 &&
           config->onset_tolerance_denominator >= 2 * config->onset_grid_denominator &&
           config->release_tolerance_denominator >= 2 * config->onset_grid_denominator &&
           config->rhythmic_unison_synchrony > 0.0 && config->rhythmic_unison_synchrony <= 1.0 &&
           config->independence_min > 0.0 && config->independence_min <= 1.0 && config->independence_max_synchrony > 0.0 &&
           config->independence_max_synchrony <= config->rhythmic_unison_synchrony && config->prominence_min > 1.0 &&
           config->prominence_gap_semitones >= 0.0;
}

StandardTextureAnalysisStatus standard_texture_input_init(StandardTextureInput *input, const MeterMap *map) {
    if (input == NULL || map == NULL) {
        return STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    memset(input, 0, sizeof(*input));
    input->map = map;
    return STANDARD_TEXTURE_ANALYSIS_OK;
}

static bool valid_span(const TimeSpan *span) {
    if (span == NULL || time_span_start(span) == NULL || time_span_end(span) == NULL) {
        return false;
    }
    const MusicalTime *start = time_span_start(span);
    const MusicalTime *end = time_span_end(span);
    return (long long)musical_time_numerator(end) * (long long)musical_time_denominator(start) >
           (long long)musical_time_numerator(start) * (long long)musical_time_denominator(end);
}

StandardTextureAnalysisStatus standard_texture_input_add_note(StandardTextureInput *input, const char *id, const TimeSpan *span,
                                                              long pitch, size_t track, size_t voice, double voice_confidence) {
    if (input == NULL) {
        return STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if (id == NULL || id[0] == '\0' || strlen(id) >= STANDARD_TEXTURE_ID_MAX || !valid_span(span) ||
        !(voice_confidence >= 0.0 && voice_confidence <= 1.0)) {
        return STANDARD_TEXTURE_ANALYSIS_ERR_INVALID;
    }
    if (input->note_count == input->note_capacity) {
        size_t next = input->note_capacity == 0 ? 256 : input->note_capacity * 2;
        StandardTextureNote *grown = (StandardTextureNote *)realloc(input->notes, next * sizeof(StandardTextureNote));
        if (grown == NULL) {
            return STANDARD_TEXTURE_ANALYSIS_ERR_ALLOC;
        }
        input->notes = grown;
        input->note_capacity = next;
    }
    StandardTextureNote *note = &input->notes[input->note_count++];
    memset(note, 0, sizeof(*note));
    strcpy(note->id, id);
    note->span = span;
    note->pitch = pitch;
    note->track = track;
    note->voice = voice;
    note->voice_confidence = voice_confidence;
    return STANDARD_TEXTURE_ANALYSIS_OK;
}

void standard_texture_input_destroy(StandardTextureInput *input) {
    if (input == NULL) {
        return;
    }
    free(input->notes);
    memset(input, 0, sizeof(*input));
}
