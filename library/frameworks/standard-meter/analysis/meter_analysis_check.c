#include "meter_analysis_check.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/value.h"
#include "meter_analysis_claim.h"
#include "time/musical_time.h"
#include "time/time_signature.h"

/* ---- Exact time (whole notes) ---- */

/* Rational arithmetic with a sticky overflow flag: after an overflow the results are 0/1 and `overflow` stays set, so a
   caller checks once at the end of a unit of work. */
typedef struct {
    bool overflow;
} Calc;

static Rational calc_take(Calc *calc, RationalResult result, Rational value) {
    if (result != RATIONAL_OK) {
        calc->overflow = true;
        return rational_ratio(0, 1);
    }
    return value;
}

static Rational calc_make(Calc *calc, long long num, long long den) {
    Rational out = {0, 1};
    return calc_take(calc, rational_make(num, den, &out), out);
}

static Rational calc_sub(Calc *calc, Rational a, Rational b) {
    Rational out = {0, 1};
    return calc_take(calc, rational_sub(a, b, &out), out);
}

static Rational calc_mul(Calc *calc, Rational a, Rational b) {
    Rational out = {0, 1};
    return calc_take(calc, rational_mul(a, b, &out), out);
}

static Rational calc_div(Calc *calc, Rational a, Rational b) {
    Rational out = {0, 1};
    return calc_take(calc, rational_div(a, b, &out), out);
}

static bool is_integer(Rational a) {
    return a.den == 1;
}

/* ---- Barlines ---- */

/* A grid of barlines and beats in the file's time. Either one measure length laid from an offset, or the meter map. */
typedef struct {
    const MeterMap *map; /* the effective reading's grid; NULL for a candidate */
    Rational measure;    /* a candidate's measure and beat lengths, in the file's time */
    Rational beat;
    Rational offset;     /* the first barline, 0 or later */
    bool four_beats;     /* a candidate's meter has four beats: its third beat is a secondary downbeat */
} Grid;

static bool signature_is_compound(unsigned top, unsigned bottom) {
    return bottom == 8 && (top == 6 || top == 9 || top == 12);
}

/* Whether a signature has four beats, so that its third beat is the secondary accent. */
static bool four_beats(unsigned top, unsigned bottom) {
    return (top == 4 && bottom == 4) || (top == 12 && bottom == 8);
}

/* The beat of a signature in whole notes: the denominator's note, a dotted one in the compound meters. */
static Rational beat_length(Calc *calc, unsigned top, unsigned bottom) {
    return calc_make(calc, signature_is_compound(top, bottom) ? 3 : 1, (long long)bottom);
}

static Rational time_rat(Calc *calc, const MusicalTime *time) {
    unsigned long den = musical_time_denominator(time);
    if (den > (unsigned long)LLONG_MAX) {
        calc->overflow = true;
        return rational_ratio(0, 1);
    }
    return calc_make(calc, (long long)musical_time_numerator(time), (long long)den);
}

/* The MusicalTime of x, or NULL when x is negative or does not fit. */
static MusicalTime *time_of(Rational x) {
    MusicalTime *time = NULL;
    if (x.num < 0 || x.num > LONG_MAX || musical_time_create((long)x.num, (unsigned long)x.den, &time) != MUSICAL_TIME_OK) {
        return NULL;
    }
    return time;
}

