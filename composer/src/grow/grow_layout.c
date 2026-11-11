#include <ursatz_composer/theme_shape.h>

#include <string.h>

#include "grow/grow_internal.h"

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

bool grow_layout_of(const GrowThemeRequest *request, GrowLayout *out) {
    GrowLayout layout;
    memset(&layout, 0, sizeof(layout));
    for (size_t place = 0; place < GROW_PLACE_COUNT; place++) {
        layout.start[place] = rational_ratio(0, 1);
        layout.length[place] = rational_ratio(0, 1);
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
