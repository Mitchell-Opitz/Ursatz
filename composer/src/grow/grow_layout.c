#include <ursatz_composer/theme_shape.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/motive_develop.h>

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
    layout->main_ending_bars = ending - evaded_bars;
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
    layout.main_ending_bars = layout.ending_bars;
    *out = layout;
    return true;
}

/* The motive of the idea at a place as the draw made it: its liquidation when it has one, else its development. */
static bool placed_motive(const GrowThemeSpec *spec, GrowPlace place, const Motive **out) {
    if (spec->has_liquidated[place]) {
        *out = &spec->liquidated[place];
        return true;
    }
    if (spec->has_developed[place]) {
        *out = &spec->developed[place];
        return true;
    }
    return false;
}

/* Whether two motives have the same notes: count, pitches and durations. */
static bool same_notes(const Motive *a, const Motive *b) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (motive_pitch(a, i) != motive_pitch(b, i) || rational_cmp(a->notes[i].duration, b->notes[i].duration) != 0) {
            return false;
        }
    }
    return true;
}

/* The pitches a repeat may go to, so the melody has room to place it. */
#define EXTENSION_LOW 50
#define EXTENSION_HIGH 90

/*
 * The motive of a repeat of `base` (R-468): one step down in the key's scale, or, varied, its last interval widened by two
 * semitones with the contour kept, else narrowed by two, whichever differs from the idea. It must fit `room`.
 */
static bool repeat_motive(const Motive *base, bool varied, Rational room, Motive *out) {
    for (int attempt = 0; attempt < (varied ? 2 : 1); attempt++) {
        PlanMotiveRef ref;
        memset(&ref, 0, sizeof(ref));
        ref.has_step = true;
        if (varied) {
            ref.step.kind = MOTIVE_RELATION_INTERVAL_CHANGE;
            ref.step.value[0] = (long)base->note_count - 1;
            ref.step.value[1] = attempt == 0 ? 2 : -2;
            ref.step.value[2] = 1;
        } else {
            ref.step.kind = MOTIVE_RELATION_TRANSPOSE;
            ref.step.value[0] = 1;
            ref.step.value[1] = -1;
        }
        Motive result;
        if (base->note_count >= (varied ? 2u : 1u) && grow_develop_motive(base, &ref, &result) &&
            (!varied || !same_notes(&result, base)) && rational_cmp(result.length, room) <= 0 &&
            grow_motive_in_range(&result, EXTENSION_LOW, EXTENSION_HIGH, false)) {
            *out = result;
            return true;
        }
    }
    return false;
}

/*
 * The addition's motive: the longest trailing segment of the last idea's motive, two notes or more and fewer than it has,
 * that fits `room` and ends on `want`, the pitch class of the cadence's tone.
 */
static bool addition_motive(const Motive *last, Rational room, int want, Motive *out) {
    for (size_t notes = last->note_count - 1; last->note_count > 2 && notes >= 2; notes--) {
        MotiveRelationStep step;
        Motive result;
        memset(&step, 0, sizeof(step));
        step.kind = MOTIVE_RELATION_FRAGMENT;
        step.value[0] = MOTIVE_FRAGMENT_TRAILING;
        step.value[1] = (long)notes;
        step.value[2] = 1;
        if (motive_develop(last, &step, &result) == MOTIVE_DEV_OK && rational_cmp(result.length, room) <= 0 &&
            (want < 0 || ((motive_pitch(&result, result.note_count - 1) % 12) + 12) % 12 == want)) {
            *out = result;
            return true;
        }
    }
    return false;
}

