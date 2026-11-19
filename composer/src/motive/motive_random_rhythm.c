#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_develop.h>

#include "motive/motive_random_internal.h"
#include "util/util.h"

/* The rhythm cells of the SEGMENTS strategy: figures, syncopation and the cells a and b (motive_random_segments.c draws the motive). */


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
    return (type == SYNC_TIE_BEAT ? 8 : 4) * motive_random_sync_unit(grid, level);
}

/* The notes of a figure inside a cell (u is the short note): the count. Every figure starts on a beat and ends on a beat or a half beat. */
static size_t sync_figure(const Grid *grid, size_t type, size_t level, size_t shape, long *ticks) {
    long u = motive_random_sync_unit(grid, level);
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
    if (preferences->syncopation_one_in == 0 || !motive_random_simple_meter(grid)) {
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
    size_t pick = motive_random_pick_weighted(rng, weights, 2 * SYNC_LEVELS);
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
        const Figure *chosen = &figures[motive_random_pick_weighted(rng, weights, FIGURE_KINDS)];
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

bool motive_random_draw_cell_a(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long length, Feature feature, bool long_end, Cell *cell) {
    for (int attempt = 0; attempt < SEGMENT_CELL_TRIES; attempt++) {
        if (try_cell_a(preferences, grid, rng, length, feature, long_end, attempt < SEGMENT_CELL_TRIES / 2, cell)) {
            return true;
        }
    }
    return false;
}

bool motive_random_draw_cell_b(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long length, const Cell *not_like, Cell *cell) {
    for (int attempt = 0; attempt < SEGMENT_CELL_TRIES; attempt++) {
        if (try_cell_b(preferences, grid, rng, length, not_like, attempt < SEGMENT_CELL_TRIES / 2, cell)) {
            return true;
        }
    }
    return false;
}

