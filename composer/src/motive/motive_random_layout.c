#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_develop.h>

#include "motive/motive_random_internal.h"
#include "util/util.h"

/* The layout of the SEGMENTS strategy: the setup (bars, shape, plan, ending) and the cells put end to end. */

/* ----------------------------------------------------------------- layout */


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
bool motive_random_plan_bars_fit(const MotiveRandomPreferences *preferences, bool halves, long bars) {
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
bool motive_random_has_halves(const Grid *grid) {
    return grid->beats % 2 == 0 && grid->beats >= 2 * SEGMENT_MIN_CELL_BEATS;
}

void motive_random_draw_setup(const MotiveRandomPreferences *preferences, const PlanMeter *meter, FillRng *rng, Setup *setup) {
    memset(setup, 0, sizeof(*setup));
    setup->grid = motive_random_grid_of(preferences, meter);
    bool halves = motive_random_has_halves(&setup->grid);
    bool given = preferences->plan_count > 0;
    setup->placed = given && plan_placed(preferences);
    setup->bars = 2;
    if (halves) {
        unsigned long weights[2] = {preferences->bars[0], preferences->bars[1]};
        for (size_t b = 0; given && b < 2; b++) {
            if (!motive_random_plan_bars_fit(preferences, halves, (long)b + 1)) {
                weights[b] = 0; /* a given plan does not fit that many bars */
            }
        }
        setup->bars = 1 + (long)motive_random_pick_weighted(rng, weights, 2);
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
        setup->shape = (Shape)motive_random_pick_weighted(rng, shapes, shape_count);
        setup->plan.count = setup->shape == SHAPE_REPEAT_CONTRAST ? 3 : 2;
        for (size_t s = 0; s < setup->plan.count; s++) {
            bool own = !setup->placed && preferences->plan_count > 1;
            setup->plan.degrees[s] = preferences->plan[own ? s : 0].degree;
        }
        if (setup->placed) {
            setup->plan.degrees[setup->plan.count - 1] = preferences->plan[preferences->plan_count - 1].degree;
        }
    } else {
        setup->shape = (Shape)(setup->bars == 1 ? motive_random_pick_weighted(rng, preferences->shapes_one_bar, 2)
                                                : motive_random_pick_weighted(rng, preferences->shapes_two_bars, halves ? SHAPE_KINDS : SHAPE_KINDS - 1));
        if (setup->shape == SHAPE_REPEAT_CONTRAST) {
            setup->plan = PLANS_THREE[motive_random_pick_weighted(rng, preferences->plans_three, PLAN_KINDS_THREE)];
        } else {
            setup->plan = PLANS_TWO[motive_random_pick_weighted(rng, preferences->plans_two, PLAN_KINDS_TWO)];
        }
    }
    /* The ending (R-440), drawn only when one besides free has a weight, so the defaults draw nothing here. */
    if (preferences->endings[1] + preferences->endings[2] + preferences->endings[3] > 0) {
        setup->grid.ending = (Ending)motive_random_pick_weighted(rng, preferences->endings, ENDING_KINDS);
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
                long u = motive_random_sync_unit(grid, level);
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
        size_t pick = motive_random_pick_weighted(rng, weights, 2 * SYNC_LEVELS);
        Sync type = (Sync)(SYNC_TIE_BAR + pick / SYNC_LEVELS);
        long u = motive_random_sync_unit(grid, pick % SYNC_LEVELS);
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
bool motive_random_draw_layout(const MotiveRandomPreferences *preferences, const Setup *setup, FillRng *rng, Layout *layout, Feature *feature_out) {
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
    Feature feature = (Feature)motive_random_pick_weighted(rng, preferences->features, FEATURE_KINDS);
    Cell a;
    size_t tried = 0;
    while (tried < FEATURE_KINDS && !motive_random_draw_cell_a(preferences, grid, rng, a_length, feature, shape == SHAPE_REPEAT, &a)) {
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
        if (!motive_random_draw_cell_b(preferences, grid, rng, last_length, &a, &b)) {
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
    if (preferences->syncopation_one_in > 0 && motive_random_simple_meter(grid)) {
        place_boundary_syncopation(preferences, rng, layout);
    }
    set_note_chords(layout);
    return layout->rhythm.count >= MIN_NOTES;
}