bool grow_extensions_draw(const GrowThemeRequest *request, GrowThemeSpec *spec) {
    const GrowLayout *layout = &spec->layout;
    AnalysisKey ends_in = request->has_end_change ? request->end_key : request->key;
    int tonic = analysis_key_tonic_pitch_class(&ends_in);
    int want = request->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC ? tonic
               : request->cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC ? (tonic + (ends_in.mode == KEY_STRUCTURE_MODE_MINOR ? 3 : 4)) % 12
                                                                      : -1;
    for (size_t k = 0; k < layout->extension_count; k++) {
        const GrowLayoutExtension *extension = &layout->extensions[k];
        const Motive *base;
        Motive made;
        if (extension->kind == PLAN_EXTENSION_REPEAT) {
            if (!placed_motive(spec, extension->place, &base) ||
                !repeat_motive(base, extension->varied, extension->length, &made)) {
                return false;
            }
        } else if (extension->kind == PLAN_EXTENSION_ADDITION) {
            if (!placed_motive(spec, extension->place, &base) || !addition_motive(base, extension->length, want, &made)) {
                return false;
            }
        } else {
            continue;
        }
        spec->extended[k] = made;
        spec->has_extended[k] = true;
    }
    /*
     * The idea after an evaded cadence restates the motive of the idea before it exactly (the last repeat after the theme's
     * last idea, else that idea): it must have the room.
     */
    for (size_t k = 0; k < layout->extension_count; k++) {
        const GrowLayoutExtension *extension = &layout->extensions[k];
        const Motive *base;
        if (extension->kind != PLAN_EXTENSION_EVADED) {
            continue;
        }
        if (!placed_motive(spec, extension->place, &base)) {
            return false;
        }
        for (size_t other = 0; other < layout->extension_count; other++) {
            if (layout->extensions[other].kind == PLAN_EXTENSION_REPEAT && layout->extensions[other].place == extension->place &&
                spec->has_extended[other]) {
                base = &spec->extended[other];
            }
        }
        if (rational_cmp(base->length, extension->length) > 0) {
            return false;
        }
    }
    return true;
}

size_t grow_extended_bars(const GrowThemeSpec *spec, Rational bar) {
    size_t bars = 0;
    for (size_t k = 0; k < PLAN_CONSTRUCTION_EXTENSIONS; k++) {
        if (spec->has_extended[k]) {
            size_t wanted = grow_motive_layout_bars(&spec->extended[k], bar);
            bars += wanted > 0 ? wanted : 1;
        }
    }
    return bars;
}

size_t grow_extension_material_bars(const PlanThemePairing *pairing, const PlanConstruction *construction) {
    size_t bars = 0;
    for (size_t k = 0; k < construction->extension_count && k < PLAN_CONSTRUCTION_EXTENSIONS; k++) {
        const PlanExtension *extension = &construction->extensions[k];
        if (extension->kind == PLAN_EXTENSION_REPEAT) {
            size_t own = extension->idea < PLAN_CONSTRUCTION_IDEAS && theme_shape_idea_in(pairing, (GrowIdea)extension->idea)
                             ? construction->idea_bars[extension->idea]
                             : 1;
            size_t wanted = extension->bars ? extension->bars : own;
            bars += wanted > 0 ? wanted : 1;
        } else if (extension->kind == PLAN_EXTENSION_ADDITION) {
            bars += extension->bars ? extension->bars : 1;
        }
    }
    return bars;
}

PlanResult grow_add_repeat_node(GrowBuild *build, NodeId container, size_t k, NodeId *out) {
    const GrowLayoutExtension *extension = &build->layout.extensions[k];
    const ThemeShapeIdea *row = theme_shape_place_in(&build->spec->pairing, (unsigned)extension->place);
    char name[64];
    snprintf(name, sizeof(name), "%s repeated", row ? row->name : "idea");
    return grow_add_node(build, container, PLAN_NODE_IDEA, extension->start, extension->length, name, out);
}

void grow_extension_ref(const GrowBuild *build, size_t k, PlanMotiveRef *out) {
    memset(out, 0, sizeof(*out));
    out->motive = build->extension_node[k];
    out->has_step = true;
    out->step.kind = MOTIVE_RELATION_EXACT;
}

/*
 * The bars of each place of the construction (with its stretches), of the repeats that follow it, and of the evaded idea
 * and the addition after the last place, in the order the extensions are listed.
 */
