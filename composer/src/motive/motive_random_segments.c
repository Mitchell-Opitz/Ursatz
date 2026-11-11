#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_develop.h>

#include "motive/motive_random_internal.h"

/*
 * Random motives from segments (docs/cli-random-motive.md): a motive is made of one to three segments (Schoenberg's
 * motives), the way a theme starts. Segment a is a rhythm cell with one distinctive feature; a' repeats a's rhythm and
 * contour on the next chord (the dominant form); b is a new cell. A harmony plan puts one chord on each segment, and the
 * last segment slows down to end on a chord tone of the last chord. The defaults below are the whole of what shapes a
 * segment motive; tune them here.
 */

#define SEGMENT_MIN_CELL_BEATS 2 /* a cell must hold a feature: the shortest cell is two beats */
#define TICKS_PER_UNIT 12        /* ticks in the meter's beat unit: halves, thirds and quarters of it are whole */
#define SEGMENT_MAX 3
#define CHORD_MAX MOTIVE_RANDOM_PLAN_MAX /* the chords of a motive: one per segment, or a given plan's */
#define SEGMENT_TRIES_PER_SETUP 8 /* notes drawn within one bars, shape and plan before another is drawn (a candidate) */
#define SEGMENT_CELL_TRIES 32 /* a cell that does not fit is drawn again, so a failed cell does not change the shape's odds */
#define SEGMENT_START_REACH 4 /* rungs either side of the note before, searched for where a new segment starts */
#define SEGMENT_START_DISTANCE 5 /* the most semitones from the note before to where a new segment starts */
#define LEAP_SEMITONES 5 /* an interval this large or larger is a leap (leaps_per_motif, leap_recovery) */
#define SKIP_SEMITONES 3 /* an interval of 3 or 4 semitones is a skip (skips_per_motif) */

/* Syncopation figures (R-454): the two inside a cell, then the two across a segment boundary. */
typedef enum { SYNC_OFF_BEAT = 0, SYNC_TIE_BEAT, SYNC_TIE_BAR, SYNC_ANTICIPATION, SYNC_TYPES } Sync;
#define SYNC_LEVELS 2 /* eighth, sixteenth */

/* Shapes: a a' (a cell and its repeat), a b (a cell and a new one), a a' b (two half-bar cells, then a bar). */
typedef enum { SHAPE_REPEAT = 0, SHAPE_CONTRAST, SHAPE_REPEAT_CONTRAST, SHAPE_KINDS } Shape;

/* Harmony plans, one chord (a degree) per segment, and how often each is drawn (ch. V). */
typedef struct {
    size_t count;
    int degrees[SEGMENT_MAX];
} Plan;
static const Plan PLANS_TWO[] = {{2, {1, 5, 0}}, {2, {1, 4, 0}}, {2, {1, 1, 0}}};
static const Plan PLANS_THREE[] = {{3, {1, 5, 1}}, {3, {1, 4, 1}}};
#define PLAN_KINDS_TWO (sizeof(PLANS_TWO) / sizeof(PLANS_TWO[0]))
#define PLAN_KINDS_THREE (sizeof(PLANS_THREE) / sizeof(PLANS_THREE[0]))

/* The distinctive feature of segment a. */
typedef enum { FEATURE_DOTTED = 0, FEATURE_LEAP, FEATURE_REPEAT, FEATURE_KINDS } Feature;

/* How the last segment ends (the endings list, R-440). */
typedef enum { ENDING_FREE = 0, ENDING_CLOSED, ENDING_OPEN, ENDING_LEAD_IN, ENDING_KINDS } Ending;

/*
 * The tick grid of a meter: the bar's beats, the ticks in a beat and in half of it, and whether the beat is a dotted unit.
 * Simple meters beat once per beat unit (12 ticks); a compound meter with compound_beats beats once per three. It also
 * carries the ending drawn for the motive and `last_offset`, the ticks from its barline to where the last segment's cell
 * starts (draw_setup sets both).
 */
typedef struct {
    long beats;
    long beat;
    long half;
    bool compound;
    Ending ending;
    long last_offset;
} Grid;

static Grid grid_of(const MotiveRandomPreferences *preferences, const PlanMeter *meter) {
    long bar = (long)meter->beats_per_bar;
    if (preferences->compound_beats && (bar == 6 || bar == 9 || bar == 12)) {
        Grid grid = {bar / 3, 3 * TICKS_PER_UNIT, TICKS_PER_UNIT, true, ENDING_FREE, 0};
        return grid;
    }
    Grid grid = {bar, TICKS_PER_UNIT, TICKS_PER_UNIT / 2, false, ENDING_FREE, 0};
    return grid;
}

/* Syncopation needs a simple meter: not a compound grid and not 6, 9 or 12 beats a bar. */
static bool simple_meter(const Grid *grid) {
    return !grid->compound && (grid->beats == 3 || grid->beats % 3 != 0);
}

/* The short note of a figure: an eighth (a half beat) or a sixteenth (a quarter beat), 6 or 3 ticks. */
static long sync_unit(const Grid *grid, size_t level) {
    return level == 0 ? grid->half : grid->half / 2;
}

/* ---------------------------------------------------------------- helpers */

static size_t pick_weighted(FillRng *rng, const unsigned long *weights, size_t count) {
    unsigned long sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += weights[i];
    }
    if (sum == 0) {
        return 0;
    }
    unsigned long pick = (unsigned long)fill_rng_below(rng, sum);
    size_t chosen = 0;
    while (pick >= weights[chosen]) {
        pick -= weights[chosen++];
    }
    return chosen;
}

/* The defaults of the SEGMENTS strategy: what its draws were tuned to. */
void motive_random_preferences_default(MotiveRandomPreferences *out) {
    memset(out, 0, sizeof(*out));
    out->bars[0] = 1; /* a motive of one bar ... */
    out->bars[1] = 1; /* ... and of two bars, as often */
    out->pickup_one_in = 4; /* a pickup (half a beat or a beat), one note before segment a, in one motive of this many */
    out->shapes_one_bar[0] = 3;
    out->shapes_one_bar[1] = 2;
    out->shapes_two_bars[0] = 3;
    out->shapes_two_bars[1] = 2;
    out->shapes_two_bars[2] = 2;
    out->plans_two[0] = 3;
    out->plans_two[1] = 1;
    out->plans_two[2] = 1;
    out->plans_three[0] = 2;
    out->plans_three[1] = 1;
    for (size_t i = 0; i < 3; i++) {
        out->features[i] = 1; /* drawn with equal weight */
    }
    out->leap_low = 5; /* a leap of a fourth (5 semitones) ... */
    out->leap_high = 9; /* ... to a sixth (9 semitones) */
    out->rung_moves[0] = 6; /* one rung most often ... */
    out->rung_moves[1] = 2; /* ... two now and then */
    for (size_t i = 0; i < 5; i++) {
        out->decorations[i] = 1;
    }
    for (size_t i = 0; i < DURATION_KINDS; i++) {
        out->durations[i] = MOTIVE_RANDOM_DURATION_WEIGHTS[i];
    }
    /* The listening settings (R-420 to R-422), all off. */
    for (size_t i = 0; i < 3; i++) {
        out->accented_types[i] = 1;
    }
    out->leaps_per_motif = MAX_NOTES - 1; /* no motive has more intervals, so this never rejects */
    out->contrast_fidelity_min.denominator = 1;
    /* Endings and long non-chord tones (R-440, R-441), all off. */
    out->endings[0] = 1; /* free, as drawn */
    for (size_t i = 0; i < 5; i++) {
        out->long_nct_types[i] = 1;
    }
    for (size_t i = 0; i < 4; i++) {
        out->long_nct_intervals[i] = 1;
    }
    for (size_t i = 0; i < 3; i++) {
        out->long_nct_beats[i] = 1;
        out->long_nct_lengths[i] = 1;
    }
    /* Syncopation (phase L3, R-454), off. */
    for (size_t i = 0; i < 4; i++) {
        out->syncopation_types[i] = 1;
    }
    for (size_t i = 0; i < 2; i++) {
        out->syncopation_levels[i] = 1;
    }
    out->skips_per_motif = MAX_NOTES - 1; /* no motive has more intervals, so this never rejects */
}

/*
 * The scale-step offset of a chord's tones from the tones of I (same octave): ii 1, iii 2, IV 3, V down a fourth (-3), vi -2,
 * vii -1. The tones are the diatonic triad of the degree; in minor only V's third is raised (chord_alteration).
 */
static long chord_shift(int degree) {
    long shift = degree - 1;
    return shift > 3 ? shift - 7 : shift;
}

static long step_of(long position) {
    return ((position % 7) + 7) % 7;
}

/* The alteration of a position on the chord of a degree: in minor, the third of V is raised. */
static int chord_alteration(const AnalysisKey *key, int degree, long position) {
    return key->mode == KEY_STRUCTURE_MODE_MINOR && degree == 5 && step_of(position - chord_shift(degree)) == 2 ? 1 : 0;
}

typedef struct {
    long ticks[MAX_NOTES];
    size_t count;
} Cell;

/*
 * The ticks from one strong beat to the next: the bar's beats in threes in a compound meter, in twos in an even one, else
 * the whole bar. A beat that is not a multiple of it is weak (cells start at half bars or bars, so a position from the
 * cell start decides).
 */
static long weak_beat_unit(const Grid *grid) {
    return (grid->beats % 3 == 0 ? 3 : grid->beats % 2 == 0 ? 2 : grid->beats) * grid->beat;
}

/* True when the last note of the cell is longer than each of the others. */
static bool last_is_longest(const Cell *cell) {
    for (size_t i = 0; i + 1 < cell->count; i++) {
        if (cell->ticks[i] >= cell->ticks[cell->count - 1]) {
            return false;
        }
    }
    return true;
}

/* The metric level of a position in ticks from the barline: 3 on the downbeat, 2 on a strong beat, 1 on another beat. */
static int metric_level(const Grid *grid, long position) {
    if (position % (grid->beats * grid->beat) == 0) {
        return 3;
    }
    return position % weak_beat_unit(grid) == 0 ? 2 : 1;
}

/*
 * A closed ending: where the last note of a cell of `length` ticks starts. Among the beats p with beat <= p <= length - beat,
 * those of the highest metric level (counted from the barline), one by `fill_rng_below`. False when there is none.
 */
