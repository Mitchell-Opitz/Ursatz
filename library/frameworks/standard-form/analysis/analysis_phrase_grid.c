#include "analysis_phrase_grid.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "time/meter_arithmetic.h"
#include "time/meter_map.h"
#include "time/score_time.h"

#define CADENCE_PERFECT 1.0
#define CADENCE_OTHER 0.8
#define LONG_NOTE_WEIGHT 0.3
#define REST_WEIGHT 0.3
#define RETURN_WEIGHT 0.3
#define UNIT_COST 0.3
#define IRREGULAR_COST 0.6
#define ELISION_COST 0.1
#define MIN_RETURN_NOTES 6u
#define RETURN_DISTANCE 5
#define ONSET_TOLERANCE 1e-6
#define STRICTLY_HIGHER 1e-9
#define MAX_UNIT 16

typedef struct {
    const StandardFormInput *input;
    int first;
    int last;
    size_t measures;
    size_t *note_first; /* per measure (+ 3 sentinels): the first note starting in it */
    size_t *note_count;
    double *note_start; /* per note, in whole notes */
    double *note_end;
    double *measure_start; /* per measure */
    double *measure_end;
    double *beat;
    double *cadence;       /* per measure */
    unsigned char *authentic;
    double *evidence;
} Evidence;

static void evidence_free(Evidence *ev) {
    free(ev->note_first);
    free(ev->note_count);
    free(ev->note_start);
    free(ev->note_end);
    free(ev->measure_start);
    free(ev->measure_end);
    free(ev->beat);
    free(ev->cadence);
    free(ev->authentic);
    free(ev->evidence);
    memset(ev, 0, sizeof(*ev));
}

static size_t idx(const Evidence *ev, int measure) {
    return (size_t)(measure - ev->first);
}

static bool in_range(const Evidence *ev, int measure) {
    return measure >= ev->first && measure <= ev->last;
}

static size_t count_in(const Evidence *ev, int measure) {
    return in_range(ev, measure) ? ev->note_count[idx(ev, measure)] : 0;
}

static size_t first_in(const Evidence *ev, int measure) {
    return in_range(ev, measure) ? ev->note_first[idx(ev, measure)] : 0;
}

static bool cadence_weight(CadenceType type, double *weight, bool *authentic) {
    *authentic = false;
    switch (type) {
    case CADENCE_TYPE_PERFECT_AUTHENTIC:
        *weight = CADENCE_PERFECT;
        *authentic = true;
        return true;
    case CADENCE_TYPE_IMPERFECT_AUTHENTIC:
    case CADENCE_TYPE_AUTHENTIC_UNSPECIFIED:
        *weight = CADENCE_OTHER;
        *authentic = true;
        return true;
    case CADENCE_TYPE_HALF:
        *weight = CADENCE_OTHER;
        return true;
    default:
        return false;
    }
}

/* The notes starting in m + 1 and m + 2 repeat the onsets and steps of the notes starting in some b and b + 1. */
static bool melody_returns(const Evidence *ev, int m) {
    size_t a_count = count_in(ev, m + 1);
    size_t b_count = count_in(ev, m + 2);
    size_t total = a_count + b_count;
    if (total < MIN_RETURN_NOTES) {
        return false;
    }
    size_t base = first_in(ev, m + 1);
    double base_start = ev->measure_start[idx(ev, m + 1)];
    for (int b = ev->first; b <= m - RETURN_DISTANCE; b++) {
        if (count_in(ev, b) + count_in(ev, b + 1) != total) {
            continue;
        }
        size_t at = first_in(ev, b);
        double at_start = ev->measure_start[idx(ev, b)];
        bool same = true;
        for (size_t k = 0; same && k < total; k++) {
            if (fabs((ev->note_start[at + k] - at_start) - (ev->note_start[base + k] - base_start)) > ONSET_TOLERANCE) {
                same = false;
            }
            if (same && k > 0 &&
                ev->input->notes[at + k].semitone - ev->input->notes[at + k - 1].semitone !=
                    ev->input->notes[base + k].semitone - ev->input->notes[base + k - 1].semitone) {
                same = false;
            }
        }
        if (same) {
            return true;
        }
    }
    return false;
}

/* No melody note sounds in the last half beat of measure m. */
static bool melody_rests(const Evidence *ev, int m) {
    size_t i = idx(ev, m);
    double window = ev->measure_end[i] - ev->beat[i] / 2.0;
    size_t after = ev->note_first[i] + ev->note_count[i]; /* notes [0, after) start in or before m */
    for (size_t k = after; k > 0; k--) {
        if (ev->note_end[k - 1] > window + STRICTLY_HIGHER) {
            return false;
        }
    }
    return true;
}