static void construction_places(const PlanThemePairing *pairing, const PlanConstruction *construction,
                                size_t bars[GROW_PLACE_COUNT], size_t repeated[GROW_PLACE_COUNT], size_t *evaded,
                                size_t *addition) {
    GrowPlace last = last_place(pairing);
    memset(bars, 0, sizeof(size_t) * GROW_PLACE_COUNT);
    memset(repeated, 0, sizeof(size_t) * GROW_PLACE_COUNT);
    *evaded = 0;
    *addition = 0;
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        const ThemeShapeIdea *row = theme_shape_idea_in(pairing, (GrowIdea)idea);
        if (row) {
            bars[row->place] = construction->idea_bars[idea];
        }
    }
    for (size_t k = 0; k < construction->extension_count && k < PLAN_CONSTRUCTION_EXTENSIONS; k++) {
        const PlanExtension *extension = &construction->extensions[k];
        GrowPlace place = last;
        if ((extension->kind == PLAN_EXTENSION_REPEAT || extension->kind == PLAN_EXTENSION_STRETCH) &&
            !grow_idea_place(pairing, (GrowIdea)extension->idea, &place)) {
            continue;
        }
        switch (extension->kind) {
        case PLAN_EXTENSION_REPEAT:
            repeated[place] += extension->bars ? extension->bars : bars[place];
            break;
        case PLAN_EXTENSION_STRETCH:
            bars[place] += extension->bars ? extension->bars : 1;
            break;
        case PLAN_EXTENSION_EVADED:
            *evaded = extension->bars ? extension->bars : bars[last];
            break;
        case PLAN_EXTENSION_ADDITION:
            *addition = extension->bars ? extension->bars : 1;
            break;
        }
    }
}

void grow_construction_bars(const PlanThemePairing *pairing, const PlanConstruction *construction, GrowConstructionBars *out) {
    size_t bars[GROW_PLACE_COUNT];
    size_t repeated[GROW_PLACE_COUNT];
    memset(out, 0, sizeof(*out));
    construction_places(pairing, construction, bars, repeated, &out->evaded, &out->addition);
    for (size_t place = 0; place < GROW_PLACE_COUNT; place++) {
        size_t *half = place <= GROW_PLACE_REPETITION ? &out->beginning : &out->ending;
        *half += bars[place] + repeated[place];
    }
}

size_t grow_construction_extra_bars(const PlanThemePairing *pairing, const PlanConstruction *construction) {
    GrowConstructionBars halves;
    size_t ideas = 0;
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        ideas += theme_shape_idea_in(pairing, (GrowIdea)idea) ? construction->idea_bars[idea] : 0;
    }
    grow_construction_bars(pairing, construction, &halves);
    return halves.beginning + halves.ending + halves.evaded + halves.addition - ideas;
}

/* --- choosing a construction (R-469) ------------------------------------ */

/* Adds an extension to the construction; false when it holds the most there are. */
static bool add_extension(PlanConstruction *construction, PlanExtensionKind kind, GrowIdea idea, size_t bars, bool varied) {
    if (construction->extension_count >= PLAN_CONSTRUCTION_EXTENSIONS || bars > 255) {
        return false;
    }
    PlanExtension *extension = &construction->extensions[construction->extension_count++];
    extension->kind = kind;
    extension->idea = (unsigned char)idea;
    extension->bars = (unsigned char)bars;
    extension->varied = varied;
    return true;
}

/* The ending's excess over a consequent's regular length: a stretch for one bar, a varied repeat and a stretch for more. */
static bool choose_consequent_ending(PlanConstruction *construction, size_t b, size_t h1, long excess) {
    if (excess == 0) {
        return true;
    }
    if (excess == 1) {
        return add_extension(construction, PLAN_EXTENSION_STRETCH, GROW_IDEA_CONSEQUENT_CONTRASTING, 1, false);
    }
    long repeated = (long)(h1 - b);
    long left = excess - repeated;
    return left >= 0 && left <= 4 &&
           add_extension(construction, PLAN_EXTENSION_REPEAT, GROW_IDEA_CONSEQUENT_CONTRASTING, (size_t)repeated, true) &&
           (left == 0 || add_extension(construction, PLAN_EXTENSION_STRETCH, GROW_IDEA_CONSEQUENT_CONTRASTING, (size_t)left, false));
}