static bool closed_start(const Grid *grid, FillRng *rng, long length, long *start) {
    long best[16];
    size_t best_count = 0;
    int level = 0;
    for (long p = grid->beat; p <= length - grid->beat; p += grid->beat) {
        int here = metric_level(grid, grid->last_offset + p);
        if (here > level) {
            level = here;
            best_count = 0;
        }
        if (here == level && best_count < sizeof(best) / sizeof(best[0])) {
            best[best_count++] = p;
        }
    }
    if (best_count == 0) {
        return false;
    }
    *start = best[fill_rng_below(rng, best_count)];
    return true;
}

/* A lead-in ending: the cell's last two notes are each at most a half beat and the last starts off the beat. */
static bool lead_in_end(const Grid *grid, long length, const Cell *cell) {
    if (cell->count < 2 || cell->ticks[cell->count - 1] > grid->half || cell->ticks[cell->count - 2] > grid->half) {
        return false;
    }
    return (length - cell->ticks[cell->count - 1]) % grid->beat != 0;
}

/* The entries of the durations list. */
#define FIGURE_KINDS 8
#define FIGURE_NOTES_MAX 3

/* The notes of one entry of the durations list and where it may start. False when the grid has no such figure. */
typedef struct {
    long ticks[FIGURE_NOTES_MAX];
    size_t count;
    long total;
    bool on_beat_only;
} Figure;

static bool figure_of(const Grid *grid, size_t kind, Figure *figure) {
    long beat = grid->beat;
    memset(figure, 0, sizeof(*figure));
    figure->on_beat_only = true;
    switch (kind) {
    case 0: /* half_beat: anywhere */
        figure->ticks[0] = grid->half;
        figure->count = 1;
        figure->on_beat_only = false;
        break;
    case 1: /* beat */
        figure->ticks[0] = beat;
        figure->count = 1;
        break;
    case 2: /* two_beats */
        figure->ticks[0] = 2 * beat;
        figure->count = 1;
        break;
    case 3: /* quarter_beats: two notes filling a half beat, anywhere a half beat may start */
        figure->ticks[0] = grid->half / 2;
        figure->ticks[1] = grid->half / 2;
        figure->count = 2;
        figure->on_beat_only = false;
        break;
    case 4: /* long_short: a dotted pair, three quarters then one quarter of the beat (two thirds, one third in a compound beat) */
    case 5: /* short_long: the reverse */
        figure->ticks[kind == 4 ? 0 : 1] = grid->compound ? 2 * beat / 3 : 3 * beat / 4;
        figure->ticks[kind == 4 ? 1 : 0] = grid->compound ? beat / 3 : beat / 4;
        figure->count = 2;
        break;
    case 6: /* triplet: three notes in a beat (simple grid only) */
    case 7: /* triplet_two_beats: three notes in two beats (simple grid only) */
        if (grid->compound) {
            return false;
        }
        for (size_t i = 0; i < 3; i++) {
            figure->ticks[i] = (kind == 6 ? 1 : 2) * beat / 3;
        }
        figure->count = 3;
        break;
    default:
        return false;
    }
    for (size_t i = 0; i < figure->count; i++) {
        figure->total += figure->ticks[i];
    }
    return true;
}

/* The ticks of a syncopation figure inside a cell: off_beat four short notes long, tie_beat eight. */
static long sync_length(const Grid *grid, size_t type, size_t level) {
    return (type == SYNC_TIE_BEAT ? 8 : 4) * sync_unit(grid, level);
}

/* The notes of a figure inside a cell (u is the short note): the count. Every figure starts on a beat and ends on a beat or a half beat. */
static size_t sync_figure(const Grid *grid, size_t type, size_t level, size_t shape, long *ticks) {
    long u = sync_unit(grid, level);
    if (type == SYNC_TIE_BEAT) {
        ticks[0] = 3 * u;
        ticks[1] = 3 * u;
        ticks[2] = 2 * u;
        return 3;
    }
    if (shape == 0) {
        ticks[0] = u;
        ticks[1] = 2 * u;
        ticks[2] = u;
        return 3;
    }
    ticks[0] = u;
    ticks[1] = 3 * u;
    return 2;
}

/*
 * Where in a cell of `length` ticks a syncopation is tried: -1 for none, else the tick from which the first beat that holds a
 * figure takes it. One cell in syncopation_one_in; the beat is drawn among those from which the shortest wanted figure fits.
 * clean_end keeps the figure off the cell's last note. No random number is drawn when nothing can be placed.
 */
static long sync_target(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long length, bool clean_end) {
    if (preferences->syncopation_one_in == 0 || !simple_meter(grid)) {
        return -1;
    }
    long shortest = 0;
    for (size_t type = SYNC_OFF_BEAT; type <= SYNC_TIE_BEAT; type++) {
        for (size_t level = 0; level < SYNC_LEVELS; level++) {
            if (preferences->syncopation_types[type] * preferences->syncopation_levels[level] > 0) {
                long ticks = sync_length(grid, type, level);
                if (shortest == 0 || ticks < shortest) {
                    shortest = ticks;
                }
            }
        }
    }
    if (shortest == 0) {
        return -1;
    }
    long beats = 0;
    for (long p = 0; clean_end ? p + shortest < length : p + shortest <= length; p += grid->beat) {
        beats++;
    }
    if (beats == 0 || fill_rng_below(rng, preferences->syncopation_one_in) != 0) {
        return -1;
    }
    return grid->beat * (long)fill_rng_below(rng, (uint64_t)beats);
}

/*
 * Adds a drawn syncopation figure that fits in `room` ticks (and, with clean_end, leaves a note after it) to the cell.
 * `added` is its ticks, 0 when none fits (no random number is drawn). False when the cell is out of notes.
 */
static bool try_sync_figure(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long room, bool clean_end, Cell *cell,
                            long *added) {
    unsigned long weights[2 * SYNC_LEVELS];
    unsigned long total = 0;
    for (size_t type = SYNC_OFF_BEAT; type <= SYNC_TIE_BEAT; type++) {
        for (size_t level = 0; level < SYNC_LEVELS; level++) {
            long ticks = sync_length(grid, type, level);
            bool fits = ticks <= room && !(clean_end && ticks == room);
            weights[type * SYNC_LEVELS + level] = fits ? preferences->syncopation_types[type] * preferences->syncopation_levels[level] : 0;
            total += weights[type * SYNC_LEVELS + level];
        }
    }
    *added = 0;
    if (total == 0) {
        return true;
    }
    size_t pick = pick_weighted(rng, weights, 2 * SYNC_LEVELS);
    size_t type = pick / SYNC_LEVELS;
    size_t level = pick % SYNC_LEVELS;
    size_t shape = type == SYNC_OFF_BEAT ? (size_t)fill_rng_below(rng, 2) : 0;
    long ticks[FIGURE_NOTES_MAX];
    size_t count = sync_figure(grid, type, level, shape, ticks);
    if (cell->count + count > MAX_NOTES) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        cell->ticks[cell->count++] = ticks[i];
    }
    *added = sync_length(grid, type, level);
    return true;
}

/*
 * Tiles from to to (ticks from the start of the cell) with the figures of the durations list. At the first beat from
 * `*sync_at` (when it is not -1) that holds one, a syncopation figure goes in instead and `*sync_at` becomes -1.
 */
static bool tile(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long from, long to, bool clean_end, long *sync_at,
                 Cell *cell) {
    long unit = weak_beat_unit(grid);
    Figure figures[FIGURE_KINDS];
    bool available[FIGURE_KINDS];
    for (size_t i = 0; i < FIGURE_KINDS; i++) {
        available[i] = figure_of(grid, i, &figures[i]);
    }
    for (long position = from; position < to;) {
        long room = to - position;
        bool on_beat = position % grid->beat == 0;
        if (on_beat && *sync_at >= 0 && position >= *sync_at) {
            long added;
            if (!try_sync_figure(preferences, grid, rng, room, clean_end, cell, &added)) {
                return false;
            }
            if (added > 0) {
                *sync_at = -1;
                position += added;
                continue;
            }
        }
        unsigned long weights[FIGURE_KINDS];
        for (size_t i = 0; i < FIGURE_KINDS; i++) {
            bool fits = available[i] && figures[i].total <= room && (on_beat || !figures[i].on_beat_only);
            weights[i] = fits ? preferences->durations[i] : 0;
        }
        if (on_beat && position % unit != 0) {
            weights[2] *= 1 + preferences->long_on_weak; /* the two-beat note on a weak beat (long_on_weak) */
        }
        const Figure *chosen = &figures[pick_weighted(rng, weights, FIGURE_KINDS)];
        if (cell->count + chosen->count > MAX_NOTES) {
            return false;
        }
        for (size_t i = 0; i < chosen->count; i++) {
            cell->ticks[cell->count++] = chosen->ticks[i];
        }
        position += chosen->total;
    }
    return true;
}

/*
 * Segment a's cell of length ticks for a feature: a dotted figure on a beat, or a cell with two notes on beats. When the cell
 * is the last one (long_end) and the ending is closed, its last note starts on the strongest beat available and is longer
 * than every other; with a lead-in it ends in two short notes, the last off the beat.
 */
