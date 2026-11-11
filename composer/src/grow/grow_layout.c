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

bool grow_extension_allowed(const PlanThemePairing *pairing, PlanExtensionKind kind, GrowIdea idea) {
    if (kind == PLAN_EXTENSION_ADDITION || kind == PLAN_EXTENSION_EVADED) {
        return true;
    }
    if ((int)idea < 0 || idea >= GROW_IDEA_COUNT || !theme_shape_idea_in(pairing, idea)) {
        return false;
    }
    if (kind == PLAN_EXTENSION_REPEAT) {
        return idea == GROW_IDEA_REPETITION || idea == GROW_IDEA_CONTRASTING || idea == GROW_IDEA_FRAGMENT_1 ||
               idea == GROW_IDEA_FRAGMENT_2 || idea == GROW_IDEA_CONSEQUENT_CONTRASTING || idea == GROW_IDEA_CADENTIAL_1;
    }
    return idea == GROW_IDEA_CADENTIAL || idea == GROW_IDEA_CONSEQUENT_CONTRASTING || idea == GROW_IDEA_CADENTIAL_2 ||
           (idea == GROW_IDEA_CONTRASTING && pairing->beginning == PLAN_THEME_BEGINNING_ANTECEDENT);
}

/* The place the theme's last idea holds under the pairing. */
static GrowPlace last_place(const PlanThemePairing *pairing) {
    for (int place = GROW_PLACE_CADENTIAL; place > GROW_PLACE_REPETITION; place--) {
        if (theme_shape_place_in(pairing, (unsigned)place)) {
            return (GrowPlace)place;
        }
    }
    return GROW_PLACE_BASIC;
}

/*
 * Lays the places of one half end to end from bar `*at`, each as long as its bars (`bars[place]`, the construction's
 * bars with the stretches added), with the repeats that follow a place after it, in the construction's order. Leaves `*at`
 * at the half's end; false when the pairing has an idea of 0 bars.
 */
static bool lay_half_bars(const GrowThemeRequest *request, GrowLayout *layout, const size_t *bars,
                          const size_t *repeat_bars, size_t first, size_t last, size_t *at) {
    for (size_t place = first; place <= last; place++) {
        if (!theme_shape_place_in(&request->pairing, (unsigned)place)) {
            continue;
        }
        if (bars[place] == 0 || !scaled(request->bar, *at, &layout->start[place]) ||
            !scaled(request->bar, bars[place], &layout->length[place])) {
            return false;
        }
        *at += bars[place];
        for (size_t k = 0; k < layout->extension_count; k++) {
            GrowLayoutExtension *extension = &layout->extensions[k];
            if (extension->kind == PLAN_EXTENSION_REPEAT && extension->place == place) {
                if (!scaled(request->bar, *at, &extension->start) || !scaled(request->bar, repeat_bars[k], &extension->length)) {
                    return false;
                }
                *at += repeat_bars[k];
            }
        }
    }
    return true;
}

/*
 * The layout of a theme with a construction (R-468): its ideas are the bars it gives, laid end to end in place order in
 * each half, a stretch adding to its idea's bars, a repeat after the idea it repeats, an evaded cadence's idea after the
 * last idea and an addition after that. False when the ideas the pairing has and the extensions do not add up to the
 * theme's bars, one of the ideas has no bars, an idea the pairing does not have has some, or an extension cannot follow
 * or stretch its idea, is a second of its kind, or needs a cadence the theme does not end in.
 */