static bool last_note_long(const Evidence *ev, int m) {
    size_t i = idx(ev, m);
    if (ev->note_count[i] == 0) {
        return false;
    }
    size_t k = ev->note_first[i] + ev->note_count[i] - 1;
    double length = ev->measure_end[i] - ev->measure_start[i];
    return (ev->note_end[k] - ev->note_start[k]) * 2.0 >= length - STRICTLY_HIGHER;
}

static StandardFormAnalysisStatus evidence_build(const StandardFormInput *input, Evidence *ev) {
    memset(ev, 0, sizeof(*ev));
    ev->input = input;
    size_t count = input->note_count;
    ev->note_start = (double *)calloc(count, sizeof(double));
    ev->note_end = (double *)calloc(count, sizeof(double));
    int *note_measure = (int *)calloc(count, sizeof(int));
    if (ev->note_start == NULL || ev->note_end == NULL || note_measure == NULL) {
        free(note_measure);
        evidence_free(ev);
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        Rational start;
        Rational end;
        if (!score_time_span(input->notes[i].span, &start, &end) ||
            meter_map_measure_of(input->map, start, &note_measure[i]) != METER_MAP_OK) {
            free(note_measure);
            evidence_free(ev);
            return STANDARD_FORM_ANALYSIS_ERR_METER;
        }
        ev->note_start[i] = rational_to_double(start);
        ev->note_end[i] = rational_to_double(end);
        if (i == 0 || note_measure[i] < ev->first) {
            ev->first = note_measure[i];
        }
        if (i == 0 || note_measure[i] > ev->last) {
            ev->last = note_measure[i];
        }
    }
    ev->measures = (size_t)(ev->last - ev->first + 1);
    size_t slots = ev->measures + 3;
    ev->note_first = (size_t *)calloc(slots, sizeof(size_t));
    ev->note_count = (size_t *)calloc(slots, sizeof(size_t));
    ev->measure_start = (double *)calloc(slots, sizeof(double));
    ev->measure_end = (double *)calloc(slots, sizeof(double));
    ev->beat = (double *)calloc(slots, sizeof(double));
    ev->cadence = (double *)calloc(slots, sizeof(double));
    ev->authentic = (unsigned char *)calloc(slots, sizeof(unsigned char));
    ev->evidence = (double *)calloc(slots, sizeof(double));
    if (ev->note_first == NULL || ev->note_count == NULL || ev->measure_start == NULL || ev->measure_end == NULL ||
        ev->beat == NULL || ev->cadence == NULL || ev->authentic == NULL || ev->evidence == NULL) {
        free(note_measure);
        evidence_free(ev);
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        ev->note_count[idx(ev, note_measure[i])]++;
    }
    free(note_measure);
    for (size_t m = 0, running = 0; m < ev->measures; m++) { /* an empty measure's first is the next note */
        ev->note_first[m] = running;
        running += ev->note_count[m];
    }
    for (int m = ev->first; m <= ev->last; m++) {
        Rational start;
        Rational end;
        Rational beat;
        if (meter_map_measure_bounds(input->map, m, &start, &end) != METER_MAP_OK ||
            meter_map_beat_length(input->map, m, &beat) != METER_MAP_OK) {
            evidence_free(ev);
            return STANDARD_FORM_ANALYSIS_ERR_METER;
        }
        ev->measure_start[idx(ev, m)] = rational_to_double(start);
        ev->measure_end[idx(ev, m)] = rational_to_double(end);
        ev->beat[idx(ev, m)] = rational_to_double(beat);
    }
    for (size_t c = 0; c < input->cadence_count; c++) {
        const StandardFormCadence *cadence = &input->cadences[c];
        double weight = 0.0;
        bool authentic = false;
        if (!cadence_weight(cadence->type, &weight, &authentic)) {
            continue;
        }
        Rational start;
        Rational end;
        int from = 0;
        int to = 0;
        if (!score_time_span(cadence->arrival, &start, &end) || meter_map_measure_of(input->map, start, &from) != METER_MAP_OK ||
            meter_map_last_measure_touched(input->map, end, &to) != METER_MAP_OK) {
            evidence_free(ev);
            return STANDARD_FORM_ANALYSIS_ERR_METER;
        }
        if (to < from) {
            to = from;
        }
        for (int m = from < ev->first ? ev->first : from; m <= to && m <= ev->last; m++) {
            size_t i = idx(ev, m);
            if (weight > ev->cadence[i]) {
                ev->cadence[i] = weight;
            }
            if (authentic) {
                ev->authentic[i] = 1;
            }
        }
    }
    for (int m = ev->first; m <= ev->last; m++) {
        ev->evidence[idx(ev, m)] = ev->cadence[idx(ev, m)] + (last_note_long(ev, m) ? LONG_NOTE_WEIGHT : 0.0) +
                                   (melody_rests(ev, m) ? REST_WEIGHT : 0.0) + (melody_returns(ev, m) ? RETURN_WEIGHT : 0.0);
    }
    return STANDARD_FORM_ANALYSIS_OK;
}

