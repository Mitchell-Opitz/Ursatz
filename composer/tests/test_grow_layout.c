#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/theme_shape.h>

#include "grow/grow_internal.h"

/*
 * The layout of a theme (grow_layout_of) for each of the nine pairings at 4 and 8 bars, in 4/4 and 3/4: the start and the
 * length of each place, in eighths of the theme, written out here from the layout of the fixed 4 and 8 bar themes, not
 * read from the theme shape table. A place a pairing does not have has length 0.
 */

typedef struct {
    unsigned start;
    unsigned length;
} Eighths;

/* Indexed by the place (basic, repetition, fragment 1, fragment 2, cadential). */
static const Eighths BEGINNING[GROW_PLACE_COUNT] = {{0, 2}, {2, 2}, {0, 0}, {0, 0}, {0, 0}};
static const Eighths ENDINGS[3][GROW_PLACE_COUNT] = {
    {{0, 0}, {0, 0}, {4, 1}, {5, 1}, {6, 2}}, /* continuation */
    {{0, 0}, {0, 0}, {4, 2}, {6, 2}, {0, 0}}, /* consequent */
    {{0, 0}, {0, 0}, {4, 2}, {6, 2}, {0, 0}}, /* cadential */
};

/* Length of count eighths of a theme of `bars` bars of `bar`. */
static Rational eighths_of(Rational bar, size_t bars, unsigned count) {
    Rational theme;
    Rational eighth;
    Rational out;
    assert(rational_mul_int(bar, (long long)bars, &theme) == RATIONAL_OK);
    assert(rational_div_int(theme, 8, &eighth) == RATIONAL_OK);
    assert(rational_mul_int(eighth, (long long)count, &out) == RATIONAL_OK);
    return out;
}

static void check_place(const GrowLayout *layout, Rational bar, size_t bars, size_t place, Eighths expected) {
    assert(rational_cmp(layout->start[place], eighths_of(bar, bars, expected.start)) == 0 || expected.length == 0);
    assert(rational_cmp(layout->length[place], eighths_of(bar, bars, expected.length)) == 0);
}

/* The places of a half lie end to end from the half's start to its end, with no gap. */
static void check_tiles(const GrowLayout *layout, size_t first, size_t last, Rational start, Rational length) {
    Rational at = start;
    for (size_t place = first; place <= last; place++) {
        if (rational_is_zero(layout->length[place])) {
            continue;
        }
        assert(rational_cmp(layout->start[place], at) == 0);
        assert(rational_add(at, layout->length[place], &at) == RATIONAL_OK);
    }
    Rational end;
    assert(rational_add(start, length, &end) == RATIONAL_OK);
    assert(rational_cmp(at, end) == 0);
}

int main(void) {
    static const Rational METERS[2] = {{1, 1}, {3, 4}};
    static const size_t BARS[2] = {4, 8};
    size_t cases = 0;
    for (int m = 0; m < 2; m++) {
        for (int b = 0; b < 2; b++) {
            for (int beginning = PLAN_THEME_BEGINNING_PRESENTATION; beginning <= PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA;
                 beginning++) {
                for (int ending = PLAN_THEME_ENDING_CONTINUATION; ending <= PLAN_THEME_ENDING_CADENTIAL; ending++) {
                    GrowThemeRequest request;
                    GrowLayout layout;
                    memset(&request, 0, sizeof(request));
                    request.bar = METERS[m];
                    request.bars = BARS[b];
                    request.pairing.beginning = (PlanThemeBeginning)beginning;
                    request.pairing.ending = (PlanThemeEnding)ending;
                    assert(grow_layout_of(&request, &layout));
                    for (size_t place = GROW_PLACE_BASIC; place <= GROW_PLACE_REPETITION; place++) {
                        check_place(&layout, request.bar, request.bars, place, BEGINNING[place]);
                    }
                    for (size_t place = GROW_PLACE_FRAGMENT_1; place <= GROW_PLACE_CADENTIAL; place++) {
                        check_place(&layout, request.bar, request.bars, place, ENDINGS[ending][place]);
                    }
                    /* The halves are four eighths each, and tile the theme. */
                    assert(rational_cmp(layout.beginning, eighths_of(request.bar, request.bars, 4)) == 0);
                    assert(rational_cmp(layout.ending, eighths_of(request.bar, request.bars, 4)) == 0);
                    Rational whole;
                    assert(rational_add(layout.beginning, layout.ending, &whole) == RATIONAL_OK);
                    assert(rational_cmp(whole, eighths_of(request.bar, request.bars, 8)) == 0);
                    check_tiles(&layout, GROW_PLACE_BASIC, GROW_PLACE_REPETITION, rational_ratio(0, 1), layout.beginning);
                    check_tiles(&layout, GROW_PLACE_FRAGMENT_1, GROW_PLACE_CADENTIAL, layout.beginning, layout.ending);
                    /* The whole bars: one for a 4 bar theme, two for an 8 bar one, the halves half the theme. */
                    assert(layout.basic_bars == request.bars / 4);
                    assert(layout.beginning_bars == request.bars / 2);
                    assert(layout.ending_bars == request.bars / 2);
                    cases++;
                }
            }
        }
    }
    assert(cases == 36);
    /* A theme with no length has no layout. */
    GrowThemeRequest empty;
    GrowLayout none;
    memset(&empty, 0, sizeof(empty));
    empty.bar = rational_ratio(1, 1);
    assert(!grow_layout_of(&empty, &none));
    printf("test_grow_layout: %zu layouts ok\n", cases);
    return 0;
}
