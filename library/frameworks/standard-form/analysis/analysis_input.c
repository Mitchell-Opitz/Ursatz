#include "analysis_input.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_time.h"

#define DEFAULT_LEAP_SEMITONES 7
#define DEFAULT_LOOKAHEAD 4
#define DEFAULT_MAX_TAIL 8
#define DEFAULT_SIMILARITY_MIN 0.75
#define DEFAULT_NO_SIGNAL_UNCERTAINTY 0.5

StandardFormConfig standard_form_config_default(void) {
    StandardFormConfig config = {0, DEFAULT_LEAP_SEMITONES, DEFAULT_LOOKAHEAD, DEFAULT_MAX_TAIL, DEFAULT_SIMILARITY_MIN,
                                 DEFAULT_NO_SIGNAL_UNCERTAINTY, 3, 8, 1, false, NULL, NULL};
    return config;
}

bool standard_form_config_valid(const StandardFormConfig *config) {
    return config != NULL && config->discontinuity_leap_semitones > 0 && isfinite(config->similarity_min) &&
           config->similarity_min > 0.0 && config->similarity_min <= 1.0 && isfinite(config->no_signal_uncertainty) &&
           config->no_signal_uncertainty >= 0.0 && config->no_signal_uncertainty <= 1.0 && config->motive_min_notes >= 3 &&
           config->motive_max_notes >= config->motive_min_notes && config->motive_max_notes <= 32;
}

void standard_form_input_init(StandardFormInput *input, const MeterMap *map) {
    if (input == NULL) {
        return;
    }
    memset(input, 0, sizeof(*input));
    input->map = map;
}

void standard_form_input_destroy(StandardFormInput *input) {
    if (input == NULL) {
        return;
    }
    free(input->cadences);
    free(input->notes);
    free(input->segments);
    free(input->bass_notes);
    const MeterMap *map = input->map;
    memset(input, 0, sizeof(*input));
    input->map = map;
}

static bool copy_id(char *to, size_t size, const char *from) {
    if (from == NULL || from[0] == '\0' || strlen(from) >= size) {
        return false;
    }
    memcpy(to, from, strlen(from) + 1);
    return true;
}

static bool grow(void **items, size_t *capacity, size_t count, size_t item_size) {
    if (count < *capacity) {
        return true;
    }
    size_t next = *capacity == 0 ? 16 : *capacity * 2;
    void *grown = realloc(*items, next * item_size);
    if (grown == NULL) {
        return false;
    }
    *items = grown;
    *capacity = next;
    return true;
}