/* Whether the grid has a barline (a downbeat) at x, and whether it has a beat. */
static void grid_place(Calc *calc, const Grid *grid, Rational x, bool *downbeat, bool *beat, bool *secondary) {
    *downbeat = false;
    *beat = false;
    *secondary = false;
    const Rational half = {1, 2};
    if (!grid->map) {
        Rational from = calc_sub(calc, x, grid->offset);
        Rational measures = calc_div(calc, from, grid->measure);
        *downbeat = is_integer(measures);
        *beat = is_integer(calc_div(calc, from, grid->beat));
        *secondary = grid->four_beats && is_integer(calc_sub(calc, measures, half));
        return;
    }
    MusicalTime *time = time_of(x);
    if (!time) {
        return;
    }
    const MeterMapRegion *region = NULL;
    MeterMapResult found = meter_map_region_at(grid->map, time, &region);
    musical_time_destroy(time);
    if (found != METER_MAP_OK) {
        return;
    }
    const TimeSignature *signature = meter_map_region_time_signature(region);
    unsigned top = time_signature_top_number(signature);
    unsigned bottom = time_signature_bottom_number(signature);
    /* a pickup region's grid is laid back from its end, the first barline */
    const MusicalTime *anchor_time =
        meter_map_region_is_pickup(region) && meter_map_region_end(region) ? meter_map_region_end(region)
                                                                           : meter_map_region_start(region);
    Rational from = calc_sub(calc, x, time_rat(calc, anchor_time));
    Rational measure = calc_make(calc, (long long)top, (long long)bottom);
    Rational measures = calc_div(calc, from, measure);
    *downbeat = is_integer(measures);
    *beat = is_integer(calc_div(calc, from, beat_length(calc, top, bottom)));
    *secondary = four_beats(top, bottom) && is_integer(calc_sub(calc, measures, half));
}

/* How many downbeats the grid has from time from to time to, both inclusive; at least 1. */
static long long downbeats_between(Calc *calc, const Grid *grid, Rational from, Rational to) {
    long long count = 1;
    if (!grid->map) {
        long long last = 0;
        long long first = 0;
        Rational first_measures = calc_div(calc, calc_sub(calc, from, grid->offset), grid->measure);
        Rational last_measures = calc_div(calc, calc_sub(calc, to, grid->offset), grid->measure);
        if (rational_floor(last_measures, &last) != RATIONAL_OK || rational_ceil(first_measures, &first) != RATIONAL_OK ||
            (first > 0 && last < LLONG_MIN + first) || (first < 0 && last > LLONG_MAX + first) ||
            last - first == LLONG_MAX) {
            calc->overflow = true;
            return 1;
        }
        count = last - first + 1;
    } else {
        int measure_from = 0;
        int measure_to = 0;
        MusicalTime *a = time_of(from);
        MusicalTime *b = time_of(to);
        if (a && b && meter_map_measure_index_at(grid->map, a, &measure_from) == METER_MAP_OK &&
            meter_map_measure_index_at(grid->map, b, &measure_to) == METER_MAP_OK) {
            count = (long long)measure_to - (long long)measure_from + 1;
        }
        musical_time_destroy(a);
        musical_time_destroy(b);
    }
    return count < 1 ? 1 : count;
}

/* The score of a grid in [0, 1]. */
static double grid_score(Calc *calc, const Grid *grid, const StandardMeterCheckEvidence *evidence) {
    double weights = 0.0;
    double sum = 0.0;
    if (evidence->change_count > 0) {
        size_t on_down = 0;
        size_t on_beat = 0;
        double on_accent = 0.0; /* a secondary downbeat counts half */
        for (size_t i = 0; i < evidence->change_count; i++) {
            bool downbeat;
            bool beat;
            bool secondary;
            grid_place(calc, grid, evidence->changes[i], &downbeat, &beat, &secondary);
            on_down += downbeat ? 1 : 0;
            on_accent += downbeat ? 1.0 : (secondary ? 0.5 : 0.0);
            on_beat += (downbeat || beat) ? 1 : 0;
        }
        /* The downbeat share is the harmonic mean of the share of the changes on a downbeat and the share of the
           downbeats that carry a change. Without the second, a grid with a barline at every beat would always win: it
           holds every barline of the real meter and more. */
        double recall = on_accent / (double)evidence->change_count;
        long long downbeats =
            downbeats_between(calc, grid, evidence->changes[0], evidence->changes[evidence->change_count - 1]);
        double precision = fmin(1.0, (double)on_down / (double)downbeats);
        double downbeat_fit = recall + precision > 0.0 ? 2.0 * recall * precision / (recall + precision) : 0.0;
        sum += 2.0 * downbeat_fit + 1.0 * (double)on_beat / (double)evidence->change_count;
        weights += 3.0;
    }
    if (evidence->arrival_count > 0) {
        double on_down = 0.0;
        for (size_t i = 0; i < evidence->arrival_count; i++) {
            bool downbeat;
            bool beat;
            bool secondary;
            grid_place(calc, grid, evidence->arrivals[i], &downbeat, &beat, &secondary);
            on_down += downbeat ? 1.0 : (secondary ? 0.5 : 0.0);
        }
        sum += 3.0 * on_down / (double)evidence->arrival_count;
        weights += 3.0;
    }
    return weights > 0.0 ? sum / weights : 0.0;
}

