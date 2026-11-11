#include <ursatz_composer/theme_shape.h>

#include <string.h>

#include "grow/grow_internal.h"

_Static_assert(PLAN_CONSTRUCTION_IDEAS == GROW_IDEA_COUNT, "a construction holds the bars of every idea");

/* The whole bars a length is, 0 when it is not whole. */
static size_t whole_bars(Rational length, Rational bar) {
    Rational bars = {0, 1};
    if (bar.num <= 0 || rational_div(length, bar, &bars) != RATIONAL_OK || bars.den != 1 || bars.num <= 0) {
        return 0;
    }
    return (size_t)bars.num;
}

/* A length of count units, false when it does not fit a Rational. */
static bool scaled(Rational unit, size_t count, Rational *out) {
    return rational_mul_int(unit, (long long)count, out) == RATIONAL_OK;
}

/* Lays the places of one half end to end from the half's start, in place order. */
static bool lay_half(const GrowThemeRequest *request, GrowLayout *layout, Rational unit, size_t first, size_t last,
                     size_t half_start_units) {
    size_t at = half_start_units;
    for (size_t place = first; place <= last; place++) {
        const ThemeShapeIdea *row = theme_shape_place_in(&request->pairing, (unsigned)place);
        if (!row) {
            continue;
        }
        if (!scaled(unit, at, &layout->start[place]) || !scaled(unit, row->units, &layout->length[place])) {
            return false;
        }
        at += row->units;
    }
    return true;
}

/*
 * Lays the places of one half end to end from the half's start, each as long as the bars the construction gives its
 * idea. Starts at bar `*at` and leaves it at the half's end; false when the pairing has an idea of 0 bars.
 */
static bool lay_half_bars(const GrowThemeRequest *request, GrowLayout *layout, size_t first, size_t last,
                          size_t *at) {
    for (size_t place = first; place <= last; place++) {
        const ThemeShapeIdea *row = theme_shape_place_in(&request->pairing, (unsigned)place);
        if (!row) {
            continue;
        }
        size_t bars = request->construction.idea_bars[row->idea];
        if (bars == 0 || !scaled(request->bar, *at, &layout->start[place]) ||
            !scaled(request->bar, bars, &layout->length[place])) {
            return false;
        }
        *at += bars;
    }
    return true;
}

/*
 * The layout of a theme with a construction (R-468): its ideas are the bars it gives, laid end to end in place order in
 * each half. False when the ideas the pairing has do not add up to the theme's bars, one of them has no bars, an idea
 * the pairing does not have has some, or the construction has an extension (R-468; a theme with one is laid out later).
 */
static bool construction_layout(const GrowThemeRequest *request, GrowLayout *layout) {
    const PlanConstruction *construction = &request->construction;
    if (request->bar.num <= 0 || construction->extension_count != 0) {
        return false;
    }
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        if (!theme_shape_idea_in(&request->pairing, (GrowIdea)idea) && construction->idea_bars[idea] != 0) {
            return false;
        }
    }
    size_t beginning = 0;
    size_t total = 0;
    if (!lay_half_bars(request, layout, GROW_PLACE_BASIC, GROW_PLACE_REPETITION, &beginning)) {
        return false;
    }
    total = beginning;
    if (!lay_half_bars(request, layout, GROW_PLACE_FRAGMENT_1, GROW_PLACE_CADENTIAL, &total) || total != request->bars ||
        !scaled(request->bar, beginning, &layout->beginning) || !scaled(request->bar, total - beginning, &layout->ending)) {
        return false;
    }
    layout->basic_bars = construction->idea_bars[GROW_IDEA_BASIC];
    layout->beginning_bars = beginning;
    layout->ending_bars = total - beginning;
    return true;
}

bool grow_layout_of(const GrowThemeRequest *request, GrowLayout *out) {
    GrowLayout layout;
    memset(&layout, 0, sizeof(layout));
    for (size_t place = 0; place < GROW_PLACE_COUNT; place++) {
        layout.start[place] = rational_ratio(0, 1);
        layout.length[place] = rational_ratio(0, 1);
    }
    if (request->has_construction) {
        if (!construction_layout(request, &layout)) {
            return false;
        }
        *out = layout;
        return true;
    }
    Rational unit = grow_unit(request);
    if (unit.num == 0 || !scaled(unit, 4, &layout.beginning) || !scaled(unit, 4, &layout.ending) ||
        !lay_half(request, &layout, unit, GROW_PLACE_BASIC, GROW_PLACE_REPETITION, 0) ||
        !lay_half(request, &layout, unit, GROW_PLACE_FRAGMENT_1, GROW_PLACE_CADENTIAL, 4)) {
        return false;
    }
    layout.basic_bars = whole_bars(layout.length[GROW_PLACE_BASIC], request->bar);
    layout.beginning_bars = whole_bars(layout.beginning, request->bar);
    layout.ending_bars = whole_bars(layout.ending, request->bar);
    *out = layout;
    return true;
}