/* A continuation's excess: a sequential repeat of fragment 2 (a stretch of the cadential idea when fragments are longer than a bar). */
static bool choose_continuation_ending(PlanConstruction *construction, size_t b, size_t f, long excess, CadenceType cadence) {
    bool authentic = cadence == CADENCE_TYPE_PERFECT_AUTHENTIC || cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC;
    if (excess == 0) {
        return true;
    }
    if (excess > 4 || (excess >= 2 && !authentic)) {
        return false;
    }
    bool repeat = excess == 1 || excess == 3;
    bool evaded = excess >= 2;
    if (repeat) {
        bool added = f == 1 ? add_extension(construction, PLAN_EXTENSION_REPEAT, GROW_IDEA_FRAGMENT_2, 1, false)
                            : add_extension(construction, PLAN_EXTENSION_STRETCH, GROW_IDEA_CADENTIAL, 1, false);
        if (!added) {
            return false;
        }
    }
    return (!evaded || add_extension(construction, PLAN_EXTENSION_EVADED, GROW_IDEA_CADENTIAL, b, false)) &&
           (excess != 4 || add_extension(construction, PLAN_EXTENSION_ADDITION, GROW_IDEA_CADENTIAL, 2, false));
}

/* A cadential ending's excess: cadential 2 stretched for one bar, the cadence evaded for two. */
static bool choose_cadential_ending(PlanConstruction *construction, size_t b, long excess, CadenceType cadence) {
    if (excess == 0) {
        return true;
    }
    if (excess == 1) {
        return add_extension(construction, PLAN_EXTENSION_STRETCH, GROW_IDEA_CADENTIAL_2, 1, false);
    }
    return excess == 2 && cadence != CADENCE_TYPE_HALF &&
           add_extension(construction, PLAN_EXTENSION_EVADED, GROW_IDEA_CADENTIAL_2, b, false);
}