static bool try_cell_a(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long length, Feature feature, bool long_end,
                       bool syncopate, Cell *cell) {
    memset(cell, 0, sizeof(*cell));
    bool closed = long_end && grid->ending == ENDING_CLOSED;
    long body = length; /* the ticks tiled before a closed ending's last note */
    if (closed && !closed_start(grid, rng, length, &body)) {
        return false;
    }
    long sync_at = syncopate ? sync_target(preferences, grid, rng, body, long_end && !closed) : -1;
    if (feature == FEATURE_DOTTED) {
        /* A dotted beat and the half beat that completes the next one (two beats); in a compound beat, a dotted eighth,
           a sixteenth and an eighth in 6/8 (one beat). */
        long dotted[3] = {3 * grid->half, grid->half, 0};
        size_t dotted_count = 2;
        if (grid->compound) {
            dotted[0] = grid->beat / 2;
            dotted[1] = grid->beat / 6;
            dotted[2] = grid->beat / 3;
            dotted_count = 3;
        }
        long figure = 0;
        for (size_t i = 0; i < dotted_count; i++) {
            figure += dotted[i];
        }
        long slots = (body - figure) / grid->beat + 1;
        if (slots < 1) {
            return false;
        }
        long start = grid->beat * (long)fill_rng_below(rng, (uint64_t)slots);
        if (!tile(preferences, grid, rng, 0, start, false, &sync_at, cell) || cell->count + dotted_count > MAX_NOTES) {
            return false;
        }
        for (size_t i = 0; i < dotted_count; i++) {
            cell->ticks[cell->count++] = dotted[i];
        }
        if (!tile(preferences, grid, rng, start + figure, body, long_end && !closed, &sync_at, cell)) {
            return false;
        }
        if (closed) {
            if (cell->count == MAX_NOTES) {
                return false;
            }
            cell->ticks[cell->count++] = length - body;
        }
    } else {
        if (!tile(preferences, grid, rng, 0, body, long_end && !closed, &sync_at, cell)) {
            return false;
        }
        if (closed) {
            if (cell->count == MAX_NOTES) {
                return false;
            }
            cell->ticks[cell->count++] = length - body;
        }
        size_t on_beats = 0;
        long position = 0;
        for (size_t i = 0; i < cell->count; i++) {
            on_beats += position % grid->beat == 0;
            position += cell->ticks[i];
        }
        if (on_beats < 2) {
            return false;
        }
    }
    if (long_end && grid->ending == ENDING_LEAD_IN) {
        if (!lead_in_end(grid, length, cell)) {
            return false;
        }
    } else if (long_end && cell->ticks[cell->count - 1] < grid->beat) {
        return false;
    }
    return !(long_end && (preferences->final_long || closed)) || last_is_longest(cell);
}

/*
 * A new cell for the last segment: tiled, then its last notes joined until the last note is at least a beat long. A closed
 * ending tiles up to the last note's start instead and then adds it, longer than every other note; a lead-in joins nothing
 * and ends in two short notes, the last off the beat.
 */
static bool try_cell_b(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long length, const Cell *not_like, bool syncopate,
                       Cell *cell) {
    memset(cell, 0, sizeof(*cell));
    long sync_at = -1;
    if (grid->ending == ENDING_CLOSED) {
        long start;
        if (!closed_start(grid, rng, length, &start)) {
            return false;
        }
        sync_at = syncopate ? sync_target(preferences, grid, rng, start, false) : -1;
        if (!tile(preferences, grid, rng, 0, start, false, &sync_at, cell) || cell->count == MAX_NOTES) {
            return false;
        }
        cell->ticks[cell->count++] = length - start;
        if (!last_is_longest(cell)) {
            return false;
        }
    } else {
        sync_at = syncopate ? sync_target(preferences, grid, rng, length, true) : -1;
        if (!tile(preferences, grid, rng, 0, length, true, &sync_at, cell)) {
            return false;
        }
        if (grid->ending == ENDING_LEAD_IN) {
            if (!lead_in_end(grid, length, cell)) {
                return false;
            }
        } else {
            while (cell->count >= 2 && cell->ticks[cell->count - 1] < grid->beat) {
                cell->ticks[cell->count - 2] += cell->ticks[cell->count - 1];
                cell->count--;
            }
            if (cell->ticks[cell->count - 1] < grid->beat) {
                return false;
            }
        }
    }
    if (preferences->final_long && (!last_is_longest(cell) || cell->count > not_like->count)) {
        return false;
    }
    return cell->count != not_like->count || memcmp(cell->ticks, not_like->ticks, cell->count * sizeof(long)) != 0;
}

static bool draw_cell_a(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long length, Feature feature, bool long_end, Cell *cell) {
    for (int attempt = 0; attempt < SEGMENT_CELL_TRIES; attempt++) {
        if (try_cell_a(preferences, grid, rng, length, feature, long_end, attempt < SEGMENT_CELL_TRIES / 2, cell)) {
            return true;
        }
    }
    return false;
}

static bool draw_cell_b(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long length, const Cell *not_like, Cell *cell) {
    for (int attempt = 0; attempt < SEGMENT_CELL_TRIES; attempt++) {
        if (try_cell_b(preferences, grid, rng, length, not_like, attempt < SEGMENT_CELL_TRIES / 2, cell)) {
            return true;
        }
    }
    return false;
}

/* ----------------------------------------------------------------- layout */

typedef struct {
    Rhythm rhythm;
    size_t segment_count;
    size_t first[SEGMENT_MAX];
    size_t last[SEGMENT_MAX];
    bool repeats[SEGMENT_MAX]; /* the segment repeats segment a's rhythm */
    Plan plan;
    size_t cell_start; /* the first note of segment a's cell: after the pickup note, if any */
    Grid grid;
    long start[SEGMENT_MAX];     /* ticks from the motive's start, the pickup included, to where each segment's cell starts */
    Sync boundary[SEGMENT_MAX];  /* SYNC_TIE_BAR or SYNC_ANTICIPATION at the start of segment s >= 1, else SYNC_TYPES */
    size_t chord_count;
    int chord_degree[CHORD_MAX];
    long chord_onset[CHORD_MAX]; /* ticks from the motive's start, the pickup included, to where each chord starts */
    int degree[MAX_NOTES];       /* the chord at each note's onset (set_note_chords) */
    bool chord_first[MAX_NOTES]; /* the first note on a chord after the first */
} Layout;

static bool add_cell(Layout *layout, const Grid *grid, const Cell *cell, long *position) {
    for (size_t i = 0; i < cell->count; i++) {
        if (layout->rhythm.count == MAX_NOTES) {
            return false;
        }
        layout->rhythm.on_beat[layout->rhythm.count] = *position % grid->beat == 0;
        layout->rhythm.ticks[layout->rhythm.count++] = cell->ticks[i];
        *position += cell->ticks[i];
    }
    return true;
}

/* What a motive is before its notes: its bars, pickup, shape and harmony plan. Drawn once; the notes are drawn within it. */
typedef struct {
    Grid grid;
    long bars;
    long pickup;
    Shape shape;
    Plan plan;
    bool placed; /* a given plan whose chords are at a bar and beat, not one to a segment */
} Setup;

/* True when a given plan places a chord after the first at a bar and beat (the places are all or none, checked by the refusal). */
static bool plan_placed(const MotiveRandomPreferences *preferences) {
    for (size_t k = 1; k < preferences->plan_count; k++) {
        if (preferences->plan[k].bar > 0) {
            return true;
        }
    }
    return false;
}

/*
 * The weights of the shapes of a motive of `bars` bars, as drawn (the one- or two-bar list), with a shape set to 0 that a given
 * plan cannot fill: an unplaced plan of two chords fills a a' and a b, one of three fills a a' b; one chord or a placed plan
 * fills every shape. Returns how many shapes the list draws from.
 */
static size_t shape_weights(const MotiveRandomPreferences *preferences, long bars, bool halves, unsigned long *weights) {
    size_t count = bars == 1 ? 2 : halves ? SHAPE_KINDS : SHAPE_KINDS - 1;
    const unsigned long *list = bars == 1 ? preferences->shapes_one_bar : preferences->shapes_two_bars;
    bool unplaced = !plan_placed(preferences);
    for (size_t shape = 0; shape < count; shape++) {
        size_t segments = shape == SHAPE_REPEAT_CONTRAST ? 3 : 2;
        bool fits = !unplaced || preferences->plan_count == 1 || preferences->plan_count == segments;
        weights[shape] = fits ? list[shape] : 0;
    }
    return count;
}

/* True when a motive of `bars` bars can hold the given plan: the bars are allowed, a placed chord is in them, a shape fits. */
static bool plan_bars_fit(const MotiveRandomPreferences *preferences, bool halves, long bars) {
    if (bars == 1 && !halves) {
        return false;
    }
    if (halves && preferences->bars[bars - 1] == 0) {
        return false;
    }
    for (size_t k = 1; bars == 1 && k < preferences->plan_count; k++) {
        if (preferences->plan[k].bar == 2) {
            return false;
        }
    }
    unsigned long weights[SHAPE_KINDS];
    size_t count = shape_weights(preferences, bars, halves, weights);
    unsigned long total = 0;
    for (size_t shape = 0; shape < count; shape++) {
        total += weights[shape];
    }
    return total > 0;
}

/* True when a bar splits into two cells: an even number of beats, four or more. */
static bool has_halves(const Grid *grid) {
    return grid->beats % 2 == 0 && grid->beats >= 2 * SEGMENT_MIN_CELL_BEATS;
}

static void draw_setup(const MotiveRandomPreferences *preferences, const PlanMeter *meter, FillRng *rng, Setup *setup) {
    memset(setup, 0, sizeof(*setup));
    setup->grid = grid_of(preferences, meter);
    bool halves = has_halves(&setup->grid);
    bool given = preferences->plan_count > 0;
    setup->placed = given && plan_placed(preferences);
    setup->bars = 2;
    if (halves) {
        unsigned long weights[2] = {preferences->bars[0], preferences->bars[1]};
        for (size_t b = 0; given && b < 2; b++) {
            if (!plan_bars_fit(preferences, halves, (long)b + 1)) {
                weights[b] = 0; /* a given plan does not fit that many bars */
            }
        }
        setup->bars = 1 + (long)pick_weighted(rng, weights, 2);
    }
    setup->pickup = preferences->pickup_one_in > 0 && fill_rng_below(rng, preferences->pickup_one_in) == 0
                        ? (1 + (long)fill_rng_below(rng, 2)) * setup->grid.half
                        : 0;
    if (given) {
        /* A given plan: the shape is drawn among those it fills, and no plan is drawn. The degrees are the chord of each segment;
           a placed plan keeps its first chord on the early segments and its last on the last (the chords themselves are placed
           by set_chord_list). */
        unsigned long shapes[SHAPE_KINDS];
        size_t shape_count = shape_weights(preferences, setup->bars, halves, shapes);
        setup->shape = (Shape)pick_weighted(rng, shapes, shape_count);
        setup->plan.count = setup->shape == SHAPE_REPEAT_CONTRAST ? 3 : 2;
        for (size_t s = 0; s < setup->plan.count; s++) {
            bool own = !setup->placed && preferences->plan_count > 1;
            setup->plan.degrees[s] = preferences->plan[own ? s : 0].degree;
        }
        if (setup->placed) {
            setup->plan.degrees[setup->plan.count - 1] = preferences->plan[preferences->plan_count - 1].degree;
        }
    } else {
        setup->shape = (Shape)(setup->bars == 1 ? pick_weighted(rng, preferences->shapes_one_bar, 2)
                                                : pick_weighted(rng, preferences->shapes_two_bars, halves ? SHAPE_KINDS : SHAPE_KINDS - 1));
        if (setup->shape == SHAPE_REPEAT_CONTRAST) {
            setup->plan = PLANS_THREE[pick_weighted(rng, preferences->plans_three, PLAN_KINDS_THREE)];
        } else {
            setup->plan = PLANS_TWO[pick_weighted(rng, preferences->plans_two, PLAN_KINDS_TWO)];
        }
    }
    /* The ending (R-440), drawn only when one besides free has a weight, so the defaults draw nothing here. */
    if (preferences->endings[1] + preferences->endings[2] + preferences->endings[3] > 0) {
        setup->grid.ending = (Ending)pick_weighted(rng, preferences->endings, ENDING_KINDS);
    }
    long bar = setup->grid.beats * setup->grid.beat;
    long a_length = setup->shape == SHAPE_REPEAT_CONTRAST ? bar / 2 : (setup->bars == 1 ? bar / 2 : bar);
    long last_length = setup->shape == SHAPE_REPEAT_CONTRAST ? bar : a_length;
    setup->grid.last_offset = (setup->bars * bar - last_length) % bar;
}

