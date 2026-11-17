#include "analysis/form/analysis_input.h"
#include "analysis/form/analysis_input_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "model/core/rational.h"
#include "model/time/score_time.h"

#define DEFAULT_LEAP_SEMITONES 7
#define DEFAULT_LOOKAHEAD 4
#define DEFAULT_MAX_TAIL 8
#define DEFAULT_SIMILARITY_MIN 0.75
#define DEFAULT_NO_SIGNAL_UNCERTAINTY 0.5

StandardFormConfig standard_form_config_default(void) {
    StandardFormConfig config = {0, DEFAULT_LEAP_SEMITONES, DEFAULT_LOOKAHEAD, DEFAULT_MAX_TAIL, DEFAULT_SIMILARITY_MIN,
                                 DEFAULT_NO_SIGNAL_UNCERTAINTY, 3, 8, 1, false, NULL, NULL, NULL};
    return config;
}

bool standard_form_config_valid(const StandardFormConfig *config) {
    return config != NULL && config->discontinuity_leap_semitones > 0 && isfinite(config->similarity_min) &&
           config->similarity_min > 0.0 && config->similarity_min <= 1.0 && isfinite(config->no_signal_uncertainty) &&
           config->no_signal_uncertainty >= 0.0 && config->no_signal_uncertainty <= 1.0 && config->motive_min_notes >= 3 &&
           config->motive_max_notes >= config->motive_min_notes && config->motive_max_notes <= 32;
}

void standard_form_input_init(StandardFormInput *input, const MeterMap *map, const AnalysisNote *notes, size_t note_count) {
    if (input == NULL) {
        return;
    }
    memset(input, 0, sizeof(*input));
    input->map = map;
    input->notes = notes;
    input->note_count = note_count;
}

void standard_form_input_destroy(StandardFormInput *input) {
    if (input == NULL) {
        return;
    }
    free(input->cadences);
    memset(input, 0, sizeof(*input));
}

static bool copy_id_valid(const char *id) {
    return id != NULL && id[0] != '\0' && strlen(id) < 64;
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
        !score_time_span(arrival, NULL, NULL)) {
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

static int compare_cadences(const void *a, const void *b) {
    Rational left;
    Rational right;
    score_time_span(((const StandardFormCadence *)a)->arrival, &left, NULL);
    score_time_span(((const StandardFormCadence *)b)->arrival, &right, NULL);
    return rational_cmp(left, right);
}

/* What a note of a line must hold, and its start not before the one before it. */
static bool line_valid(const AnalysisNote *notes, size_t count) {
    if (count > 0 && notes == NULL) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        Rational start;
        if (!copy_id_valid(notes[i].note_id) || notes[i].midi == ANALYSIS_PITCH_UNKNOWN || notes[i].event == NULL ||
            notes[i].span == NULL || !score_time_span(notes[i].span, &start, NULL)) {
            return false;
        }
        if (i > 0) {
            Rational previous;
            score_time_span(notes[i - 1].span, &previous, NULL);
            if (rational_cmp(start, previous) < 0) {
                return false;
            }
        }
    }
    return true;
}

bool standard_form_next_note_step(const AnalysisNote *notes, size_t count, size_t index, long *semitones) {
    if (index + 1 >= count) {
        return false;
    }
    Rational end;
    Rational next_start;
    score_time_span(notes[index].span, NULL, &end);
    score_time_span(notes[index + 1].span, &next_start, NULL);
    if (rational_cmp(end, next_start) < 0) {
        return false;
    }
    if (semitones != NULL) {
        *semitones = (long)notes[index + 1].midi - (long)notes[index].midi;
    }
    return true;
}

StandardFormAnalysisStatus standard_form_input_finish(StandardFormInput *input) {
    if (input == NULL || input->map == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if (!line_valid(input->notes, input->note_count) || !line_valid(input->bass_notes, input->bass_count)) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    for (size_t i = 0; input->degrees != NULL && i < input->note_count; i++) {
        if (input->degrees[i] < 0 || input->degrees[i] > 7) {
            return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
        }
    }
    if (input->cadence_count > 1) {
        qsort(input->cadences, input->cadence_count, sizeof(StandardFormCadence), compare_cadences);
    }
    input->finished = true;
    return STANDARD_FORM_ANALYSIS_OK;
}