bool grow_construction_choose(const Motive *motive, const PlanMeter *meter, size_t bars, const PlanThemePairing *pairing,
                              CadenceType cadence, PlanConstruction *out) {
    if (!motive || !meter || !pairing || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    size_t motif_bars = grow_motive_layout_bars(motive, grow_bar_length(meter));
    if ((bars == 4 || bars == 8) && motif_bars <= 2) {
        return false;
    }
    /* The basic idea: the motif's bars from 3, else 2 from 6 bars up (R-469 gives 3+3 from 6 bars), else 1. */
    size_t b = motif_bars >= 3 ? motif_bars : (bars >= 6 ? 2 : 1);
    out->reason = motif_bars >= 3 ? PLAN_CONSTRUCTION_FROM_MOTIVE : PLAN_CONSTRUCTION_FROM_LENGTH;
    if (motif_bars > b || bars < 3 || bars > 16) {
        memset(out, 0, sizeof(*out));
        return false;
    }
    size_t h1 = pairing->beginning == PLAN_THEME_BEGINNING_PRESENTATION ? 2 * b
                : (2 * b < bars && 2 * b <= bars - 2 * b) ? 2 * b
                                                          : bars / 2;
    bool ok = h1 > b && h1 < bars;
    size_t h2 = ok ? bars - h1 : 0;
    long excess = 0;
    if (ok) {
        out->idea_bars[GROW_IDEA_BASIC] = (unsigned char)b;
        if (pairing->beginning == PLAN_THEME_BEGINNING_PRESENTATION) {
            out->idea_bars[GROW_IDEA_REPETITION] = (unsigned char)b;
        } else {
            out->idea_bars[GROW_IDEA_CONTRASTING] = (unsigned char)(h1 - b);
        }
        switch (pairing->ending) {
        case PLAN_THEME_ENDING_CONSEQUENT:
            out->idea_bars[GROW_IDEA_CONSEQUENT_BASIC] = (unsigned char)b;
            out->idea_bars[GROW_IDEA_CONSEQUENT_CONTRASTING] = (unsigned char)(h1 - b);
            excess = (long)h2 - (long)h1;
            ok = excess >= 0 && choose_consequent_ending(out, b, h1, excess);
            break;
        case PLAN_THEME_ENDING_CONTINUATION: {
            size_t f = b / 2 > 1 ? b / 2 : 1;
            out->idea_bars[GROW_IDEA_FRAGMENT_1] = (unsigned char)f;
            out->idea_bars[GROW_IDEA_FRAGMENT_2] = (unsigned char)f;
            out->idea_bars[GROW_IDEA_CADENTIAL] = (unsigned char)b;
            excess = (long)h2 - (long)(2 * f + b);
            ok = b >= 2 && excess >= 0 && choose_continuation_ending(out, b, f, excess, cadence);
            break;
        }
        case PLAN_THEME_ENDING_CADENTIAL:
            out->idea_bars[GROW_IDEA_CADENTIAL_1] = (unsigned char)b;
            out->idea_bars[GROW_IDEA_CADENTIAL_2] = (unsigned char)b;
            excess = (long)h2 - (long)(2 * b);
            ok = excess >= 0 && choose_cadential_ending(out, b, excess, cadence);
            break;
        default:
            ok = false;
            break;
        }
    }
    /* An antecedent's first cadence is the default, HC: the request's own is checked when the plan is built. */
    ok = ok && grow_construction_buildable(meter, bars, pairing, cadence, CADENCE_TYPE_HALF, out);
    if (!ok) {
        memset(out, 0, sizeof(*out));
    }
    return ok;
}

/* --- describing a construction (R-469) ---------------------------------- */

/* "bar 7", "bars 9 and 10", "bars 3 to 5" for the 0-based bars first to first + count - 1. */
static void bars_text(size_t first, size_t count, char *text, size_t size) {
    if (count <= 1) {
        snprintf(text, size, "bar %zu", first + 1);
    } else if (count == 2) {
        snprintf(text, size, "bars %zu and %zu", first + 1, first + 2);
    } else {
        snprintf(text, size, "bars %zu to %zu", first + 1, first + count);
    }
}

/* Appends the formatted text to text at *used, as much as fits. */
static void put(char *text, size_t size, size_t *used, const char *format, ...) {
    if (*used >= size) {
        return;
    }
    va_list args;
    va_start(args, format);
    int written = vsnprintf(text + *used, size - *used, format, args);
    va_end(args);
    if (written > 0) {
        *used += (size_t)written;
        if (*used >= size) {
            *used = size - 1;
        }
    }
}

/* The clause that names one extension and the bars it takes, 0-based `first` and `count`. */
static void extension_clause(const PlanExtension *extension, size_t first, size_t count, size_t cadence_bar, char *text,
                             size_t size) {
    char at[32];
    bars_text(first, count, at, sizeof(at));
    const char *name = grow_idea_name((GrowIdea)extension->idea);
    switch (extension->kind) {
    case PLAN_EXTENSION_REPEAT:
        if (extension->idea == GROW_IDEA_CONSEQUENT_CONTRASTING) {
            snprintf(text, size, "the consequent repeats its contrasting idea, %s (%s)", extension->varied ? "varied" : "as it is", at);
        } else if (extension->varied) {
            snprintf(text, size, "%s is repeated, varied (%s)", name, at);
        } else {
            snprintf(text, size, "%s is repeated a step lower (%s)", name, at);
        }
        break;
    case PLAN_EXTENSION_STRETCH:
        snprintf(text, size, "the %s is stretched to %zu bars (%s)", name, count, at);
        break;
    case PLAN_EXTENSION_ADDITION:
        snprintf(text, size, "a %zu-bar addition follows (%s)", count, at);
        break;
    case PLAN_EXTENSION_EVADED:
        snprintf(text, size, "the cadence in bar %zu is evaded (V to vi) and the cadential idea returns to close in bar %zu",
                 cadence_bar, first + count);
        break;
    }
}

void grow_construction_spans(const PlanThemePairing *pairing, const PlanConstruction *construction, GrowConstructionSpans *out) {
    memset(out, 0, sizeof(*out));
    if (!pairing || !construction) {
        return;
    }
    size_t bars[GROW_PLACE_COUNT];
    size_t repeated[GROW_PLACE_COUNT];
    size_t evaded;
    size_t addition;
    construction_places(pairing, construction, bars, repeated, &evaded, &addition);
    GrowConstructionBars halves;
    grow_construction_bars(pairing, construction, &halves);
    out->halves[0] = halves.beginning;
    out->halves[1] = halves.ending + halves.evaded;
    size_t start[GROW_PLACE_COUNT];
    size_t repeat_start[GROW_PLACE_COUNT];
    size_t position = 0;
    for (size_t place = 0; place < GROW_PLACE_COUNT; place++) {
        start[place] = position;
        position += bars[place];
        repeat_start[place] = position;
        position += repeated[place];
    }
    size_t evaded_start = position;
    size_t addition_start = position + evaded;
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        const ThemeShapeIdea *row = theme_shape_idea_in(pairing, (GrowIdea)idea);
        if (row) {
            out->idea_first[idea] = start[row->place];
            out->idea_bars[idea] = bars[row->place];
        }
    }
    size_t repeat_seen[GROW_PLACE_COUNT] = {0};
    for (size_t k = 0; k < construction->extension_count && k < PLAN_CONSTRUCTION_EXTENSIONS; k++) {
        const PlanExtension *extension = &construction->extensions[k];
        GrowPlace place;
        switch (extension->kind) {
        case PLAN_EXTENSION_REPEAT:
            if (grow_idea_place(pairing, (GrowIdea)extension->idea, &place)) {
                out->extension_bars[k] = extension->bars ? extension->bars : bars[place];
                out->extension_first[k] = repeat_start[place] + repeat_seen[place];
                repeat_seen[place] += out->extension_bars[k];
            }
            break;
        case PLAN_EXTENSION_STRETCH:
            if (grow_idea_place(pairing, (GrowIdea)extension->idea, &place)) {
                out->extension_first[k] = start[place];
                out->extension_bars[k] = bars[place];
            }
            break;
        case PLAN_EXTENSION_EVADED:
            out->extension_first[k] = evaded_start;
            out->extension_bars[k] = evaded;
            break;
        case PLAN_EXTENSION_ADDITION:
            out->extension_first[k] = addition_start;
            out->extension_bars[k] = addition;
            break;
        }
    }
}