/*
 * True when the ending still holds with note j at `ticks` (j is the first note of the last segment): a final_long or closed
 * ending keeps its last note longest, a lead-in keeps its last two notes at most a half beat.
 */
static bool ending_kept(const MotiveRandomPreferences *preferences, const Layout *layout, size_t j, long ticks) {
    size_t last = layout->rhythm.count - 1;
    if ((preferences->final_long || layout->grid.ending == ENDING_CLOSED) && ticks >= layout->rhythm.ticks[last]) {
        return false;
    }
    return !(layout->grid.ending == ENDING_LEAD_IN && j + 2 == layout->rhythm.count && ticks > layout->grid.half);
}

/*
 * Syncopation across a segment boundary (R-454): the last note before a barline held across it (tie_bar), or a segment's
 * first note arriving early on a chord change (anticipation). One boundary in syncopation_one_in; note counts do not change.
 */
static void place_boundary_syncopation(const MotiveRandomPreferences *preferences, FillRng *rng, Layout *layout) {
    const Grid *grid = &layout->grid;
    long bar = grid->beats * grid->beat;
    long *ticks = layout->rhythm.ticks;
    for (size_t s = 1; s < layout->segment_count; s++) {
        size_t i = layout->last[s - 1];
        size_t j = layout->first[s];
        if (j + 1 >= layout->rhythm.count || i < layout->cell_start) {
            continue;
        }
        bool barline = (layout->start[s] - layout->rhythm.pickup) % bar == 0;
        bool change = false; /* a chord starts where segment s does, and it is not the chord before */
        for (size_t k = 1; k < layout->chord_count; k++) {
            change = change || (layout->chord_onset[k] == layout->start[s] && layout->chord_degree[k] != layout->chord_degree[k - 1]);
        }
        unsigned long weights[2 * SYNC_LEVELS];
        unsigned long total = 0;
        for (size_t type = SYNC_TIE_BAR; type < SYNC_TYPES; type++) {
            for (size_t level = 0; level < SYNC_LEVELS; level++) {
                long u = sync_unit(grid, level);
                bool fits = type == SYNC_TIE_BAR ? barline && ticks[j] >= 2 * u
                                                 : change && ticks[i] >= 2 * u &&
                                                       (s + 1 != layout->segment_count || ending_kept(preferences, layout, j, ticks[j] + u));
                size_t slot = (type - SYNC_TIE_BAR) * SYNC_LEVELS + level;
                weights[slot] = fits ? preferences->syncopation_types[type] * preferences->syncopation_levels[level] : 0;
                total += weights[slot];
            }
        }
        if (total == 0 || fill_rng_below(rng, preferences->syncopation_one_in) != 0) {
            continue;
        }
        size_t pick = pick_weighted(rng, weights, 2 * SYNC_LEVELS);
        Sync type = (Sync)(SYNC_TIE_BAR + pick / SYNC_LEVELS);
        long u = sync_unit(grid, pick % SYNC_LEVELS);
        if (type == SYNC_TIE_BAR) {
            ticks[i] += u;
            ticks[j] -= u;
        } else {
            ticks[i] -= u;
            ticks[j] += u;
        }
        layout->rhythm.on_beat[j] = false;
        layout->boundary[s] = type;
    }
}

/*
 * The chords of a layout: one per segment on the plan's degrees, each from its segment's cell start (the first from 0), or
 * the chords of a given placed plan.
 */
static void set_chord_list(const MotiveRandomPreferences *preferences, const Setup *setup, Layout *layout) {
    if (setup->placed) {
        /* A given plan's own chords: the first holds the pickup, each later one starts at its bar and beat (grid beats). */
        layout->chord_count = preferences->plan_count;
        for (size_t k = 0; k < layout->chord_count; k++) {
            const MotiveRandomPlanChord *chord = &preferences->plan[k];
            layout->chord_degree[k] = chord->degree;
            layout->chord_onset[k] =
                k == 0 ? 0 : setup->pickup + (((long)chord->bar - 1) * setup->grid.beats + (long)chord->beat - 1) * setup->grid.beat;
        }
        return;
    }
    layout->chord_count = setup->plan.count;
    for (size_t s = 0; s < layout->chord_count; s++) {
        layout->chord_degree[s] = setup->plan.degrees[s];
        layout->chord_onset[s] = s == 0 ? 0 : layout->start[s];
    }
}

/*
 * The chord at each note: the last chord whose onset is at or before the note's, except that an anticipated note (the first
 * of a segment that arrives early) takes the chord that starts where its segment's cell does, so it is still drawn on the
 * new chord's tone. chord_first marks the first note on each chord after the first.
 */
static void set_note_chords(Layout *layout) {
    long at = 0;
    size_t previous = 0;
    for (size_t i = 0; i < layout->rhythm.count; i++) {
        size_t chord = 0;
        while (chord + 1 < layout->chord_count && layout->chord_onset[chord + 1] <= at) {
            chord++;
        }
        for (size_t s = 1; s < layout->segment_count; s++) {
            if (layout->first[s] == i && layout->boundary[s] == SYNC_ANTICIPATION) {
                for (size_t k = 0; k < layout->chord_count; k++) {
                    if (layout->chord_onset[k] == layout->start[s]) {
                        chord = k;
                    }
                }
            }
        }
        layout->degree[i] = layout->chord_degree[chord];
        layout->chord_first[i] = i > 0 && chord != previous;
        previous = chord;
        at += layout->rhythm.ticks[i];
    }
}

/* The rhythm of every segment of a setup. False when this draw does not fit. */
static bool draw_layout(const MotiveRandomPreferences *preferences, const Setup *setup, FillRng *rng, Layout *layout, Feature *feature_out) {
    memset(layout, 0, sizeof(*layout));
    Shape shape = setup->shape;
    const Grid *grid = &setup->grid;
    long bar = grid->beats * grid->beat;
    layout->grid = *grid;
    layout->rhythm.ticks_per_unit = TICKS_PER_UNIT;
    layout->rhythm.pickup = setup->pickup;
    long a_length = shape == SHAPE_REPEAT_CONTRAST ? bar / 2 : (setup->bars == 1 ? bar / 2 : bar);
    long last_length = shape == SHAPE_REPEAT_CONTRAST ? bar : a_length;
    layout->segment_count = shape == SHAPE_REPEAT_CONTRAST ? 3 : 2;
    layout->plan = setup->plan;
    if (a_length < SEGMENT_MIN_CELL_BEATS * grid->beat) {
        return false;
    }
    /* The feature is drawn by its weight; one that cannot fit the cell in this shape (a dotted figure in a cell that
       must end on a beat) gives way to the next with a weight, so the shape's odds hold. */
    Feature feature = (Feature)pick_weighted(rng, preferences->features, FEATURE_KINDS);
    Cell a;
    size_t tried = 0;
    while (tried < FEATURE_KINDS && !draw_cell_a(preferences, grid, rng, a_length, feature, shape == SHAPE_REPEAT, &a)) {
        Feature next = feature;
        for (size_t step = 1; step <= FEATURE_KINDS; step++) {
            Feature candidate = (Feature)((feature + step) % FEATURE_KINDS);
            if (preferences->features[candidate] > 0) {
                next = candidate;
                break;
            }
        }
        feature = next;
        tried++;
    }
    if (tried == FEATURE_KINDS) {
        return false;
    }
    *feature_out = feature;
    long position = 0;
    if (layout->rhythm.pickup > 0) {
        layout->rhythm.on_beat[0] = layout->rhythm.pickup % grid->beat == 0;
        layout->rhythm.ticks[0] = layout->rhythm.pickup;
        layout->rhythm.count = 1;
    }
    layout->cell_start = layout->rhythm.count;
    layout->first[0] = 0;
    for (size_t s = 0; s < SEGMENT_MAX; s++) {
        layout->boundary[s] = SYNC_TYPES;
    }
    if (!add_cell(layout, grid, &a, &position)) {
        return false;
    }
    layout->last[0] = layout->rhythm.count - 1;
    if (shape == SHAPE_REPEAT || shape == SHAPE_REPEAT_CONTRAST) {
        layout->first[1] = layout->rhythm.count;
        layout->repeats[1] = true;
        layout->start[1] = layout->rhythm.pickup + position;
        if (!add_cell(layout, grid, &a, &position)) {
            return false;
        }
        layout->last[1] = layout->rhythm.count - 1;
    }
    if (shape != SHAPE_REPEAT) {
        Cell b;
        size_t index = shape == SHAPE_CONTRAST ? 1 : 2;
        if (!draw_cell_b(preferences, grid, rng, last_length, &a, &b)) {
            return false;
        }
        layout->first[index] = layout->rhythm.count;
        layout->start[index] = layout->rhythm.pickup + position;
        if (!add_cell(layout, grid, &b, &position)) {
            return false;
        }
        layout->last[index] = layout->rhythm.count - 1;
    }
    layout->rhythm.total = layout->rhythm.pickup + position;
    set_chord_list(preferences, setup, layout);
    if (preferences->syncopation_one_in > 0 && simple_meter(grid)) {
        place_boundary_syncopation(preferences, rng, layout);
    }
    set_note_chords(layout);
    return layout->rhythm.count >= MIN_NOTES;
}

