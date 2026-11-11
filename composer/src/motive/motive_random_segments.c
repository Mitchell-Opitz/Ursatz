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
#define SEGMENT_TRIES_PER_SETUP 8 /* notes drawn within one bars, shape and plan before another is drawn (a candidate) */
#define SEGMENT_CELL_TRIES 32 /* a cell that does not fit is drawn again, so a failed cell does not change the shape's odds */
#define SEGMENT_START_REACH 4 /* rungs either side of the note before, searched for where a new segment starts */
#define SEGMENT_START_DISTANCE 5 /* the most semitones from the note before to where a new segment starts */
#define LEAP_SEMITONES 5 /* an interval this large or larger is a leap (leaps_per_motif, leap_recovery) */

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

/*
 * The tick grid of a meter: the bar's beats, the ticks in a beat and in half of it, and whether the beat is a dotted unit.
 * Simple meters beat once per beat unit (12 ticks); a compound meter with compound_beats beats once per three.
 */
typedef struct {
    long beats;
    long beat;
    long half;
    bool compound;
} Grid;

static Grid grid_of(const MotiveRandomPreferences *preferences, const PlanMeter *meter) {
    (void)preferences;
    Grid grid = {(long)meter->beats_per_bar, TICKS_PER_UNIT, TICKS_PER_UNIT / 2, false};
    return grid;
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
}