size_t grow_construction_describe(const PlanConstruction *construction, const PlanThemePairing *pairing, char *text,
                                  size_t size) {
    if (!text || size == 0) {
        return 0;
    }
    text[0] = '\0';
    if (!construction || !pairing) {
        return 0;
    }
    size_t bars[GROW_PLACE_COUNT];
    size_t repeated[GROW_PLACE_COUNT];
    size_t evaded;
    size_t addition;
    construction_places(pairing, construction, bars, repeated, &evaded, &addition);
    GrowConstructionBars halves;
    grow_construction_bars(pairing, construction, &halves);
    /* Where each place, each repeat after it, the evaded idea and the addition start (0-based bars). */
    size_t start[GROW_PLACE_COUNT];
    size_t repeat_start[GROW_PLACE_COUNT];
    size_t position = 0;
    for (size_t place = 0; place < GROW_PLACE_COUNT; place++) {
        start[place] = position;
        position += bars[place];
        repeat_start[place] = position;
        position += repeated[place];
    }
    size_t evaded_start = position;
    size_t addition_start = position + evaded;
    size_t total = addition_start + addition;
    int archetype = grow_pairing_archetype(pairing);
    const char *form = archetype == PLAN_ARCHETYPE_PERIOD ? "period" : archetype == PLAN_ARCHETYPE_SENTENCE ? "sentence" : "hybrid";
    size_t first_half = halves.beginning;
    size_t second_half = halves.ending + halves.evaded;
    size_t used = 0;
    put(text, size, &used, "%zu+%zu %s", first_half, second_half, form);
    /* The extension clauses, each from the bars its extension takes. */
    char clauses[PLAN_CONSTRUCTION_EXTENSIONS][160];
    size_t clause_count = 0;
    size_t repeat_seen[GROW_PLACE_COUNT] = {0};
    for (size_t k = 0; k < construction->extension_count && k < PLAN_CONSTRUCTION_EXTENSIONS; k++) {
        const PlanExtension *extension = &construction->extensions[k];
        GrowPlace place;
        size_t first = 0;
        size_t count = 0;
        switch (extension->kind) {
        case PLAN_EXTENSION_REPEAT:
            if (!grow_idea_place(pairing, (GrowIdea)extension->idea, &place)) {
                continue;
            }
            count = extension->bars ? extension->bars : bars[place];
            first = repeat_start[place] + repeat_seen[place];
            repeat_seen[place] += count;
            break;
        case PLAN_EXTENSION_STRETCH:
            if (!grow_idea_place(pairing, (GrowIdea)extension->idea, &place)) {
                continue;
            }
            first = start[place];
            count = bars[place];
            break;
        case PLAN_EXTENSION_EVADED:
            first = evaded_start;
            count = evaded;
            break;
        case PLAN_EXTENSION_ADDITION:
            first = addition_start;
            count = addition;
            break;
        }
        extension_clause(extension, first, count, evaded_start, clauses[clause_count], sizeof(clauses[clause_count]));
        clause_count++;
    }
    const char *noun = archetype == PLAN_ARCHETYPE_PERIOD ? "phrase" : "half";
    bool from_length = construction->reason == PLAN_CONSTRUCTION_FROM_LENGTH;
    char halves_text[64];
    if (first_half == second_half) {
        snprintf(halves_text, sizeof(halves_text), "each %s %s%zu", noun, from_length ? "is " : "", first_half);
    } else {
        snprintf(halves_text, sizeof(halves_text), "the %ss are %zu and %zu", noun, first_half, second_half);
    }
    const char *joint = ", ";
    switch (construction->reason) {
    case PLAN_CONSTRUCTION_GIVEN:
        put(text, size, &used, " as given");
        joint = ": ";
        break;
    case PLAN_CONSTRUCTION_FROM_MOTIVE: {
        size_t basic = construction->idea_bars[GROW_IDEA_BASIC];
        put(text, size, &used, ": the motif is %zu bars, so the basic idea is %zu bars and %s", basic, basic, halves_text);
        break;
    }
    case PLAN_CONSTRUCTION_FROM_LENGTH:
        put(text, size, &used, ": the section is %zu bars, so ", total);
        if (clause_count == 0) {
            put(text, size, &used, "%s", halves_text);
        }
        joint = "";
        break;
    }
    for (size_t k = 0; k < clause_count; k++) {
        put(text, size, &used, "%s%s", k == 0 ? joint : " and ", clauses[k]);
    }
    put(text, size, &used, ".");
    return used;
}

size_t grow_section_describe(const PiecePlan *plan, NodeId section, char *text, size_t size) {
    const PlanConstraint *construction = NULL;
    const PlanConstraint *pairing = NULL;
    if (text && size > 0) {
        text[0] = '\0';
    }
    for (size_t i = 0; plan && i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_CONSTRUCTION) {
            construction = constraint;
        } else if (constraint->kind == PLAN_CONSTRAINT_THEME_PAIRING) {
            pairing = constraint;
        }
    }
    if (!construction || !pairing) {
        return 0;
    }
    return grow_construction_describe(&construction->params.construction, &pairing->params.theme_pairing, text, size);
}