/* ----------------------------------------------------------------- pitch */

typedef struct {
    long position[MAX_NOTES]; /* scale step, shift included */
    long rung[MAX_NOTES];     /* frame notes: the rung on the ladder of I's chord tones */
    bool frame[MAX_NOTES];
    int segment[MAX_NOTES];
    int alteration[MAX_NOTES];
    bool repeat_pair[MAX_NOTES]; /* the second note of the repeat feature's pair, and the matching note of a repeat of segment a */
} Place;

static int pitch_of(const AnalysisKey *key, const Place *place, size_t i) {
    return motive_position_pitch(key, place->position[i], place->alteration[i]);
}

/* A frame note on the ladder of I, moved to the tones of the chord at its onset. In minor, the third of V is raised. */
static bool set_frame(const AnalysisKey *key, const Layout *layout, Place *place, Melody *melody, size_t i, long rung) {
    int degree = layout->degree[i];
    long shift = chord_shift(degree);
    if (rung < 0) {
        return false;
    }
    place->rung[i] = rung;
    place->position[i] = motive_random_rung_position(rung) + shift;
    place->alteration[i] = chord_alteration(key, degree, place->position[i]);
    melody->pitches[i] = pitch_of(key, place, i);
    return motive_random_in_range(melody->pitches[i]);
}

/* The next rung of a walk on the chord tones: one rung most often, two now and then, and a move of two or more comes back. */
static long next_rung(const MotiveRandomPreferences *preferences, FillRng *rng, long rung, int *step_back) {
    int move;
    if (*step_back != 0) {
        move = *step_back;
        *step_back = 0;
    } else {
        int size = 1 + (int)pick_weighted(rng, preferences->rung_moves, 2);
        move = fill_rng_below(rng, 2) == 0 ? size : -size;
        if (size >= BIG_LEAP_RUNGS) {
            *step_back = move > 0 ? -1 : 1;
        }
    }
    return rung + move;
}

/* Segment a's frame: a walk, with the feature (a leap or a repeated note) between two frame notes. */
static bool draw_frame_a(const MotiveRandomPreferences *preferences, const AnalysisKey *key, const Layout *layout, FillRng *rng, Feature feature, Place *place,
                         Melody *melody) {
    size_t frames[MAX_NOTES];
    size_t frame_count = 0;
    for (size_t i = layout->first[0]; i <= layout->last[0]; i++) {
        if (place->frame[i]) {
            frames[frame_count++] = i;
        }
    }
    size_t pair = frame_count > 1 ? (size_t)fill_rng_below(rng, frame_count - 1) : 0;
    long rung = BASE_RUNG + (long)fill_rng_below(rng, TRIAD_SIZE);
    int step_back = 0;
    for (size_t f = 0; f < frame_count; f++) {
        if (f > 0) {
            if (feature == FEATURE_REPEAT && f == pair + 1) {
                /* the same pitch */
                place->repeat_pair[frames[f]] = true;
            } else if (feature == FEATURE_LEAP && f == pair + 1) {
                long options[6];
                size_t option_count = 0;
                int from = melody->pitches[frames[f - 1]];
                for (long move = -3; move <= 3; move++) {
                    if (move == 0 || rung + move < 0) {
                        continue;
                    }
                    int to = motive_position_pitch(key, motive_random_rung_position(rung + move), 0);
                    int leap = abs(to - from);
                    if (leap >= preferences->leap_low && leap <= preferences->leap_high) {
                        options[option_count++] = move;
                    }
                }
                if (option_count == 0) {
                    return false;
                }
                long move = options[fill_rng_below(rng, option_count)];
                rung += move;
                step_back = move > 0 ? -1 : 1;
            } else {
                rung = next_rung(preferences, rng, rung, &step_back);
            }
        }
        if (!set_frame(key, layout, place, melody, frames[f], rung)) {
            return false;
        }
    }
    return true;
}

/*
 * The frame of a repeat: a's rungs, with the contour of a, on the tones of the repeat's own chord. The whole repeat is moved
 * by a number of rungs (an octave is three) so that it starts near the note before it; on the same chord it is moved at
 * least one rung, so it is not a copy.
 */
static bool draw_frame_repeat(const MotiveRandomPreferences *preferences, const AnalysisKey *key, const Layout *layout, FillRng *rng, size_t segment,
                              Place *place, Melody *melody) {
    size_t first = layout->first[segment];
    size_t previous = first - 1;
    while (!place->frame[previous]) {
        previous--;
    }
    int before = melody->pitches[previous];
    bool same_chord = layout->degree[layout->first[segment]] == layout->degree[layout->cell_start];
    long offsets[7];
    size_t offset_count = 0;
    for (long offset = -3; offset <= 3; offset++) {
        if ((same_chord && offset == 0) || place->rung[layout->cell_start] + offset < 0) {
            continue;
        }
        if (set_frame(key, layout, place, melody, first, place->rung[layout->cell_start] + offset) &&
            abs(melody->pitches[first] - before) <= SEGMENT_START_DISTANCE && !(preferences->avoid_repeats && melody->pitches[first] == before)) {
            offsets[offset_count++] = offset;
        }
    }
    if (offset_count == 0) {
        return false;
    }
    long offset = offsets[fill_rng_below(rng, offset_count)];
    size_t from = layout->cell_start;
    for (size_t i = first; i <= layout->last[segment]; i++, from++) {
        if (place->frame[i] && !set_frame(key, layout, place, melody, i, place->rung[from] + offset)) {
            return false;
        }
        place->repeat_pair[i] = place->repeat_pair[from];
    }
    return true;
}

/* The frame of a new segment: it starts on a chord tone near the last frame note before it, then walks. */
static bool draw_frame_new(const MotiveRandomPreferences *preferences, const AnalysisKey *key, const Layout *layout, FillRng *rng, size_t segment, Place *place,
                           Melody *melody) {
    size_t first = layout->first[segment];
    size_t previous = first - 1;
    while (!place->frame[previous]) {
        previous--;
    }
    int before = melody->pitches[previous];
    long centre = place->rung[previous];
    long near_rung[2 * SEGMENT_START_REACH + 1];
    size_t near_count = 0;
    long best_rung = -1;
    int best_distance = 1000;
    for (long rung = centre - SEGMENT_START_REACH; rung <= centre + SEGMENT_START_REACH; rung++) {
        if (!set_frame(key, layout, place, melody, first, rung)) {
            continue;
        }
        int distance = abs(melody->pitches[first] - before);
        if (preferences->avoid_repeats && distance == 0) {
            continue;
        }
        if (distance <= SEGMENT_START_DISTANCE) {
            near_rung[near_count++] = rung;
        }
        if (distance < best_distance) {
            best_distance = distance;
            best_rung = rung;
        }
    }
    if (best_rung < 0) {
        return false;
    }
    long rung = near_count > 0 ? near_rung[fill_rng_below(rng, near_count)] : best_rung;
    int step_back = 0;
    for (size_t i = first; i <= layout->last[segment]; i++) {
        if (!place->frame[i]) {
            continue;
        }
        if (i > first) {
            rung = next_rung(preferences, rng, rung, &step_back);
        }
        if (!set_frame(key, layout, place, melody, i, rung)) {
            return false;
        }
    }
    return true;
}

/* The positions near the note before where a non-chord tone of a kind (1 to 4 of the decorations list) makes its figure. */
static size_t find_figures(const AnalysisKey *key, const Melody *melody, size_t i, long before, long shift, size_t kind, long *options) {
    size_t option_count = 0;
    for (long p = before - OFF_FRAME_REACH; p <= before + OFF_FRAME_REACH; p++) {
        int pitch = motive_position_pitch(key, p, 0);
        if (!motive_random_is_chord_step(p - shift) && motive_random_in_range(pitch) &&
            motive_random_forms(MOTIVE_RANDOM_DECORATIONS[kind - 1], melody->pitches[i - 1], pitch, melody->pitches[i + 1])) {
            options[option_count++] = p;
        }
    }
    return option_count;
}

/* A run of notes between two frame notes, walked by step (draw_run). */
typedef struct {
    const AnalysisKey *key;
    int degree;
    long shift;
    size_t length;       /* the notes of the run */
    long target;         /* the position of the frame note after the run */
    int target_pitch;
    int start_pitch;     /* the pitch of the frame note before the run */
    long path[MAX_NOTES];
    int alteration[MAX_NOTES];
    int pitch[MAX_NOTES];
    unsigned long seen;  /* complete paths counted so far */
    unsigned long pick;  /* the path to keep */
    long chosen[MAX_NOTES];
    int chosen_alteration[MAX_NOTES];
} RunWalk;

/* Counts (and, at index `pick`, keeps) every path of the run's notes from the position `from` at depth d. */
static void walk_run(RunWalk *walk, size_t d, long from, int from_pitch) {
    if (d == walk->length) {
        if (labs(from - walk->target) == 1 && from_pitch != walk->target_pitch) {
            if (walk->seen == walk->pick) {
                memcpy(walk->chosen, walk->path, walk->length * sizeof(long));
                memcpy(walk->chosen_alteration, walk->alteration, walk->length * sizeof(int));
            }
            walk->seen++;
        }
        return;
    }
    for (long move = d == 0 ? -2 : -1; move <= (d == 0 ? 2 : 1); move++) {
        if (move == 0) {
            continue;
        }
        long position = from + move;
        bool chord_tone = motive_random_is_chord_step(position - walk->shift);
        if (labs(move) == 2 && !chord_tone) {
            continue; /* the first move may be two steps, onto a chord tone */
        }
        int alteration = chord_tone ? chord_alteration(walk->key, walk->degree, position) : 0;
        int pitch = motive_position_pitch(walk->key, position, alteration);
        if (!motive_random_in_range(pitch) || pitch == from_pitch) {
            continue;
        }
        walk->path[d] = position;
        walk->alteration[d] = alteration;
        walk_run(walk, d + 1, position, pitch);
    }
}

