#include "pipeline/meter_check_pipeline.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline/harmonic_rhythm_pipeline.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"

/* ---- Exact time (whole notes) ---- */

__extension__ typedef __int128 Wide;

typedef struct {
    Wide num;
    Wide den; /* positive, lowest terms */
} Rat;

static Wide wide_abs(Wide v) {
    return v < 0 ? -v : v;
}

static Wide wide_gcd(Wide a, Wide b) {
    a = wide_abs(a);
    b = wide_abs(b);
    while (b != 0) {
        Wide t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static Rat rat_make(Wide num, Wide den) {
    Wide g = wide_gcd(num, den);
    if (g > 1) {
        num /= g;
        den /= g;
    }
    return (Rat){num, den};
}

static Rat rat_sub(Rat a, Rat b) {
    return rat_make(a.num * b.den - b.num * a.den, a.den * b.den);
}

static Rat rat_mul(Rat a, Rat b) {
    return rat_make(a.num * b.num, a.den * b.den);
}

static Rat rat_div(Rat a, Rat b) {
    return rat_make(a.num * b.den, a.den * b.num);
}

static int rat_cmp(Rat a, Rat b) {
    Wide left = a.num * b.den;
    Wide right = b.num * a.den;
    return left < right ? -1 : (left > right ? 1 : 0);
}

static bool rat_is_integer(Rat a) {
    return a.den == 1;
}

static double rat_double(Rat a) {
    return (double)a.num / (double)a.den;
}

/* ---- Barlines ---- */

/* A grid of barlines and beats in the file's time. Either one measure length laid from an offset, or the analysis's
   meter map. */
typedef struct {
    const MeterMap *map; /* the effective reading's grid; NULL for a candidate */
    Rat measure;         /* a candidate's measure and beat lengths, in the file's time */
    Rat beat;
    Rat offset;          /* the first barline, 0 or later */
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
static Rat beat_length(unsigned top, unsigned bottom) {
    return rat_make(signature_is_compound(top, bottom) ? 3 : 1, (Wide)bottom);
}

static Rat time_rat(const MusicalTime *time) {
    return rat_make((Wide)musical_time_numerator(time), (Wide)musical_time_denominator(time));
}

/* Whether the grid has a barline (a downbeat) at x, and whether it has a beat. */
static void grid_place(const Grid *grid, Rat x, bool *downbeat, bool *beat, bool *secondary) {
    *downbeat = false;
    *beat = false;
    *secondary = false;
    if (!grid->map) {
        Rat from = rat_sub(x, grid->offset);
        *downbeat = rat_is_integer(rat_div(from, grid->measure));
        *beat = rat_is_integer(rat_div(from, grid->beat));
        *secondary = grid->four_beats && rat_is_integer(rat_sub(rat_div(from, grid->measure), (Rat){1, 2}));
        return;
    }
    MusicalTime *time = NULL;
    if (musical_time_create((long)x.num, (unsigned long)x.den, &time) != MUSICAL_TIME_OK) {
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
    Rat from = rat_sub(x, time_rat(anchor_time));
    Rat measure = rat_make((Wide)top, (Wide)bottom);
    *downbeat = rat_is_integer(rat_div(from, measure));
    *beat = rat_is_integer(rat_div(from, beat_length(top, bottom)));
    *secondary = four_beats(top, bottom) && rat_is_integer(rat_sub(rat_div(from, measure), (Rat){1, 2}));
}

/* ---- Evidence ---- */

typedef struct {
    Rat *changes;
    size_t change_count;
    Rat *arrivals;
    size_t arrival_count;
} Evidence;

static Rat entry_time(const AnalysisReportEntry *entry) {
    return rat_make((Wide)entry->time_numerator, (Wide)entry->time_denominator);
}

static void evidence_destroy(Evidence *evidence) {
    free(evidence->changes);
    free(evidence->arrivals);
    memset(evidence, 0, sizeof(*evidence));
}

static bool evidence_collect(const AnalysisReportEntry *entries, size_t entry_count, const AnalysisCadence *cadences,
                             size_t cadence_count, Evidence *evidence) {
    memset(evidence, 0, sizeof(*evidence));
    evidence->changes = malloc(entry_count * sizeof(Rat));
    evidence->arrivals = cadence_count > 0 ? malloc(cadence_count * sizeof(Rat)) : NULL;
    if (!evidence->changes || (cadence_count > 0 && !evidence->arrivals)) {
        evidence_destroy(evidence);
        return false;
    }
    for (size_t i = 0; i < entry_count; i++) {
        if (i == 0 || harmonic_rhythm_is_change(&entries[i - 1], &entries[i])) {
            evidence->changes[evidence->change_count++] = entry_time(&entries[i]);
        }
    }
    for (size_t c = 0; c < cadence_count; c++) {
        if (cadences[c].arrival_first < entry_count) {
            evidence->arrivals[evidence->arrival_count++] = entry_time(&entries[cadences[c].arrival_first]);
        }
    }
    return true;
}

/* floor and ceiling of a rational */
static Wide rat_floor(Rat a) {
    Wide q = a.num / a.den;
    return (a.num % a.den != 0 && a.num < 0) ? q - 1 : q;
}

static Wide rat_ceil(Rat a) {
    return -rat_floor((Rat){-a.num, a.den});
}

/* How many downbeats the grid has from time from to time to, both inclusive; at least 1. */
static Wide downbeats_between(const Grid *grid, Rat from, Rat to) {
    Wide count = 1;
    if (!grid->map) {
        Rat first = rat_div(rat_sub(from, grid->offset), grid->measure);
        Rat last = rat_div(rat_sub(to, grid->offset), grid->measure);
        count = rat_floor(last) - rat_ceil(first) + 1;
    } else {
        int measure_from = 0;
        int measure_to = 0;
        MusicalTime *a = NULL;
        MusicalTime *b = NULL;
        if (musical_time_create((long)from.num, (unsigned long)from.den, &a) == MUSICAL_TIME_OK &&
            musical_time_create((long)to.num, (unsigned long)to.den, &b) == MUSICAL_TIME_OK &&
            meter_map_measure_index_at(grid->map, a, &measure_from) == METER_MAP_OK &&
            meter_map_measure_index_at(grid->map, b, &measure_to) == METER_MAP_OK) {
            count = (Wide)measure_to - (Wide)measure_from + 1;
        }
        musical_time_destroy(a);
        musical_time_destroy(b);
    }
    return count < 1 ? 1 : count;
}

/* The score of a grid in [0, 1]. */
static double grid_score(const Grid *grid, const Evidence *evidence) {
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
            grid_place(grid, evidence->changes[i], &downbeat, &beat, &secondary);
            on_down += downbeat ? 1 : 0;
            on_accent += downbeat ? 1.0 : (secondary ? 0.5 : 0.0);
            on_beat += (downbeat || beat) ? 1 : 0;
        }
        /* The downbeat share is the harmonic mean of the share of the changes on a downbeat and the share of the
           downbeats that carry a change. Without the second, a grid with a barline at every beat would always win: it
           holds every barline of the real meter and more. */
        double recall = on_accent / (double)evidence->change_count;
        Wide downbeats = downbeats_between(grid, evidence->changes[0], evidence->changes[evidence->change_count - 1]);
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
            grid_place(grid, evidence->arrivals[i], &downbeat, &beat, &secondary);
            on_down += downbeat ? 1.0 : (secondary ? 0.5 : 0.0);
        }
        sum += 3.0 * on_down / (double)evidence->arrival_count;
        weights += 3.0;
    }
    return weights > 0.0 ? sum / weights : 0.0;
}

/* ---- Inter-onset interval ---- */

static int rat_compare_qsort(const void *a, const void *b) {
    return rat_cmp(*(const Rat *)a, *(const Rat *)b);
}

/* The most common gap between consecutive distinct onsets of the notes, the smallest of equally common ones. False
   when there are fewer than two onsets. */
static bool common_interval(const ImportedNotes *notes, Rat *interval, bool *ok) {
    *ok = false;
    if (notes->count < 2) {
        return true;
    }
    Rat *onsets = malloc(notes->count * sizeof(Rat));
    if (!onsets) {
        return false;
    }
    for (size_t i = 0; i < notes->count; i++) {
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(notes->events[i]));
        onsets[i] = time_rat(onset);
    }
    qsort(onsets, notes->count, sizeof(Rat), rat_compare_qsort);
    size_t gaps = 0;
    for (size_t i = 1; i < notes->count; i++) {
        if (rat_cmp(onsets[i], onsets[i - 1]) > 0) {
            onsets[gaps++] = rat_sub(onsets[i], onsets[i - 1]); /* in place: the gap index never passes i */
        }
    }
    if (gaps > 0) {
        qsort(onsets, gaps, sizeof(Rat), rat_compare_qsort);
        size_t best_start = 0;
        size_t best_run = 0;
        for (size_t start = 0; start < gaps;) {
            size_t end = start + 1;
            while (end < gaps && rat_cmp(onsets[end], onsets[start]) == 0) {
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
    return true;
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
    Rat scale;  /* 1/2, 1 or 2 */
    Rat pickup; /* whole notes after scaling */
    Grid grid;
    double score;
    double distance; /* of the most common interval, scaled, from an eighth note; huge when the piece has none */
} Candidate;

static const Rat SCALES[] = {{1, 2}, {1, 1}, {2, 1}};

static bool same_grid(const Grid *a, const Grid *b) {
    return rat_cmp(a->measure, b->measure) == 0 && rat_cmp(a->beat, b->beat) == 0 &&
           rat_is_integer(rat_div(rat_sub(a->offset, b->offset), a->measure));
}

/* Whether a is the better one to show of two candidates with the same barlines. */
static bool shown_before(const Candidate *a, const Candidate *b) {
    if (a->distance != b->distance) {
        return a->distance < b->distance;
    }
    return a->bottom == 4 && b->bottom == 8;
}

static AnalysisMeterReading reading_of(const Candidate *candidate) {
    AnalysisMeterReading reading;
    memset(&reading, 0, sizeof(reading));
    reading.beats = (int)candidate->top;
    reading.beat_type = (int)candidate->bottom;
    reading.scale_numerator = (long)candidate->scale.num;
    reading.scale_denominator = (unsigned long)candidate->scale.den;
    reading.pickup_numerator = (long)candidate->pickup.num;
    reading.pickup_denominator = (unsigned long)candidate->pickup.den;
    reading.score = candidate->score;
    return reading;
}

/* The effective reading from the notes' meter map: its first full measure's signature, scale 1, the length of a
   leading pickup region as the pickup. The grid is the map itself. False when the map has no full measure. */
static bool effective_candidate(const ImportedNotes *notes, Candidate *effective, Grid *barlines) {
    const MeterMap *map = notes->meter.meter_map;
    if (!map) {
        return false;
    }
    size_t index = meter_map_has_pickup(map) ? 1 : 0;
    const MeterMapRegion *full = meter_map_region(map, index);
    if (!full) {
        return false;
    }
    const TimeSignature *signature = meter_map_region_time_signature(full);
    unsigned top = time_signature_top_number(signature);
    unsigned bottom = time_signature_bottom_number(signature);
    Rat pickup = {0, 1};
    if (index == 1) {
        const MusicalTime *end = meter_map_region_end(meter_map_region(map, 0));
        if (end) {
            pickup = time_rat(end);
        }
    }
    memset(effective, 0, sizeof(*effective));
    effective->top = top;
    effective->bottom = bottom;
    effective->scale = (Rat){1, 1};
    effective->pickup = pickup;
    effective->grid.map = map;
    /* the first full measure's grid, for telling equal candidates apart */
    barlines->map = NULL;
    barlines->measure = rat_make((Wide)top, (Wide)bottom);
    barlines->beat = beat_length(top, bottom);
    barlines->offset = pickup;
    effective->grid.four_beats = four_beats(top, bottom);
    return true;
}

bool meter_check_run(const ImportedNotes *notes, const AnalysisReportEntry *entries, size_t entry_count,
                     const AnalysisCadence *cadences, size_t cadence_count, AnalysisMeterCheck *out) {
    memset(out, 0, sizeof(*out));
    if (entry_count == 0) {
        return true;
    }
    Candidate effective;
    Grid effective_barlines;
    if (!effective_candidate(notes, &effective, &effective_barlines)) {
        return true;
    }
    Evidence evidence;
    if (!evidence_collect(entries, entry_count, cadences, cadence_count, &evidence)) {
        return false;
    }
    Rat interval = {0, 1};
    bool has_interval = false;
    if (!common_interval(notes, &interval, &has_interval)) {
        evidence_destroy(&evidence);
        return false;
    }
    effective.score = grid_score(&effective.grid, &evidence);

    Candidate *best = NULL;
    Candidate best_value;
    const Rat eighth = {1, 8};
    for (size_t m = 0; m < METER_COUNT; m++) {
        unsigned top = METERS[m].top;
        unsigned bottom = METERS[m].bottom;
        Rat beat = beat_length(top, bottom);
        size_t beats = signature_is_compound(top, bottom) ? top / 3 : top;
        for (size_t s = 0; s < sizeof(SCALES) / sizeof(SCALES[0]); s++) {
            Rat scale = SCALES[s];
            for (size_t k = 0; k < beats; k++) {
                Candidate candidate;
                memset(&candidate, 0, sizeof(candidate));
                candidate.top = top;
                candidate.bottom = bottom;
                candidate.scale = scale;
                candidate.pickup = rat_mul(beat, (Rat){(Wide)k, 1});
                candidate.grid.measure = rat_div(rat_make((Wide)top, (Wide)bottom), scale);
                candidate.grid.beat = rat_div(beat, scale);
                candidate.grid.offset = rat_div(candidate.pickup, scale);
                candidate.grid.four_beats = four_beats(top, bottom);
                if (rat_cmp(candidate.grid.measure, effective_barlines.beat) <= 0) {
                    continue; /* a bar no longer than the beat already counted only adds barlines */
                }
                if (same_grid(&candidate.grid, &effective_barlines)) {
                    continue;
                }
                candidate.distance = has_interval ? fabs(rat_double(rat_sub(rat_mul(interval, scale), eighth))) : 1e9;
                candidate.score = grid_score(&candidate.grid, &evidence);
                if (!best) {
                    best_value = candidate;
                    best = &best_value;
                } else if (candidate.score > best->score) {
                    best_value = candidate;
                } else if (candidate.score == best->score && same_grid(&candidate.grid, &best->grid) &&
                           shown_before(&candidate, best)) {
                    best_value = candidate;
                }
            }
        }
    }
    evidence_destroy(&evidence);

    out->present = true;
    out->effective = reading_of(&effective);
    if (best) {
        out->has_other = true;
        out->other = reading_of(best);
        double margin = best->score - effective.score;
        out->has_alternative = margin >= METER_CHECK_MIN_MARGIN;
        double size = fabs(margin);
        out->uncertainty = size >= 1.0 ? 0.0 : 1.0 - size;
    }
    return true;
}

bool meter_check_stage_collect(const ImportedNotes *notes, AnalysisReport *report) {
    return meter_check_run(notes, report->entries, report->entry_count, report->cadences, report->cadence_count,
                           &report->meter_check);
}