/* The scale-step offset of a chord's tones from the tones of I: I 0, V down a fourth, IV up a fourth (same octave). */
static long chord_shift(int degree) {
    return degree == 5 ? -3 : degree == 4 ? 3 : 0;
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

/* Tiles from to to (ticks from the start of the cell) with the notes of the durations list: half a beat, a beat, two beats. */
static bool tile(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long from, long to, Cell *cell) {
    long unit = weak_beat_unit(grid);
    const long lengths[DURATION_KINDS] = {grid->half, grid->beat, 2 * grid->beat};
    for (long position = from; position < to;) {
        long room = to - position;
        bool on_beat = position % grid->beat == 0;
        unsigned long weights[DURATION_KINDS];
        for (size_t i = 0; i < DURATION_KINDS; i++) {
            bool fits = lengths[i] <= room && (lengths[i] == grid->half || on_beat);
            weights[i] = fits ? preferences->durations[i] : 0;
        }
        if (on_beat && position % unit != 0) {
            weights[2] *= 1 + preferences->long_on_weak; /* the two-beat note on a weak beat (long_on_weak) */
        }
        size_t chosen = pick_weighted(rng, weights, DURATION_KINDS);
        if (cell->count == MAX_NOTES) {
            return false;
        }
        cell->ticks[cell->count++] = lengths[chosen];
        position += lengths[chosen];
    }
    return true;
}

/* Segment a's cell of length ticks for a feature: a dotted figure on a beat, or a cell with two notes on beats. */
static bool try_cell_a(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long length, Feature feature, bool long_end, Cell *cell) {
    memset(cell, 0, sizeof(*cell));
    if (feature == FEATURE_DOTTED) {
        long figure = 4 * grid->half; /* a dotted beat and the half beat that completes the next one: two beats */
        long slots = (length - figure) / grid->beat + 1;
        if (slots < 1) {
            return false;
        }
        long start = grid->beat * (long)fill_rng_below(rng, (uint64_t)slots);
        if (!tile(preferences, grid, rng, 0, start, cell) || cell->count + 2 > MAX_NOTES) {
            return false;
        }
        cell->ticks[cell->count++] = 3 * grid->half;
        cell->ticks[cell->count++] = grid->half;
        if (!tile(preferences, grid, rng, start + figure, length, cell)) {
            return false;
        }
    } else {
        if (!tile(preferences, grid, rng, 0, length, cell)) {
            return false;
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
    if (long_end && cell->ticks[cell->count - 1] < grid->beat) {
        return false;
    }
    return !(long_end && preferences->final_long) || last_is_longest(cell);
}

/* A new cell for the last segment: tiled, then its last notes joined until the last note is at least a beat long. */
static bool try_cell_b(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long length, const Cell *not_like, Cell *cell) {
    memset(cell, 0, sizeof(*cell));
    if (!tile(preferences, grid, rng, 0, length, cell)) {
        return false;
    }
    while (cell->count >= 2 && cell->ticks[cell->count - 1] < grid->beat) {
        cell->ticks[cell->count - 2] += cell->ticks[cell->count - 1];
        cell->count--;
    }
    if (cell->ticks[cell->count - 1] < grid->beat) {
        return false;
    }
    if (preferences->final_long && (!last_is_longest(cell) || cell->count > not_like->count)) {
        return false;
    }
    return cell->count != not_like->count || memcmp(cell->ticks, not_like->ticks, cell->count * sizeof(long)) != 0;
}

static bool draw_cell_a(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long length, Feature feature, bool long_end, Cell *cell) {
    for (int attempt = 0; attempt < SEGMENT_CELL_TRIES; attempt++) {
        if (try_cell_a(preferences, grid, rng, length, feature, long_end, cell)) {
            return true;
        }
    }
    return false;
}

static bool draw_cell_b(const MotiveRandomPreferences *preferences, const Grid *grid, FillRng *rng, long length, const Cell *not_like, Cell *cell) {
    for (int attempt = 0; attempt < SEGMENT_CELL_TRIES; attempt++) {
        if (try_cell_b(preferences, grid, rng, length, not_like, cell)) {
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
} Setup;

static void draw_setup(const MotiveRandomPreferences *preferences, const PlanMeter *meter, FillRng *rng, Setup *setup) {
    memset(setup, 0, sizeof(*setup));
    setup->grid = grid_of(preferences, meter);
    long beats = setup->grid.beats;
    bool halves = beats % 2 == 0 && beats >= 2 * SEGMENT_MIN_CELL_BEATS;
    setup->bars = 2;
    if (halves) {
        setup->bars = 1 + (long)pick_weighted(rng, preferences->bars, 2);
    }
    setup->pickup = preferences->pickup_one_in > 0 && fill_rng_below(rng, preferences->pickup_one_in) == 0
                        ? (1 + (long)fill_rng_below(rng, 2)) * setup->grid.half
                        : 0;
    setup->shape = (Shape)(setup->bars == 1 ? pick_weighted(rng, preferences->shapes_one_bar, 2)
                                            : pick_weighted(rng, preferences->shapes_two_bars, halves ? SHAPE_KINDS : SHAPE_KINDS - 1));
    if (setup->shape == SHAPE_REPEAT_CONTRAST) {
        setup->plan = PLANS_THREE[pick_weighted(rng, preferences->plans_three, PLAN_KINDS_THREE)];
    } else {
        setup->plan = PLANS_TWO[pick_weighted(rng, preferences->plans_two, PLAN_KINDS_TWO)];
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
    if (!add_cell(layout, grid, &a, &position)) {
        return false;
    }
    layout->last[0] = layout->rhythm.count - 1;
    if (shape == SHAPE_REPEAT || shape == SHAPE_REPEAT_CONTRAST) {
        layout->first[1] = layout->rhythm.count;
        layout->repeats[1] = true;
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
        if (!add_cell(layout, grid, &b, &position)) {
            return false;
        }
        layout->last[index] = layout->rhythm.count - 1;
    }
    layout->rhythm.total = layout->rhythm.pickup + position;
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

/* A frame note on the ladder of I, moved to the tones of the chord of its segment. In minor, the third of V is raised. */
static bool set_frame(const AnalysisKey *key, const Layout *layout, Place *place, Melody *melody, size_t i, long rung) {
    int degree = layout->plan.degrees[place->segment[i]];
    long shift = chord_shift(degree);
    if (rung < 0) {
        return false;
    }
    place->rung[i] = rung;
    place->position[i] = motive_random_rung_position(rung) + shift;
    place->alteration[i] = key->mode == KEY_STRUCTURE_MODE_MINOR && degree == 5 &&
                                   step_of(place->position[i] - shift) == 2
                               ? 1
                               : 0;
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
    bool same_chord = layout->plan.degrees[segment] == layout->plan.degrees[0];
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

/*
 * The notes between frame notes: a type is drawn for each (or none), and a note takes a non-chord pitch near the note
 * before it that makes the figure with the notes either side, else a chord tone of its segment's chord near it.
 */
static bool draw_decorations(const MotiveRandomPreferences *preferences, const AnalysisKey *key, const Layout *layout, FillRng *rng, Place *place, Melody *melody) {
    for (size_t i = 0; i < layout->rhythm.count; i++) {
        if (place->frame[i]) {
            continue;
        }
        if (i == 0 || i + 1 == layout->rhythm.count || !place->frame[i - 1] || !place->frame[i + 1]) {
            return false;
        }
        int degree = layout->plan.degrees[place->segment[i]];
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
    int degree = layout->plan.degrees[place->segment[i]];
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
    size_t s = (size_t)place->segment[i];
    if (s == 0 || i != layout->first[s] || layout->plan.degrees[s] == layout->plan.degrees[s - 1]) {
        return false;
    }
    int degree = layout->plan.degrees[s];
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
 * pickup, the first note, or the last note of a segment. Each eligible note is chosen in one of `accented_one_in`.
 */
static bool place_accents(const MotiveRandomPreferences *preferences, const AnalysisKey *key, const Layout *layout, FillRng *rng, Place *place,
                          Melody *melody) {
    bool accented[SEGMENT_MAX] = {false, false, false};
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

/* The frames of every segment, the notes between, and a range of no more than a twelfth. */
static bool draw_melody(const MotiveRandomPreferences *preferences, const AnalysisKey *key, const Layout *layout, FillRng *rng, Feature feature, Melody *melody) {
    Place place;
    memset(&place, 0, sizeof(place));
    memset(melody, 0, sizeof(*melody));
    for (size_t s = 0; s < layout->segment_count; s++) {
        for (size_t i = layout->first[s]; i <= layout->last[s]; i++) {
            place.segment[i] = (int)s;
            place.frame[i] = layout->rhythm.on_beat[i] || i == layout->first[s];
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
    if (preferences->accented_one_in > 0 && !place_accents(preferences, key, layout, rng, &place, melody)) {
        return false;
    }
    if (!draw_decorations(preferences, key, layout, rng, &place, melody)) {
        return false;
    }
    /* A motive with no pickup does not open as one that has: a short note, then a longer one on the root, higher. */
    if (preferences->pickup_from_accent && layout->rhythm.pickup == 0 && layout->rhythm.ticks[0] <= layout->grid.beat &&
        layout->rhythm.ticks[1] > layout->rhythm.ticks[0] &&
        step_of(place.position[1] - chord_shift(layout->plan.degrees[place.segment[1]])) == 0 && melody->pitches[1] > melody->pitches[0]) {
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
    /* Leaps (an interval of a fourth or more): at most leaps_per_motif, and each one followed by a step back when asked. */
    unsigned leaps = 0;
    for (size_t k = 0; k + 1 < layout->rhythm.count; k++) {
        int move = melody->pitches[k + 1] - melody->pitches[k];
        if (abs(move) < LEAP_SEMITONES) {
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
    if (leaps > preferences->leaps_per_motif) {
        return false;
    }
    return high - low <= MAX_RANGE_SEMITONES;
}

/* ------------------------------------------------------- the motive itself */

/* The plan as the motive's own harmony (a chord on each segment's first beat) and the segments as its features. */
static bool set_plan_and_segments(const MotiveRandomOptions *options, const Layout *layout, Motive *motive) {
    unsigned long den = (unsigned long)TICKS_PER_UNIT * (unsigned long)options->meter.beat_unit;
    long onset[MAX_NOTES];
    long position = 0;
    for (size_t i = 0; i < layout->rhythm.count; i++) {
        onset[i] = position;
        position += layout->rhythm.ticks[i];
    }
    static const char *const LETTERS = "abcdefgh";
    char names[SEGMENT_MAX][MOTIVE_FEATURE_LABEL_MAX + 1];
    size_t letter = 0;
    for (size_t s = 0; s < layout->segment_count; s++) {
        MotiveSketchChord *chord = &motive->chords[s];
        memset(chord, 0, sizeof(*chord));
        chord->degree = layout->plan.degrees[s];
        if (rational_make(s == 0 ? 0 : onset[layout->first[s]], den, &chord->onset) != RATIONAL_OK) {
            return false;
        }
        names[s][0] = layout->repeats[s] ? LETTERS[0] : LETTERS[letter++];
        names[s][1] = layout->repeats[s] ? '\'' : '\0';
        names[s][2] = '\0';
        MotiveSketchFeature *feature = &motive->features[s];
        memset(feature, 0, sizeof(*feature));
        strcpy(feature->label, names[s]);
        feature->first = layout->first[s];
        feature->last = layout->last[s];
    }
    motive->chord_count = layout->segment_count;
    motive->feature_count = layout->segment_count;
    return true;
}

/*
 * True when the tone roles read the motive as it was drawn: the base harmony is the plan, given, every frame note is a
 * chord tone, and every other note is a non-chord tone of its type that resolves.
 */
static bool tones_as_drawn(const Motive *motive, const Layout *layout, const Melody *melody) {
    MotiveAnalysis analysis;
    if (!motive_analyze(motive, &analysis) || !analysis.base_harmony_known || !analysis.tones_known ||
        !analysis.base_harmony.given || analysis.base_harmony.count != layout->segment_count) {
        return false;
    }
    for (size_t s = 0; s < layout->segment_count; s++) {
        if (analysis.base_harmony.chords[s].degree != layout->plan.degrees[s]) {
            return false;
        }
    }
    for (size_t i = 0; i < motive->note_count; i++) {
        const MotiveToneLabel *label = &analysis.tones[i];
        bool expected = melody->decorated[i] ? label->role == MOTIVE_TONE_NON_CHORD && label->type == melody->types[i] &&
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