/*
 * The notes first to last, none of them a frame note, between the frame notes before and after: a path of scale steps from
 * the first to the second in which each move is one step, except that the first may be two steps onto a chord tone of the
 * segment's chord, and the last note is a step from the frame note after. One path is drawn by `fill_rng_below`; none fails
 * the draw.
 */
static bool draw_run(const AnalysisKey *key, const Layout *layout, FillRng *rng, Place *place, Melody *melody, size_t first, size_t last) {
    RunWalk walk;
    memset(&walk, 0, sizeof(walk));
    walk.key = key;
    walk.degree = layout->degree[first];
    walk.shift = chord_shift(walk.degree);
    walk.length = last - first + 1;
    walk.target = place->position[last + 1];
    walk.target_pitch = melody->pitches[last + 1];
    walk.start_pitch = melody->pitches[first - 1];
    walk.pick = ~0UL;
    walk_run(&walk, 0, place->position[first - 1], walk.start_pitch);
    if (walk.seen == 0) {
        return false;
    }
    walk.pick = (unsigned long)fill_rng_below(rng, walk.seen);
    walk.seen = 0;
    walk_run(&walk, 0, place->position[first - 1], walk.start_pitch);
    for (size_t k = 0; k < walk.length; k++) {
        place->position[first + k] = walk.chosen[k];
        place->alteration[first + k] = walk.chosen_alteration[k];
        melody->pitches[first + k] = pitch_of(key, place, first + k);
        melody->run[first + k] = true;
    }
    return true;
}

/*
 * The notes between frame notes: a type is drawn for each (or none), and a note takes a non-chord pitch near the note
 * before it that makes the figure with the notes either side, else a chord tone of its segment's chord near it. Two or more
 * such notes in a row are a run (draw_run), walked by step.
 */
static bool draw_decorations(const MotiveRandomPreferences *preferences, const AnalysisKey *key, const Layout *layout, FillRng *rng, Place *place, Melody *melody) {
    for (size_t i = 0; i < layout->rhythm.count; i++) {
        if (place->frame[i]) {
            continue;
        }
        if (i == 0 || !place->frame[i - 1]) {
            return false;
        }
        size_t last = i;
        while (last + 1 < layout->rhythm.count && !place->frame[last + 1]) {
            last++;
        }
        if (last + 1 == layout->rhythm.count) {
            return false;
        }
        if (last > i) {
            if (!draw_run(key, layout, rng, place, melody, i, last)) {
                return false;
            }
            i = last;
            continue;
        }
        int degree = layout->degree[i];
        long shift = chord_shift(degree);
        long before = place->position[i - 1];
        size_t kind = pick_weighted(rng, preferences->decorations, DECORATION_KINDS + 1);
        long options[2 * OFF_FRAME_REACH + 1];
        size_t option_count = kind > 0 ? find_figures(key, melody, i, before, shift, kind, options) : 0;
        if (option_count == 0 && preferences->avoid_repeats && kind > 0) {
            /* the other kinds, in list order after the one drawn, before a chord tone */
            for (size_t step = 1; step < DECORATION_KINDS && option_count == 0; step++) {
                size_t other = 1 + (kind - 1 + step) % DECORATION_KINDS;
                if (preferences->decorations[other] > 0 && (option_count = find_figures(key, melody, i, before, shift, other, options)) > 0) {
                    kind = other;
                }
            }
        }
        if (option_count > 0) {
            place->position[i] = options[fill_rng_below(rng, option_count)];
            place->alteration[i] = 0;
            melody->decorated[i] = true;
            melody->types[i] = MOTIVE_RANDOM_DECORATIONS[kind - 1];
        } else {
            for (long p = before - 4; p <= before + 4; p++) {
                int alteration = chord_alteration(key, degree, p);
                int pitch = motive_position_pitch(key, p, alteration);
                if (motive_random_is_chord_step(p - shift) && motive_random_in_range(pitch) &&
                    !(preferences->avoid_repeats && (pitch == melody->pitches[i - 1] || pitch == melody->pitches[i + 1]))) {
                    options[option_count++] = p;
                }
            }
            if (option_count == 0) {
                return false;
            }
            place->position[i] = options[fill_rng_below(rng, option_count)];
            place->alteration[i] = chord_alteration(key, degree, place->position[i]);
        }
        melody->pitches[i] = pitch_of(key, place, i);
    }
    return true;
}

/* Note i keeps a non-chord tone `type` at position p, and the note after it takes the chord tone (a position and alteration). */
static void put_accent(const AnalysisKey *key, Place *place, Melody *melody, size_t i, long position, int alteration, NonChordTone type,
                       long target_position, int target_alteration) {
    place->position[i] = position;
    place->alteration[i] = alteration;
    melody->pitches[i] = pitch_of(key, place, i);
    melody->decorated[i] = true;
    melody->types[i] = type;
    place->position[i + 1] = target_position;
    place->alteration[i + 1] = target_alteration;
    place->rung[i + 1] = place->rung[i];
    place->frame[i + 1] = true;
    melody->pitches[i + 1] = pitch_of(key, place, i + 1);
}

/* An appoggiatura or passing tone on the beat: the chord tone moves one note later, and a step beside it comes first. */
static bool place_step_accent(const AnalysisKey *key, const Layout *layout, FillRng *rng, size_t i, NonChordTone type, Place *place, Melody *melody) {
    int degree = layout->degree[i];
    long shift = chord_shift(degree);
    long target = place->position[i];
    int target_pitch = melody->pitches[i];
    long options[2];
    size_t option_count = 0;
    for (long p = target - 1; p <= target + 1; p += 2) {
        int pitch = motive_position_pitch(key, p, 0);
        if (!motive_random_is_chord_step(p - shift) && motive_random_in_range(pitch) &&
            motive_random_forms(type, melody->pitches[i - 1], pitch, target_pitch)) {
            options[option_count++] = p;
        }
    }
    if (option_count == 0) {
        return false;
    }
    long chosen = options[fill_rng_below(rng, option_count)];
    put_accent(key, place, melody, i, chosen, 0, type, target, place->alteration[i]);
    return true;
}

/* A suspension where the chord changes: the note before is held over as a non-chord tone and falls to a chord tone. */
static bool place_suspension(const AnalysisKey *key, const Layout *layout, size_t i, Place *place, Melody *melody) {
    if (i == 0 || layout->degree[i] == layout->degree[i - 1]) {
        return false;
    }
    int degree = layout->degree[i];
    long held = place->position[i - 1];
    long resolution = held - 1;
    int resolution_alteration = chord_alteration(key, degree, resolution);
    int resolution_pitch = motive_position_pitch(key, resolution, resolution_alteration);
    if (motive_random_is_chord_step(held - chord_shift(degree)) || !motive_random_is_chord_step(resolution - chord_shift(degree)) ||
        !motive_random_in_range(resolution_pitch) ||
        !motive_random_forms(NON_CHORD_TONE_SUSPENSION, melody->pitches[i - 1], melody->pitches[i - 1], resolution_pitch)) {
        return false;
    }
    put_accent(key, place, melody, i, held, place->alteration[i - 1], NON_CHORD_TONE_SUSPENSION, resolution, resolution_alteration);
    return true;
}

/*
 * Accented non-chord tones (R-425): a note that starts on a beat becomes an appoggiatura, a passing tone or a suspension
 * that resolves to the next note, which takes the chord tone the accented note displaced. One per segment, and never the
 * pickup, the first note, or the last note of a segment. Each eligible note is chosen in one of `accented_one_in`. A
 * segment that already holds a long non-chord tone (`accented`, place_long_accents) is skipped.
 */
static bool place_accents(const MotiveRandomPreferences *preferences, const AnalysisKey *key, const Layout *layout, FillRng *rng, Place *place,
                          Melody *melody, bool accented[SEGMENT_MAX]) {
    for (size_t i = 1; i + 1 < layout->rhythm.count; i++) {
        size_t s = (size_t)place->segment[i];
        if (!layout->rhythm.on_beat[i] || !place->frame[i] || i < layout->cell_start || !place->frame[i - 1] ||
            place->segment[i + 1] != place->segment[i] || accented[s]) {
            continue;
        }
        if (fill_rng_below(rng, preferences->accented_one_in) != 0) {
            continue;
        }
        size_t kind = pick_weighted(rng, preferences->accented_types, 3);
        bool placed = kind == 2 ? place_suspension(key, layout, i, place, melody)
                                : place_step_accent(key, layout, rng, i, kind == 0 ? NON_CHORD_TONE_APPOGGIATURA : NON_CHORD_TONE_PASSING, place,
                                                    melody);
        accented[s] = accented[s] || placed;
    }
    return true;
}

/* The long non-chord tone types, in the order of the long_nct_types list. */
#define LONG_NCT_TYPES 5
static const NonChordTone LONG_NCT[LONG_NCT_TYPES] = {NON_CHORD_TONE_APPOGGIATURA, NON_CHORD_TONE_SUSPENSION, NON_CHORD_TONE_RETARDATION,
                                                      NON_CHORD_TONE_PASSING, NON_CHORD_TONE_NEIGHBOR};
#define LONG_NCT_CANDIDATES 64

/* One way to make note i a long non-chord tone: its position and alteration, and the chord tone the next note takes. */
typedef struct {
    NonChordTone type;
    long position;
    int alteration;
    long target;
    int target_alteration;
    unsigned long weight;
} LongNct;

/* Adds the figure `type` at position n resolving to r at note i when the rules allow it, weighted by its type and interval. */
static void add_long_nct(const MotiveRandomPreferences *preferences, const AnalysisKey *key, const Layout *layout, const Melody *melody,
                         size_t i, size_t type, long n, int n_alteration, long r, LongNct *out, size_t *count) {
    int degree = layout->degree[i];
    long shift = chord_shift(degree);
    long step = step_of(n - shift);
    if (motive_random_is_chord_step(n - shift) || !motive_random_is_chord_step(r - shift) || *count == LONG_NCT_CANDIDATES) {
        return;
    }
    size_t interval = step == 1 ? 0 : step == 3 ? 1 : step == 5 ? 2 : 3;
    int r_alteration = chord_alteration(key, degree, r);
    int n_pitch = motive_position_pitch(key, n, n_alteration);
    int r_pitch = motive_position_pitch(key, r, r_alteration);
    if (!motive_random_in_range(n_pitch) || !motive_random_in_range(r_pitch) ||
        !motive_random_forms(LONG_NCT[type], melody->pitches[i - 1], n_pitch, r_pitch)) {
        return;
    }
    LongNct *c = &out[(*count)++];
    c->type = LONG_NCT[type];
    c->position = n;
    c->alteration = n_alteration;
    c->target = r;
    c->target_alteration = r_alteration;
    c->weight = preferences->long_nct_types[type] * preferences->long_nct_intervals[interval];
}

