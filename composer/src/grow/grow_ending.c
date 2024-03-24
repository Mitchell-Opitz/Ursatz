#include <string.h>

#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"

/* Chords a phrase-model progression has before its last: a tonic, up to two predominants, a dominant. */
#define PHRASE_MODEL_SLOTS 4

/* The ideas of a continuation, in eighths of the theme. */
static const size_t IDEA_START[GROW_IDEA_COUNT] = {0, 2, 4, 5, 6};
static const size_t IDEA_UNITS[GROW_IDEA_COUNT] = {2, 2, 1, 1, 2};

/* The longest leading fragment of at most `wanted` notes that fits an idea of `room`; 0 when none does. */
static size_t fitting_fragment(GrowThemeSpec *spec, size_t slot, size_t wanted, PlanTime room) {
    GrowIdea idea = slot == 0 ? GROW_IDEA_FRAGMENT_1 : GROW_IDEA_FRAGMENT_2;
    size_t most = spec->source.note_count - 1;
    for (size_t length = wanted < most ? wanted : most; length >= 1; length--) {
        Motive result;
        spec->fragment_notes[slot] = length;
        if (grow_idea_develop(spec, idea, &result) && plan_time_compare(result.length, room) <= 0) {
            return length;
        }
    }
    return 0;
}

bool grow_ending_draw(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec) {
    PlanTime unit = {0, 1};
    if (plan_time_make(request->bar.num * (long)request->bars, request->bar.den * 8, &unit) != PLAN_OK) {
        return false;
    }
    size_t first = 2 + (size_t)fill_rng_below(rng, GROW_FRAGMENT_MAX - 1);
    size_t second = 2 + (size_t)fill_rng_below(rng, GROW_FRAGMENT_MAX - 1);
    size_t count = spec->source.note_count;
    size_t drop_most = count / 2 > 1 ? count / 2 : 1;
    spec->liquidate_drop = 1 + (size_t)fill_rng_below(rng, drop_most);
    if (spec->liquidate_drop >= count) {
        spec->liquidate_drop = count - 1;
    }
    if (fitting_fragment(spec, 0, first, grow_scaled(unit, IDEA_UNITS[GROW_IDEA_FRAGMENT_1])) == 0 ||
        fitting_fragment(spec, 1, second, grow_scaled(unit, IDEA_UNITS[GROW_IDEA_FRAGMENT_2])) == 0) {
        return false;
    }
    /* Each idea after the basic idea is made from m0 and has to fit in the room it has. */
    for (GrowIdea idea = GROW_IDEA_REPETITION; idea < GROW_IDEA_COUNT; idea++) {
        if (!grow_idea_develop(spec, idea, &spec->developed[idea]) ||
            plan_time_compare(spec->developed[idea].length, grow_scaled(unit, IDEA_UNITS[idea])) > 0) {
            return false;
        }
        spec->has_developed[idea] = true;
    }

    /* The continuation is a phrase model if it has room for one, else the cadential progression alone. */
    size_t cells = request->bars / 2 * request->cells_per_bar;
    spec->continuation_shape = cells >= harmony_phrase_min_cells(request->cadence, request->cells_per_bar)
                                   ? PLAN_HARMONY_SHAPE_PHRASE_MODEL
                                   : PLAN_HARMONY_SHAPE_CADENTIAL;
    /* Its chords change at least twice as fast as the presentation's longest could, and still deal the cells out. */
    size_t cap = spec->basic_cells / 2 > 1 ? spec->basic_cells / 2 : 1;
    size_t room = cells > request->cells_per_bar ? cells - request->cells_per_bar : 1;
    size_t least = (room + PHRASE_MODEL_SLOTS - 1) / PHRASE_MODEL_SLOTS;
    spec->continuation_cap = (int)(cap > least ? cap : least);
    return true;
}

PlanResult grow_ending_build(GrowBuild *build) {
    const GrowThemeSpec *spec = build->spec;
    const GrowThemeRequest *request = build->request;
    NodeId continuation;
    NodeId ideas[GROW_IDEA_COUNT];
    NodeId motives[GROW_IDEA_COUNT];
    PlanMotiveRef refs[GROW_IDEA_COUNT];
    static const char *const NAMES[GROW_IDEA_COUNT] = {NULL, NULL, "fragment 1", "fragment 2", "cadential"};

    PlanResult result = grow_add_node(build, build->phrase, PLAN_NODE_SUBPHRASE, 4, 4, "continuation", &continuation);
    for (GrowIdea idea = GROW_IDEA_FRAGMENT_1; result == PLAN_OK && idea < GROW_IDEA_COUNT; idea++) {
        result = grow_add_node(build, continuation, PLAN_NODE_IDEA, IDEA_START[idea], IDEA_UNITS[idea], NAMES[idea],
                               &ideas[idea]);
    }

    /* A motive in the material for each idea of the continuation, each made from m0. */
    for (GrowIdea idea = GROW_IDEA_FRAGMENT_1; result == PLAN_OK && idea < GROW_IDEA_COUNT; idea++) {
        grow_idea_ref(spec, idea, request->source, &refs[idea]);
        result = grow_add_motive(build, idea, &refs[idea], &motives[idea]);
    }

    /* The harmony and bass to write, one pair for the whole continuation. */
    PlanHarmonyShape shape;
    memset(&shape, 0, sizeof(shape));
    shape.kind = spec->continuation_shape;
    shape.max_cells_per_chord = spec->continuation_cap;
    shape.split_at_bars = true;
    if (result == PLAN_OK) {
        result = grow_add_chord_targets(build, continuation, &shape, &build->cadence);
    }

    /* The melody to write, one span for each idea, the last asking for the cadence. */
    for (GrowIdea idea = GROW_IDEA_FRAGMENT_1; result == PLAN_OK && idea < GROW_IDEA_COUNT; idea++) {
        PlanMotiveRef ref = refs[idea];
        ref.motive = motives[idea];
        result = grow_add_melody_target(build, ideas[idea], &ref, idea == GROW_IDEA_CADENTIAL ? &build->cadence : NULL);
    }
    return result;
}
