#include "analysis_motive_shape.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "time/meter_arithmetic.h"
#include "time/meter_map.h"
#include "time/score_time.h"

void standard_form_motive_line_destroy(StandardFormMotiveLine *line) {
    if (line == NULL) {
        return;
    }
    free(line->start);
    free(line->end);
    free(line->first_measure);
    free(line->can_start);
    free(line->has_generic);
    free(line->generic);
    memset(line, 0, sizeof(*line));
}

/* The signed generic interval between two notes with known degrees: the letter distance in the direction of motion plus the octaves
   the semitones imply (7 steps to an octave). 0 for an unchanged degree. False when the degrees and the direction disagree
   (a nonzero semitone change cannot be read as a generic interval of zero). */
static int generic_between(const StandardFormNote *a, const StandardFormNote *b, long semitones) {
    if (semitones == 0) {
        return 0;
    }
    int letters = semitones > 0 ? (((b->degree - a->degree) % 7) + 7) % 7 : (((a->degree - b->degree) % 7) + 7) % 7;
    double distance = (double)labs(semitones);
    long octaves = (long)floor((distance - (double)letters * 12.0 / 7.0) / 12.0 + 0.5);
    if (octaves < 0) {
        octaves = 0;
    }
    int magnitude = letters + 7 * (int)octaves;
    return semitones > 0 ? magnitude : -magnitude;
}

bool standard_form_motive_line_build(StandardFormMotiveLine *line, const StandardFormInput *input, int line_id,
                                     const bool *idea_start) {
    memset(line, 0, sizeof(*line));
    line->line = line_id;
    line->notes = line_id == 0 ? input->notes : input->bass_notes;
    line->count = line_id == 0 ? input->note_count : input->bass_count;
    if (line->count == 0) {
        return true;
    }
    size_t n = line->count;
    line->start = (Rational *)calloc(n, sizeof(Rational));
    line->end = (Rational *)calloc(n, sizeof(Rational));
    line->first_measure = (int *)calloc(n, sizeof(int));
    line->can_start = (bool *)calloc(n, sizeof(bool));
    line->has_generic = (bool *)calloc(n, sizeof(bool));
    line->generic = (int *)calloc(n, sizeof(int));
    if (line->start == NULL || line->end == NULL || line->first_measure == NULL || line->can_start == NULL ||
        line->has_generic == NULL || line->generic == NULL) {
        standard_form_motive_line_destroy(line);
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        if (!score_time_span(line->notes[i].span, &line->start[i], &line->end[i]) ||
            meter_map_measure_of(input->map, line->start[i], &line->first_measure[i]) != METER_MAP_OK) {
            standard_form_motive_line_destroy(line);
            return false;
        }
        line->can_start[i] = (idea_start != NULL && idea_start[i]) || meter_map_on_beat(input->map, line->start[i]);
        if (line->notes[i].has_next && line->notes[i].degree > 0 && i + 1 < n && line->notes[i + 1].degree > 0) {
            line->has_generic[i] = true;
            line->generic[i] = generic_between(&line->notes[i], &line->notes[i + 1], line->notes[i].semitones_to_next);
        }
    }
    return true;
}

bool standard_form_motive_window_ok(const StandardFormMotiveLine *line, size_t first, size_t n) {
    if (n < 2 || n > STANDARD_FORM_MOTIVE_MAX_INTERVALS + 1 || first + n > line->count) {
        return false;
    }
    for (size_t i = first; i + 1 < first + n; i++) {
        if (!line->notes[i].has_next || rational_cmp(line->start[i + 1], line->start[i]) <= 0) {
            return false;
        }
    }
    return true;
}

bool standard_form_motive_shape(const StandardFormMotiveLine *line, size_t first, size_t n, bool want_generic,
                                StandardFormMotiveShape *out) {
    if (!standard_form_motive_window_ok(line, first, n)) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->generic = want_generic;
    out->intervals = n - 1;
    out->first_semitone = line->notes[first].semitone;
    for (size_t k = 0; k + 1 < n; k++) {
        size_t i = first + k;
        if (want_generic && !line->has_generic[i]) {
            return false;
        }
        out->semitones[k] = (int)line->notes[i].semitones_to_next;
        out->steps[k] = want_generic ? line->generic[i] : out->semitones[k];
        if (rational_sub(line->start[i + 1], line->start[i], &out->ioi[k]) != RATIONAL_OK) {
            return false;
        }
    }
    return true;
}