/*
 * Long non-chord tones (R-441): in one segment of `long_nct_one_in`, a frame note that starts on a beat and lasts a beat or
 * more becomes an appoggiatura, suspension, retardation, passing tone or neighbor that resolves to the next note. The note
 * is drawn by where it sits in the bar (long_nct_beats) and its length (long_nct_lengths), the figure by its type and its
 * interval above the chord root (long_nct_types, long_nct_intervals). A suspension or retardation holds the note before over
 * where the chord changes. A segment with none eligible, or whose weights are all 0, draws nothing.
 */
static bool place_long_accents(const MotiveRandomPreferences *preferences, const AnalysisKey *key, const Layout *layout, FillRng *rng, Place *place,
                               Melody *melody, bool accented[SEGMENT_MAX]) {
    long bar = layout->grid.beats * layout->grid.beat;
    long onset[MAX_NOTES];
    long position = 0;
    for (size_t i = layout->cell_start; i < layout->rhythm.count; i++) {
        onset[i] = position;
        position += layout->rhythm.ticks[i];
    }
    for (size_t s = 0; s < layout->segment_count; s++) {
        size_t eligible[MAX_NOTES];
        unsigned long weights[MAX_NOTES];
        size_t eligible_count = 0;
        for (size_t i = layout->first[s]; i <= layout->last[s]; i++) {
            long ticks = layout->rhythm.ticks[i];
            if (!layout->rhythm.on_beat[i] || !place->frame[i] || i < layout->cell_start || i < 1 || !place->frame[i - 1] ||
                i + 1 >= layout->rhythm.count || place->segment[i + 1] != (int)s || ticks < layout->grid.beat) {
                continue;
            }
            long at = onset[i] % bar;
            size_t where = at == 0 ? 0 : layout->grid.beats % 2 == 0 && at == bar / 2 ? 1 : 2;
            size_t length = ticks >= 2 * layout->grid.beat ? 2 : ticks >= 3 * layout->grid.beat / 2 ? 1 : 0;
            weights[eligible_count] = preferences->long_nct_beats[where] * preferences->long_nct_lengths[length];
            eligible[eligible_count++] = i;
        }
        if (eligible_count == 0 || fill_rng_below(rng, preferences->long_nct_one_in) != 0) {
            continue;
        }
        unsigned long total = 0;
        for (size_t k = 0; k < eligible_count; k++) {
            total += weights[k];
        }
        if (total == 0) {
            continue;
        }
        size_t i = eligible[pick_weighted(rng, weights, eligible_count)];
        int degree = layout->degree[i];
        LongNct candidates[LONG_NCT_CANDIDATES];
        size_t count = 0;
        for (size_t type = 0; type < LONG_NCT_TYPES; type++) {
            if (type == 1 || type == 2) {
                /* held over from the note before, where the chord changes: down by step, or up (retardation) */
                if (degree != layout->degree[i - 1]) {
                    long held = place->position[i - 1];
                    add_long_nct(preferences, key, layout, melody, i, type, held, place->alteration[i - 1], held + (type == 1 ? -1 : 1), candidates,
                                 &count);
                }
                continue;
            }
            for (long n = place->position[i] - 2; n <= place->position[i] + 2; n++) {
                add_long_nct(preferences, key, layout, melody, i, type, n, 0, n - 1, candidates, &count);
                add_long_nct(preferences, key, layout, melody, i, type, n, 0, n + 1, candidates, &count);
            }
        }
        unsigned long candidate_weights[LONG_NCT_CANDIDATES];
        unsigned long candidate_total = 0;
        for (size_t k = 0; k < count; k++) {
            candidate_weights[k] = candidates[k].weight;
            candidate_total += candidates[k].weight;
        }
        if (candidate_total == 0) {
            continue;
        }
        const LongNct *chosen = &candidates[pick_weighted(rng, candidate_weights, count)];
        put_accent(key, place, melody, i, chosen->position, chosen->alteration, chosen->type, chosen->target, chosen->target_alteration);
        accented[s] = true;
    }
    return true;
}

/*
 * The last note of a closed ending is the root or third of its chord, one or two semitones from the note before; of an
 * open ending, degree 2, 5 or 7 of the key.
 */
static bool ending_holds(const Layout *layout, const Place *place, const Melody *melody) {
    size_t last = layout->rhythm.count - 1;
    if (layout->grid.ending == ENDING_CLOSED) {
        long step = step_of(place->position[last] - chord_shift(layout->degree[last]));
        int move = abs(melody->pitches[last] - melody->pitches[last - 1]);
        return (step == 0 || step == 2) && (move == 1 || move == 2);
    }
    if (layout->grid.ending == ENDING_OPEN) {
        long step = step_of(place->position[last]);
        return step == 1 || step == 4 || step == 6;
    }
    return true;
}

/* True when note i starts off the beat and is held across the next one (the beat counted from the first note after the pickup). */
static bool crosses_beat(const Layout *layout, const long *onset, size_t i) {
    long beat = layout->grid.beat;
    long from = onset[i] - layout->rhythm.pickup;
    long offset = ((from % beat) + beat) % beat;
    return offset != 0 && offset + layout->rhythm.ticks[i] > beat;
}

/* The frames of every segment, the notes between, and a range of no more than a twelfth. */
static bool draw_melody(const MotiveRandomPreferences *preferences, const AnalysisKey *key, const Layout *layout, FillRng *rng, Feature feature, Melody *melody) {
    Place place;
    memset(&place, 0, sizeof(place));
    memset(melody, 0, sizeof(*melody));
    long onset[MAX_NOTES];
    long at = 0;
    for (size_t i = 0; i < layout->rhythm.count; i++) {
        onset[i] = at;
        at += layout->rhythm.ticks[i];
    }
    for (size_t s = 0; s < layout->segment_count; s++) {
        for (size_t i = layout->first[s]; i <= layout->last[s]; i++) {
            place.segment[i] = (int)s;
            /* An off-beat note held across the next beat (a syncopation) is a chord tone. */
            place.frame[i] = layout->rhythm.on_beat[i] || i == layout->first[s] || layout->chord_first[i] ||
                             (preferences->syncopation_one_in > 0 && crosses_beat(layout, onset, i));
        }
    }
    /* A lead-in's last note is a frame note (a chord tone off the beat); when the last segment repeats a, so is a's. */
    if (layout->grid.ending == ENDING_LEAD_IN) {
        size_t last_segment = layout->segment_count - 1;
        place.frame[layout->last[last_segment]] = true;
        if (layout->repeats[last_segment]) {
            place.frame[layout->last[0]] = true;
        }
    }
    /* A pickup that leads into the accent is no longer than the note it leads into. */
    if (preferences->pickup_from_accent && layout->rhythm.pickup > 0 && layout->rhythm.ticks[layout->cell_start] < layout->rhythm.pickup) {
        return false;
    }
    if (!draw_frame_a(preferences, key, layout, rng, feature, &place, melody)) {
        return false;
    }
    for (size_t s = 1; s < layout->segment_count; s++) {
        if (!(layout->repeats[s] ? draw_frame_repeat(preferences, key, layout, rng, s, &place, melody)
                                 : draw_frame_new(preferences, key, layout, rng, s, &place, melody))) {
            return false;
        }
    }
    /* ... and sits one chord tone below it. */
    if (preferences->pickup_from_accent && layout->rhythm.pickup > 0 &&
        !set_frame(key, layout, &place, melody, 0, place.rung[layout->cell_start] - 1)) {
        return false;
    }
    bool accented[SEGMENT_MAX] = {false, false, false};
    if (preferences->long_nct_one_in > 0 && !place_long_accents(preferences, key, layout, rng, &place, melody, accented)) {
        return false;
    }
    if (preferences->accented_one_in > 0 && !place_accents(preferences, key, layout, rng, &place, melody, accented)) {
        return false;
    }
    if (!draw_decorations(preferences, key, layout, rng, &place, melody)) {
        return false;
    }
    if (!ending_holds(layout, &place, melody)) {
        return false;
    }
    /* A note tied across the barline is a chord tone of the chord after it too. */
    for (size_t s = 1; s < layout->segment_count; s++) {
        if (layout->boundary[s] == SYNC_TIE_BAR &&
            !motive_random_is_chord_step(place.position[layout->last[s - 1]] - chord_shift(layout->degree[layout->first[s]]))) {
            return false;
        }
    }
    /* A motive with no pickup does not open as one that has: a short note, then a longer one on the root, higher. */
    if (preferences->pickup_from_accent && layout->rhythm.pickup == 0 && layout->rhythm.ticks[0] <= layout->grid.beat &&
        layout->rhythm.ticks[1] > layout->rhythm.ticks[0] &&
        step_of(place.position[1] - chord_shift(layout->degree[1])) == 0 && melody->pitches[1] > melody->pitches[0]) {
        return false;
    }
    if (preferences->avoid_repeats) {
        for (size_t k = 0; k + 1 < layout->rhythm.count; k++) {
            bool suspension = melody->decorated[k + 1] && melody->types[k + 1] == NON_CHORD_TONE_SUSPENSION;
            if (melody->pitches[k] == melody->pitches[k + 1] && !suspension && !place.repeat_pair[k + 1]) {
                return false;
            }
        }
    }
    int low = 127;
    int high = 0;
    for (size_t i = 0; i < layout->rhythm.count; i++) {
        low = melody->pitches[i] < low ? melody->pitches[i] : low;
        high = melody->pitches[i] > high ? melody->pitches[i] : high;
    }
    /* Leaps (a fourth or more) and skips (a third): at most leaps_per_motif and skips_per_motif, and each leap followed by a step back when asked. */
    unsigned leaps = 0;
    unsigned skips = 0;
    for (size_t k = 0; k + 1 < layout->rhythm.count; k++) {
        int move = melody->pitches[k + 1] - melody->pitches[k];
        if (abs(move) < LEAP_SEMITONES) {
            skips += abs(move) >= SKIP_SEMITONES;
            continue;
        }
        leaps++;
        if (preferences->leap_recovery && k + 2 < layout->rhythm.count) {
            int back = melody->pitches[k + 2] - melody->pitches[k + 1];
            if (abs(back) < 1 || abs(back) > 2 || (back > 0) == (move > 0)) {
                return false;
            }
        }
    }
    if (leaps > preferences->leaps_per_motif || skips > preferences->skips_per_motif) {
        return false;
    }
    return high - low <= MAX_RANGE_SEMITONES;
}