static bool construction_layout(const GrowThemeRequest *request, GrowLayout *layout) {
    const PlanConstruction *construction = &request->construction;
    size_t bars[GROW_PLACE_COUNT] = {0};
    size_t repeat_bars[PLAN_CONSTRUCTION_EXTENSIONS] = {0};
    size_t evaded_bars = 0;
    size_t addition_bars = 0;
    bool evaded = false;
    bool added = false;
    bool authentic = request->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC || request->cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC;
    GrowPlace last = last_place(&request->pairing);
    if (request->bar.num <= 0 || construction->extension_count > PLAN_CONSTRUCTION_EXTENSIONS) {
        return false;
    }
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        const ThemeShapeIdea *row = theme_shape_idea_in(&request->pairing, (GrowIdea)idea);
        if (!row && construction->idea_bars[idea] != 0) {
            return false;
        }
        if (row) {
            bars[row->place] = construction->idea_bars[idea];
        }
    }
    size_t idea_beginning = bars[GROW_PLACE_BASIC] + bars[GROW_PLACE_REPETITION];
    size_t idea_ending = 0;
    for (size_t place = GROW_PLACE_FRAGMENT_1; place <= GROW_PLACE_CADENTIAL; place++) {
        idea_ending += bars[place];
    }
    layout->extension_count = construction->extension_count;
    for (size_t k = 0; k < construction->extension_count; k++) {
        const PlanExtension *extension = &construction->extensions[k];
        GrowLayoutExtension *laid = &layout->extensions[k];
        GrowPlace place = last;
        laid->kind = extension->kind;
        laid->varied = extension->varied;
        if (!grow_extension_allowed(&request->pairing, extension->kind, (GrowIdea)extension->idea)) {
            return false;
        }
        if ((extension->kind == PLAN_EXTENSION_REPEAT || extension->kind == PLAN_EXTENSION_STRETCH) &&
            !grow_idea_place(&request->pairing, (GrowIdea)extension->idea, &place)) {
            return false;
        }
        laid->place = place;
        switch (extension->kind) {
        case PLAN_EXTENSION_REPEAT:
            repeat_bars[k] = extension->bars ? extension->bars : bars[place];
            break;
        case PLAN_EXTENSION_STRETCH:
            bars[place] += extension->bars ? extension->bars : 1;
            if (!scaled(request->bar, extension->bars ? extension->bars : 1, &laid->length)) {
                return false;
            }
            break;
        case PLAN_EXTENSION_EVADED:
            if (evaded || !authentic) {
                return false;
            }
            evaded = true;
            evaded_bars = extension->bars ? extension->bars : bars[last];
            break;
        case PLAN_EXTENSION_ADDITION:
            if (added || !authentic || extension->bars > 2) {
                return false;
            }
            added = true;
            addition_bars = extension->bars ? extension->bars : 1;
            break;
        }
    }
    size_t beginning = 0;
    if (!lay_half_bars(request, layout, bars, repeat_bars, GROW_PLACE_BASIC, GROW_PLACE_REPETITION, &beginning)) {
        return false;
    }
    size_t total = beginning;
    if (!lay_half_bars(request, layout, bars, repeat_bars, GROW_PLACE_FRAGMENT_1, GROW_PLACE_CADENTIAL, &total)) {
        return false;
    }
    for (size_t k = 0; k < layout->extension_count; k++) {
        GrowLayoutExtension *laid = &layout->extensions[k];
        if (laid->kind == PLAN_EXTENSION_STRETCH) {
            laid->start = layout->start[laid->place];
        } else if (laid->kind == PLAN_EXTENSION_EVADED) {
            if (!scaled(request->bar, total, &laid->start) || !scaled(request->bar, evaded_bars, &laid->length)) {
                return false;
            }
            total += evaded_bars;
        }
    }
    size_t ending = total - beginning;
    for (size_t k = 0; k < layout->extension_count; k++) {
        GrowLayoutExtension *laid = &layout->extensions[k];
        if (laid->kind == PLAN_EXTENSION_ADDITION) {
            if (!scaled(request->bar, total, &laid->start) || !scaled(request->bar, addition_bars, &laid->length)) {
                return false;
            }
            total += addition_bars;
        }
    }
    if (total != request->bars || !scaled(request->bar, beginning, &layout->beginning) ||
        !scaled(request->bar, ending, &layout->ending)) {
        return false;
    }
    layout->basic_bars = construction->idea_bars[GROW_IDEA_BASIC];
    layout->beginning_bars = beginning;
    layout->ending_bars = ending;
    layout->idea_beginning_bars = idea_beginning;
    layout->idea_ending_bars = idea_ending;
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
    layout.idea_beginning_bars = layout.beginning_bars;
    layout.idea_ending_bars = layout.ending_bars;
    *out = layout;
    return true;
}