void standard_form_phrase_grid_destroy(StandardFormPhraseGrid *grid) {
    if (grid == NULL) {
        return;
    }
    free(grid->units);
    grid->units = NULL;
    grid->count = 0;
}

typedef struct {
    bool reached;
    double total;
    int prev_first; /* the state the unit that led here started from: its start measure and elided flag */
    int prev_elided;
} State;

StandardFormAnalysisStatus standard_form_find_phrase_grid(const StandardFormInput *input, StandardFormPhraseGrid *out) {
    if (input == NULL || out == NULL || input->map == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if (!input->finished) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    StandardFormPhraseGrid grid = {NULL, 0};
    if (input->note_count == 0) {
        *out = grid;
        return STANDARD_FORM_ANALYSIS_OK;
    }
    Evidence ev;
    StandardFormAnalysisStatus status = evidence_build(input, &ev);
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        return status;
    }
    int first_counted = meter_map_first_counted_measure(input->map);
    State *states = (State *)calloc(ev.measures * 2, sizeof(State)); /* [measure index * 2 + elided] */
    if (states == NULL) {
        evidence_free(&ev);
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    states[0].reached = true;
    bool have_final = false;
    double final_total = 0.0;
    int final_first = 0;
    int final_elided = 0;
    for (int f = ev.first; f <= ev.last; f++) {
        for (int elided = 0; elided < 2; elided++) {
            const State *state = &states[idx(&ev, f) * 2 + (size_t)elided];
            if (!state->reached) {
                continue;
            }
            int counted = f < first_counted ? first_counted : f;
            for (int e = counted + 1; e <= ev.last && e - counted + 1 <= MAX_UNIT; e++) {
                int length = e - counted + 1;
                double score = ev.evidence[idx(&ev, e)] - UNIT_COST;
                if (length != 4 && length != 8 && length != 16) {
                    score -= IRREGULAR_COST;
                }
                if (elided) {
                    score -= ELISION_COST;
                }
                if (length == 8 || length == 16) {
                    score += ev.cadence[idx(&ev, counted + length / 2 - 1)];
                }
                double total = state->total + score;
                if (e == ev.last) {
                    if (!have_final || total > final_total + STRICTLY_HIGHER) {
                        have_final = true;
                        final_total = total;
                        final_first = f;
                        final_elided = elided;
                    }
                    continue;
                }
                for (int next_elided = 0; next_elided < 2; next_elided++) {
                    if (next_elided && !ev.authentic[idx(&ev, e)]) {
                        continue;
                    }
                    State *next = &states[idx(&ev, next_elided ? e : e + 1) * 2 + (size_t)next_elided];
                    if (!next->reached || total > next->total + STRICTLY_HIGHER) {
                        next->reached = true;
                        next->total = total;
                        next->prev_first = f;
                        next->prev_elided = elided;
                    }
                }
            }
        }
    }
    if (have_final) {
        size_t count = 0;
        int f = final_first;
        int elided = final_elided;
        for (;;) {
            count++;
            if (f == ev.first && !elided) {
                break;
            }
            const State *state = &states[idx(&ev, f) * 2 + (size_t)elided];
            f = state->prev_first;
            elided = state->prev_elided;
        }
        grid.units = (StandardFormGridUnit *)calloc(count, sizeof(StandardFormGridUnit));
        if (grid.units == NULL) {
            free(states);
            evidence_free(&ev);
            return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
        }
        grid.count = count;
        f = final_first;
        elided = final_elided;
        int e = ev.last;
        for (size_t k = count; k > 0; k--) {
            StandardFormGridUnit *unit = &grid.units[k - 1];
            unit->first_measure = f;
            unit->counted_first = f < first_counted ? first_counted : f;
            unit->last_measure = e;
            unit->length_measures = e - unit->counted_first + 1;
            unit->elided = elided != 0;
            unit->half_measure = (unit->length_measures == 8 || unit->length_measures == 16)
                                     ? unit->counted_first + unit->length_measures / 2 - 1
                                     : -1;
            const State *state = &states[idx(&ev, f) * 2 + (size_t)elided];
            e = elided ? f : f - 1; /* the previous unit's last measure */
            f = state->prev_first;
            elided = state->prev_elided;
        }
    }
    free(states);
    evidence_free(&ev);
    *out = grid;
    return STANDARD_FORM_ANALYSIS_OK;
}