/* ------------------------------------------------------- the motive itself */

/* The plan as the motive's own harmony (a chord on each segment's first beat) and the segments as its features. */
static bool set_plan_and_segments(const MotiveRandomOptions *options, const Layout *layout, Motive *motive) {
    unsigned long den = (unsigned long)TICKS_PER_UNIT * (unsigned long)options->meter.beat_unit;
    static const char *const LETTERS = "abcdefgh";
    char names[SEGMENT_MAX][MOTIVE_FEATURE_LABEL_MAX + 1];
    size_t letter = 0;
    for (size_t k = 0; k < layout->chord_count; k++) {
        MotiveSketchChord *chord = &motive->chords[k];
        memset(chord, 0, sizeof(*chord));
        chord->degree = layout->chord_degree[k];
        /* The chord stays on the beat (the segment's cell start) when an anticipation sounds before it. */
        if (rational_make(layout->chord_onset[k], den, &chord->onset) != RATIONAL_OK) {
            return false;
        }
    }
    for (size_t s = 0; s < layout->segment_count; s++) {
        names[s][0] = layout->repeats[s] ? LETTERS[0] : LETTERS[letter++];
        names[s][1] = layout->repeats[s] ? '\'' : '\0';
        names[s][2] = '\0';
        MotiveSketchFeature *feature = &motive->features[s];
        memset(feature, 0, sizeof(*feature));
        strcpy(feature->label, names[s]);
        feature->first = layout->first[s];
        feature->last = layout->last[s];
    }
    motive->chord_count = layout->chord_count;
    motive->feature_count = layout->segment_count;
    return true;
}

/* True when note i is the first of a segment that arrives early (an anticipation). */
static bool anticipated(const Layout *layout, size_t i) {
    for (size_t s = 1; s < layout->segment_count; s++) {
        if (layout->first[s] == i && layout->boundary[s] == SYNC_ANTICIPATION) {
            return true;
        }
    }
    return false;
}

/*
 * True when the tone roles read the motive as it was drawn: the base harmony is the plan, given, every frame note is a
 * chord tone, and every other note is a non-chord tone of its type that resolves. An early note is read against the chord
 * before it, so like a run note it may be a chord tone or a non-chord tone that resolves.
 */
static bool tones_as_drawn(const Motive *motive, const Layout *layout, const Melody *melody) {
    MotiveAnalysis analysis;
    if (!motive_analyze(motive, &analysis) || !analysis.base_harmony_known || !analysis.tones_known ||
        !analysis.base_harmony.given || analysis.base_harmony.count != layout->chord_count) {
        return false;
    }
    for (size_t k = 0; k < layout->chord_count; k++) {
        if (analysis.base_harmony.chords[k].degree != layout->chord_degree[k]) {
            return false;
        }
    }
    for (size_t i = 0; i < motive->note_count; i++) {
        const MotiveToneLabel *label = &analysis.tones[i];
        bool expected = melody->run[i] || anticipated(layout, i) ? label->role == MOTIVE_TONE_CHORD || (label->role == MOTIVE_TONE_NON_CHORD && !label->unresolved)
                        : melody->decorated[i] ? label->role == MOTIVE_TONE_NON_CHORD && label->type == melody->types[i] &&
                                                   !label->unresolved
                                             : label->role == MOTIVE_TONE_CHORD;
        if (!expected) {
            return false;
        }
    }
    return true;
}

/* A motive of the notes first to last of a layout (no pickup), to be scored against another. */
static bool segment_motive(const MotiveRandomOptions *options, const AnalysisKey *key, const Layout *layout, const Melody *melody, size_t first,
                           size_t last, Motive *out) {
    Rhythm rhythm;
    Melody notes;
    memset(&rhythm, 0, sizeof(rhythm));
    memset(&notes, 0, sizeof(notes));
    for (size_t i = first; i <= last; i++) {
        size_t k = i - first;
        rhythm.ticks[k] = layout->rhythm.ticks[i];
        rhythm.on_beat[k] = layout->rhythm.on_beat[i];
        rhythm.total += layout->rhythm.ticks[i];
        notes.pitches[k] = melody->pitches[i];
        notes.decorated[k] = melody->decorated[i];
        notes.types[k] = melody->types[i];
    }
    rhythm.count = last - first + 1;
    rhythm.ticks_per_unit = layout->rhythm.ticks_per_unit;
    return motive_random_build(options, key, &rhythm, &notes, out);
}

/*
 * The contrast distance (contrast_fidelity_min): the recognizability of the segment that is not a repeat against segment a
 * is at least the minimum. A layout with no such segment passes. Both motives are made and scored here; false when
 * the score is below the minimum or cannot be made.
 */
static bool contrast_is_close(const MotiveRandomOptions *options, const AnalysisKey *key, const MotiveRandomPreferences *preferences, const Layout *layout,
                              const Melody *melody) {
    for (size_t s = 1; s < layout->segment_count; s++) {
        if (layout->repeats[s]) {
            continue;
        }
        Motive a;
        Motive b;
        MotiveRecognizability score;
        if (!segment_motive(options, key, layout, melody, layout->cell_start, layout->last[0], &a) ||
            !segment_motive(options, key, layout, melody, layout->first[s], layout->last[s], &b) ||
            !motive_recognizability(&a, &b, NULL, &score)) {
            return false;
        }
        const ComposerRatio *least = &preferences->contrast_fidelity_min;
        if ((unsigned long long)score.total.numerator * least->denominator < (unsigned long long)least->numerator * score.total.denominator) {
            return false;
        }
    }
    return true;
}

/*
 * True when the given plan (plan_count > 0) can be drawn in the meter: a count of 1 to MOTIVE_RANDOM_PLAN_MAX, degrees 1 to 7,
 * places that are all or none after the first, rising from 1:1 and inside the bars and the bar's beats, no more chords than
 * segments when unplaced, a bar count the settings allow that a shape fits (plan_bars_fit), and, when only an open ending can
 * be drawn, a last chord with a tone on degree 2, 5 or 7 (not IV or vi).
 */
bool motive_random_segments_plan_fits(const MotiveRandomPreferences *preferences, const PlanMeter *meter) {
    size_t count = preferences->plan_count;
    if (count == 0 || count > MOTIVE_RANDOM_PLAN_MAX) {
        return false;
    }
    Grid grid = grid_of(preferences, meter);
    size_t placed = 0;
    long before = 0; /* the place of the chord before, in beats from 1:1 (the first chord is at 1:1) */
    for (size_t k = 0; k < count; k++) {
        const MotiveRandomPlanChord *chord = &preferences->plan[k];
        if (chord->degree < 1 || chord->degree > 7 || chord->bar > 2) {
            return false;
        }
        if (k == 0) {
            if (chord->bar != 0 && (chord->bar != 1 || chord->beat != 1)) {
                return false;
            }
        } else if (chord->bar > 0) {
            long at = ((long)chord->bar - 1) * grid.beats + (long)chord->beat - 1;
            if (chord->beat < 1 || (long)chord->beat > grid.beats || at <= before) {
                return false;
            }
            before = at;
            placed++;
        }
    }
    if (placed != 0 && placed != count - 1) {
        return false; /* some but not all of the chords after the first are placed */
    }
    if (placed == 0 && count > SEGMENT_MAX) {
        return false;
    }
    bool halves = has_halves(&grid);
    if (!plan_bars_fit(preferences, halves, 1) && !plan_bars_fit(preferences, halves, 2)) {
        return false;
    }
    int last = preferences->plan[count - 1].degree;
    bool only_open = preferences->endings[2] > 0 && preferences->endings[0] + preferences->endings[1] + preferences->endings[3] == 0;
    return !(only_open && (last == 4 || last == 6));
}

bool motive_random_segments_candidate(const MotiveRandomOptions *options, const AnalysisKey *key, FillRng *rng, Motive *out,
                                      bool *found) {
    Setup setup;
    MotiveRandomPreferences defaults;
    const MotiveRandomPreferences *preferences = options->preferences;
    if (preferences == NULL) {
        motive_random_preferences_default(&defaults);
        preferences = &defaults;
    }
    *found = false;
    draw_setup(preferences, &options->meter, rng, &setup);
    if (setup.grid.ending == ENDING_OPEN) {
        /* An open ending is on degree 2, 5 or 7 of the key: the last chord needs a tone there (not so for IV and vi). */
        bool tone = false;
        for (long t = 0; t < 3; t++) {
            long step = step_of(setup.plan.degrees[setup.plan.count - 1] - 1 + 2 * t);
            tone = tone || step == 1 || step == 4 || step == 6;
        }
        if (!tone) {
            return true;
        }
    }
    for (int attempt = 0; attempt < SEGMENT_TRIES_PER_SETUP; attempt++) {
        Layout layout;
        Melody melody;
        Feature feature;
        const char *fault = NULL;
        if (!draw_layout(preferences, &setup, rng, &layout, &feature) || !draw_melody(preferences, key, &layout, rng, feature, &melody) ||
            !motive_random_build(options, key, &layout.rhythm, &melody, out) ||
            !set_plan_and_segments(options, &layout, out) || !tones_as_drawn(out, &layout, &melody)) {
            continue;
        }
        if (preferences->contrast_fidelity_min.numerator > 0 && !contrast_is_close(options, key, preferences, &layout, &melody)) {
            continue;
        }
        if (!motive_melodic_fault(out, &fault)) {
            return false;
        }
        if (fault == NULL) {
            *found = true;
            return true;
        }
    }
    return true;
}