/* ---- Inter-onset interval ---- */

static int rational_compare_qsort(const void *a, const void *b) {
    return rational_cmp(*(const Rational *)a, *(const Rational *)b);
}

/* The most common gap between consecutive distinct onsets, the smallest of equally common ones. *ok is false when there are
   fewer than two distinct onsets. */
static StandardMeterAnalysisStatus common_interval(Calc *calc, const Rational *source, size_t count, Rational *interval,
                                                   bool *ok) {
    *ok = false;
    if (count < 2) {
        return STANDARD_METER_ANALYSIS_OK;
    }
    Rational *onsets = (Rational *)malloc(count * sizeof(Rational));
    if (!onsets) {
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    memcpy(onsets, source, count * sizeof(Rational));
    qsort(onsets, count, sizeof(Rational), rational_compare_qsort);
    size_t gaps = 0;
    for (size_t i = 1; i < count; i++) {
        if (rational_cmp(onsets[i], onsets[i - 1]) > 0) {
            onsets[gaps++] = calc_sub(calc, onsets[i], onsets[i - 1]); /* in place: the gap index never passes i */
        }
    }
    if (gaps > 0) {
        qsort(onsets, gaps, sizeof(Rational), rational_compare_qsort);
        size_t best_start = 0;
        size_t best_run = 0;
        for (size_t start = 0; start < gaps;) {
            size_t end = start + 1;
            while (end < gaps && rational_cmp(onsets[end], onsets[start]) == 0) {
                end++;
            }
            if (end - start > best_run) {
                best_run = end - start;
                best_start = start;
            }
            start = end;
        }
        *interval = onsets[best_start];
        *ok = true;
    }
    free(onsets);
    return STANDARD_METER_ANALYSIS_OK;
}

/* ---- Candidates ---- */

typedef struct {
    unsigned top;
    unsigned bottom;
} MeterShape;

static const MeterShape METERS[] = {{2, 4}, {3, 4}, {4, 4}, {3, 8}, {6, 8}, {9, 8}, {12, 8}};
#define METER_COUNT (sizeof(METERS) / sizeof(METERS[0]))

typedef struct {
    unsigned top;
    unsigned bottom;
    Rational scale;  /* 1/2, 1 or 2 */
    Rational pickup; /* whole notes after scaling */
    Grid grid;
    double score;
    double distance; /* of the most common interval, scaled, from an eighth note; huge when the piece has none */
} Candidate;

static const Rational SCALES[] = {{1, 2}, {1, 1}, {2, 1}};

static bool same_grid(Calc *calc, const Grid *a, const Grid *b) {
    return rational_cmp(a->measure, b->measure) == 0 && rational_cmp(a->beat, b->beat) == 0 &&
           is_integer(calc_div(calc, calc_sub(calc, a->offset, b->offset), a->measure));
}

/* Whether a is the better one to show of two candidates with the same barlines. */
static bool shown_before(const Candidate *a, const Candidate *b) {
    if (a->distance != b->distance) {
        return a->distance < b->distance;
    }
    return a->bottom == 4 && b->bottom == 8;
}

static StandardMeterReading reading_of(const Candidate *candidate) {
    StandardMeterReading reading;
    memset(&reading, 0, sizeof(reading));
    reading.beats = candidate->top;
    reading.beat_type = candidate->bottom;
    reading.scale = candidate->scale;
    reading.pickup = candidate->pickup;
    reading.score = candidate->score;
    return reading;
}

/* The effective reading from the map: its first full measure's signature, scale 1, the length of a leading pickup region as
   the pickup. The grid is the map itself. False when the map has no full measure. */
static bool effective_candidate(Calc *calc, const MeterMap *map, Candidate *effective, Grid *barlines) {
    size_t index = meter_map_has_pickup(map) ? 1 : 0;
    const MeterMapRegion *full = meter_map_region(map, index);
    if (!full) {
        return false;
    }
    const TimeSignature *signature = meter_map_region_time_signature(full);
    unsigned top = time_signature_top_number(signature);
    unsigned bottom = time_signature_bottom_number(signature);
    Rational pickup = {0, 1};
    if (index == 1) {
        const MusicalTime *end = meter_map_region_end(meter_map_region(map, 0));
        if (end) {
            pickup = time_rat(calc, end);
        }
    }
    memset(effective, 0, sizeof(*effective));
    effective->top = top;
    effective->bottom = bottom;
    effective->scale = rational_ratio(1, 1);
    effective->pickup = pickup;
    effective->grid.map = map;
    /* the first full measure's grid, for telling equal candidates apart */
    barlines->map = NULL;
    barlines->measure = calc_make(calc, (long long)top, (long long)bottom);
    barlines->beat = beat_length(calc, top, bottom);
    barlines->offset = pickup;
    effective->grid.four_beats = four_beats(top, bottom);
    return true;
}

StandardMeterAnalysisStatus standard_meter_check_meter(const MeterMap *map, const StandardMeterCheckEvidence *evidence,
                                                       StandardMeterCheck *out) {
    if (map == NULL || evidence == NULL || out == NULL) {
        return STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    if ((evidence->change_count > 0 && evidence->changes == NULL) ||
        (evidence->arrival_count > 0 && evidence->arrivals == NULL) ||
        (evidence->onset_count > 0 && evidence->onsets == NULL)) {
        return STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    if (evidence->change_count == 0) {
        return STANDARD_METER_ANALYSIS_OK;
    }
    Calc calc = {false};
    Candidate effective;
    Grid effective_barlines;
    if (!effective_candidate(&calc, map, &effective, &effective_barlines)) {
        return calc.overflow ? STANDARD_METER_ANALYSIS_ERR_OVERFLOW : STANDARD_METER_ANALYSIS_OK;
    }
    Rational interval = {0, 1};
    bool has_interval = false;
    StandardMeterAnalysisStatus status =
        common_interval(&calc, evidence->onsets, evidence->onset_count, &interval, &has_interval);
    if (status != STANDARD_METER_ANALYSIS_OK) {
        return status;
    }
    effective.score = grid_score(&calc, &effective.grid, evidence);

    Candidate *best = NULL;
    Candidate best_value;
    const Rational eighth = {1, 8};
    for (size_t m = 0; m < METER_COUNT && !calc.overflow; m++) {
        unsigned top = METERS[m].top;
        unsigned bottom = METERS[m].bottom;
        Rational beat = beat_length(&calc, top, bottom);
        size_t beats = signature_is_compound(top, bottom) ? top / 3 : top;
        for (size_t s = 0; s < sizeof(SCALES) / sizeof(SCALES[0]); s++) {
            Rational scale = SCALES[s];
            for (size_t k = 0; k < beats; k++) {
                Candidate candidate;
                memset(&candidate, 0, sizeof(candidate));
                candidate.top = top;
                candidate.bottom = bottom;
                candidate.scale = scale;
                candidate.pickup = calc_mul(&calc, beat, rational_ratio((long long)k, 1));
                candidate.grid.measure = calc_div(&calc, calc_make(&calc, (long long)top, (long long)bottom), scale);
                candidate.grid.beat = calc_div(&calc, beat, scale);
                candidate.grid.offset = calc_div(&calc, candidate.pickup, scale);
                candidate.grid.four_beats = four_beats(top, bottom);
                if (rational_cmp(candidate.grid.measure, effective_barlines.beat) <= 0) {
                    continue; /* a bar no longer than the beat already counted only adds barlines */
                }
                if (same_grid(&calc, &candidate.grid, &effective_barlines)) {
                    continue;
                }
                candidate.distance =
                    has_interval ? fabs(rational_to_double(calc_sub(&calc, calc_mul(&calc, interval, scale), eighth))) : 1e9;
                candidate.score = grid_score(&calc, &candidate.grid, evidence);
                if (!best) {
                    best_value = candidate;
                    best = &best_value;
                } else if (candidate.score > best->score) {
                    best_value = candidate;
                } else if (candidate.score == best->score && same_grid(&calc, &candidate.grid, &best->grid) &&
                           shown_before(&candidate, best)) {
                    best_value = candidate;
                }
            }
        }
    }
    if (calc.overflow) {
        memset(out, 0, sizeof(*out));
        return STANDARD_METER_ANALYSIS_ERR_OVERFLOW;
    }

    out->present = true;
    out->effective = reading_of(&effective);
    if (best) {
        out->has_other = true;
        out->other = reading_of(best);
        double margin = best->score - effective.score;
        out->has_alternative = margin >= STANDARD_METER_CHECK_MIN_MARGIN;
        double size = fabs(margin);
        out->uncertainty = size >= 1.0 ? 0.0 : 1.0 - size;
    }
    return STANDARD_METER_ANALYSIS_OK;
}

StandardMeterAnalysisStatus standard_meter_check_changes(const char *const *labels, const long *regions, const Rational *times,
                                                         size_t count, Rational **out, size_t *out_count) {
    if (out == NULL || out_count == NULL || (count > 0 && times == NULL)) {
        return STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    *out = NULL;
    *out_count = 0;
    if (count == 0) {
        return STANDARD_METER_ANALYSIS_OK;
    }
    Rational *changes = (Rational *)malloc(count * sizeof(Rational));
    if (changes == NULL) {
        return STANDARD_METER_ANALYSIS_ERR_ALLOC;
    }
    size_t found = 0;
    for (size_t i = 0; i < count; i++) {
        bool change = i == 0;
        if (!change) {
            long before_region = regions ? regions[i - 1] : -1;
            long region = regions ? regions[i] : -1;
            const char *before = labels ? labels[i - 1] : NULL;
            const char *after = labels ? labels[i] : NULL;
            if (before_region != region) {
                change = true;
            } else if (!before || !after) {
                change = before != after;
            } else {
                change = strcmp(before, after) != 0;
            }
        }
        if (change) {
            changes[found++] = times[i];
        }
    }
    *out = changes;
    *out_count = found;
    return STANDARD_METER_ANALYSIS_OK;
}

Interpretation *standard_meter_check_interpretation(const StandardMeterCheck *check, const char *target_id) {
    if (check == NULL || target_id == NULL || !check->present) {
        return NULL;
    }
    char id[400];
    snprintf(id, sizeof(id), "standard-meter-meter-check-%s", target_id);
    return standard_meter_analysis_interpretation(id, target_id, STANDARD_METER_CHECK_CLAIM_TYPE, check, sizeof(*check),
                                                 check->uncertainty, "meter-check");
}

bool standard_meter_check_claim_read(const Interpretation *interpretation, StandardMeterCheck *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    if (claim == NULL || strcmp(value_type(claim), STANDARD_METER_CHECK_CLAIM_TYPE) != 0) {
        return false;
    }
    size_t size = 0;
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}