StandardFormAnalysisStatus standard_form_input_add_cadence(StandardFormInput *input, const char *id,
                                                           const TimeSpan *arrival, CadenceType type, int strength_rank,
                                                           bool post_cadential, double uncertainty) {
    if (input == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if (arrival == NULL || !cadence_type_valid(type) || !(uncertainty >= 0.0 && uncertainty <= 1.0) ||
        !standard_form_span_q(arrival, NULL, NULL)) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    if (!grow((void **)&input->cadences, &input->cadence_capacity, input->cadence_count, sizeof(StandardFormCadence))) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    StandardFormCadence *cadence = &input->cadences[input->cadence_count];
    memset(cadence, 0, sizeof(*cadence));
    if (!copy_id(cadence->id, sizeof(cadence->id), id)) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    cadence->arrival = arrival;
    cadence->type = type;
    cadence->strength_rank = strength_rank;
    cadence->post_cadential = post_cadential;
    cadence->uncertainty = uncertainty;
    input->cadence_count++;
    input->finished = false;
    return STANDARD_FORM_ANALYSIS_OK;
}

StandardFormAnalysisStatus standard_form_input_add_note(StandardFormInput *input, const char *note_id,
                                                        const NoteEvent *event, const TimeSpan *span,
                                                        const MetricalPosition *position, long semitone) {
    if (input == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    StandardFormQ start;
    if (event == NULL || span == NULL || !standard_form_span_q(span, &start, NULL)) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    if (input->note_count > 0) {
        StandardFormQ previous;
        standard_form_span_q(input->notes[input->note_count - 1].span, &previous, NULL);
        if (standard_form_q_compare(start, previous) < 0) {
            return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
        }
    }
    if (!grow((void **)&input->notes, &input->note_capacity, input->note_count, sizeof(StandardFormNote))) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    StandardFormNote *note = &input->notes[input->note_count];
    memset(note, 0, sizeof(*note));
    if (!copy_id(note->note_id, sizeof(note->note_id), note_id)) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    note->event = event;
    note->span = span;
    note->position = position;
    note->semitone = semitone;
    input->note_count++;
    input->finished = false;
    return STANDARD_FORM_ANALYSIS_OK;
}

static StandardFormAnalysisStatus add_to_line(StandardFormNote **items, size_t *count, size_t *capacity, const char *note_id,
                                              const NoteEvent *event, const TimeSpan *span,
                                              const MetricalPosition *position, long semitone) {
    StandardFormQ start;
    if (event == NULL || span == NULL || !standard_form_span_q(span, &start, NULL)) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    if (*count > 0) {
        StandardFormQ previous;
        standard_form_span_q((*items)[*count - 1].span, &previous, NULL);
        if (standard_form_q_compare(start, previous) < 0) {
            return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
        }
    }
    if (!grow((void **)items, capacity, *count, sizeof(StandardFormNote))) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    StandardFormNote *note = &(*items)[*count];
    memset(note, 0, sizeof(*note));
    if (!copy_id(note->note_id, sizeof(note->note_id), note_id)) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    note->event = event;
    note->span = span;
    note->position = position;
    note->semitone = semitone;
    (*count)++;
    return STANDARD_FORM_ANALYSIS_OK;
}

StandardFormAnalysisStatus standard_form_input_add_bass_note(StandardFormInput *input, const char *note_id,
                                                             const NoteEvent *event, const TimeSpan *span,
                                                             const MetricalPosition *position, long semitone) {
    if (input == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    input->finished = false;
    return add_to_line(&input->bass_notes, &input->bass_count, &input->bass_capacity, note_id, event, span, position,
                       semitone);
}

StandardFormAnalysisStatus standard_form_input_set_degree(StandardFormInput *input, bool bass, size_t index, int degree,
                                                          int alteration) {
    if (input == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    size_t count = bass ? input->bass_count : input->note_count;
    StandardFormNote *items = bass ? input->bass_notes : input->notes;
    if (index >= count || degree < 0 || degree > 7 || alteration < -1 || alteration > 1) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    items[index].degree = degree;
    items[index].alteration = alteration;
    return STANDARD_FORM_ANALYSIS_OK;
}

StandardFormAnalysisStatus standard_form_input_add_analysis_note(StandardFormInput *input, const AnalysisNote *note, bool bass) {
    if (input == NULL || note == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if (note->midi == ANALYSIS_PITCH_UNKNOWN) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    return bass ? standard_form_input_add_bass_note(input, note->note_id, note->event, note->span, NULL, note->midi)
                : standard_form_input_add_note(input, note->note_id, note->event, note->span, NULL, note->midi);
}

StandardFormAnalysisStatus standard_form_input_add_segment(StandardFormInput *input, const char *id,
                                                           const TimeSpan *span, int function, int phrase_model_label) {
    if (input == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if (span == NULL || !standard_form_span_q(span, NULL, NULL)) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    if (!grow((void **)&input->segments, &input->segment_capacity, input->segment_count, sizeof(StandardFormSegment))) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    StandardFormSegment *segment = &input->segments[input->segment_count];
    memset(segment, 0, sizeof(*segment));
    if (!copy_id(segment->id, sizeof(segment->id), id)) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    segment->span = span;
    segment->function = function;
    segment->phrase_model_label = phrase_model_label;
    input->segment_count++;
    return STANDARD_FORM_ANALYSIS_OK;
}

StandardFormAnalysisStatus standard_form_input_add_analysis_segment(StandardFormInput *input, const AnalysisSegment *segment,
                                                                    int function, int phrase_model_label) {
    if (input == NULL || segment == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    return standard_form_input_add_segment(input, segment->id, segment->span, function, phrase_model_label);
}

static int compare_cadences(const void *a, const void *b) {
    StandardFormQ left;
    StandardFormQ right;
    standard_form_span_q(((const StandardFormCadence *)a)->arrival, &left, NULL);
    standard_form_span_q(((const StandardFormCadence *)b)->arrival, &right, NULL);
    return standard_form_q_compare(left, right);
}

StandardFormAnalysisStatus standard_form_input_finish(StandardFormInput *input) {
    if (input == NULL || input->map == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if (input->cadence_count > 1) {
        qsort(input->cadences, input->cadence_count, sizeof(StandardFormCadence), compare_cadences);
    }
    for (int line = 0; line < 2; line++) {
        StandardFormNote *items = line == 0 ? input->notes : input->bass_notes;
        size_t count = line == 0 ? input->note_count : input->bass_count;
        for (size_t i = 0; i < count; i++) {
            StandardFormNote *note = &items[i];
            note->has_next = false;
            note->semitones_to_next = 0;
            note->direction_to_next = (int)CONTOUR_DIRECTION_SAME;
            if (i + 1 < count) {
                StandardFormQ end;
                StandardFormQ next_start;
                standard_form_span_q(note->span, NULL, &end);
                standard_form_span_q(items[i + 1].span, &next_start, NULL);
                if (standard_form_q_compare(end, next_start) >= 0) {
                    long delta = items[i + 1].semitone - note->semitone;
                    note->has_next = true;
                    note->semitones_to_next = delta;
                    note->direction_to_next = delta > 0 ? (int)CONTOUR_DIRECTION_UP
                                                        : (delta < 0 ? (int)CONTOUR_DIRECTION_DOWN : (int)CONTOUR_DIRECTION_SAME);
                }
            }
        }
    }
    input->finished = true;
    return STANDARD_FORM_ANALYSIS_OK;
}
