#include <string.h>

#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"

/* Chords a phrase-model progression has before its last: a tonic, up to two predominants, a dominant. */
#define PHRASE_MODEL_SLOTS 4
/* The second half starts four eighths into the theme. */
#define ENDING_START 4
#define ENDING_UNITS 4

/* The ideas of a continuation, in eighths of the theme. */
static const size_t IDEA_START[GROW_IDEA_COUNT] = {0, 2, 4, 5, 6};
static const size_t IDEA_UNITS[GROW_IDEA_COUNT] = {2, 2, 1, 1, 2};

/* The longest leading fragment of at most `wanted` notes that fits an idea of `room`; 0 when none does. */
static size_t fitting_fragment(GrowThemeSpec *spec, size_t slot, size_t wanted, PlanTime room) {
    GrowIdea idea = slot == 0 ? GROW_IDEA_FRAGMENT_1 : GROW_IDEA_FRAGMENT_2;
    size_t most = grow_idea_source(spec, idea)->note_count - 1;
    for (size_t length = wanted < most ? wanted : most; length >= 1; length--) {
        Motive result;
        spec->fragment_notes[slot] = length;
        if (grow_idea_develop(spec, idea, &result) && plan_time_compare(result.length, room) <= 0) {
            return length;
        }
    }
    return 0;
}

/* Develops an idea of the ending from m0 and keeps it if it fits `units` eighths of the theme. */
static bool keep_idea(GrowThemeSpec *spec, GrowIdea idea, PlanTime unit, size_t units) {
    if (!grow_idea_develop(spec, idea, &spec->developed[idea]) ||
        plan_time_compare(spec->developed[idea].length, grow_scaled(unit, units)) > 0) {
        return false;
    }
    spec->has_developed[idea] = true;
    return true;
}

static bool draw_continuation(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec, PlanTime unit) {
    size_t first = 2 + (size_t)fill_rng_below(rng, GROW_FRAGMENT_MAX - 1);
    size_t second = 2 + (size_t)fill_rng_below(rng, GROW_FRAGMENT_MAX - 1);
    size_t count = grow_idea_source(spec, GROW_IDEA_CADENTIAL)->note_count;
    size_t drop_most = count / 2 > 1 ? count / 2 : 1;
    spec->liquidate_drop = 1 + (size_t)fill_rng_below(rng, drop_most);
    if (spec->liquidate_drop >= count) {
        spec->liquidate_drop = count - 1;
    }
    if (fitting_fragment(spec, 0, first, grow_scaled(unit, IDEA_UNITS[GROW_IDEA_FRAGMENT_1])) == 0 ||
        fitting_fragment(spec, 1, second, grow_scaled(unit, IDEA_UNITS[GROW_IDEA_FRAGMENT_2])) == 0) {
        return false;
    }
    /* Each idea after the presentation is made from m0 and has to fit in the room it has. */
    for (GrowIdea idea = GROW_IDEA_FRAGMENT_1; idea < GROW_IDEA_COUNT; idea++) {
        if (!keep_idea(spec, idea, unit, IDEA_UNITS[idea])) {
            return false;
        }
    }

    /* The continuation is a phrase model if it has room for one, else the cadential progression alone. */
    size_t cells = request->bars / 2 * request->cells_per_bar;
    spec->ending_shape = cells >= harmony_phrase_min_cells(request->cadence, request->cells_per_bar)
                             ? PLAN_HARMONY_SHAPE_PHRASE_MODEL
                             : PLAN_HARMONY_SHAPE_CADENTIAL;
    /* Its chords change at least twice as fast as the presentation's longest could, and still deal the cells out. */
    size_t cap = spec->basic_cells / 2 > 1 ? spec->basic_cells / 2 : 1;
    size_t room = cells > request->cells_per_bar ? cells - request->cells_per_bar : 1;
    size_t least = (room + PHRASE_MODEL_SLOTS - 1) / PHRASE_MODEL_SLOTS;
    spec->ending_cap = (int)(cap > least ? cap : least);
    spec->ending_split = true;
    /* A key opens on a phrase model's pivot chord. */
    return !request->has_end_change || spec->ending_shape == PLAN_HARMONY_SHAPE_PHRASE_MODEL;
}

/* A consequent restates the basic idea and ends with a contrasting idea that is not the antecedent's. */
static bool draw_consequent(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec, PlanTime unit) {
    const MotiveDevSpec *avoid = spec->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT ? &spec->contrast : NULL;
    /* It ends in the cadence's tone: the tonic for a perfect authentic cadence, the third for an imperfect one. */
    PlanKey ends_in = request->has_end_change ? request->end_key : request->key;
    int tonic = ends_in.tonic_pitch_class;
    int want = request->cadence == PLAN_CADENCE_PAC ? tonic
               : request->cadence == PLAN_CADENCE_IAC ? (tonic + (ends_in.mode == PLAN_MODE_MINOR ? 3 : 4)) % 12
                                                      : -1;
    if (!keep_idea(spec, GROW_IDEA_RESTATED, unit, 2) ||
        !grow_draw_contrast(request, rng, spec, grow_idea_source(spec, GROW_IDEA_CONSEQUENT_CONTRAST), want, avoid,
                            &spec->consequent_contrast) ||
        !keep_idea(spec, GROW_IDEA_CONSEQUENT_CONTRAST, unit, 2)) {
        return false;
    }
    size_t cells = request->bars / 2 * request->cells_per_bar;
    spec->ending_shape = cells >= harmony_phrase_min_cells(request->cadence, request->cells_per_bar)
                             ? PLAN_HARMONY_SHAPE_PHRASE_MODEL
                             : PLAN_HARMONY_SHAPE_CADENTIAL;
    spec->ending_split = grow_split_possible(request, spec->ending_shape, request->cadence);
    return !request->has_end_change || spec->ending_shape == PLAN_HARMONY_SHAPE_PHRASE_MODEL;
}

/* A cadential ending is two ideas liquidated from m0, the second at least as much as the first. */
static bool draw_cadential(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec, PlanTime unit) {
    size_t count = grow_idea_source(spec, GROW_IDEA_CADENTIAL_2)->note_count;
    size_t most = count - 1;
    size_t drop_most = count / 2 > 1 ? count / 2 : 1;
    size_t first = 1 + (size_t)fill_rng_below(rng, drop_most);
    first = first > most ? most : first;
    size_t second = first + (size_t)fill_rng_below(rng, most - first + 1);
    spec->cadential_drop[0] = first;
    spec->cadential_drop[1] = second;
    spec->ending_shape = PLAN_HARMONY_SHAPE_CADENTIAL;
    spec->ending_split = grow_split_possible(request, spec->ending_shape, request->cadence);
    /* A cadential progression has no pivot to open a key with. */
    return !request->has_end_change && keep_idea(spec, GROW_IDEA_CADENTIAL_1, unit, 2) &&
           keep_idea(spec, GROW_IDEA_CADENTIAL_2, unit, 2);
}

bool grow_ending_draw(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec) {
    PlanTime unit = grow_unit(request);
    if (unit.num == 0) {
        return false;
    }
    switch (spec->pairing.ending) {
    case PLAN_THEME_ENDING_CONTINUATION:
        return draw_continuation(request, rng, spec, unit);
    case PLAN_THEME_ENDING_CONSEQUENT:
        return draw_consequent(request, rng, spec, unit);
    case PLAN_THEME_ENDING_CADENTIAL:
        return draw_cadential(request, rng, spec, unit);
    }
    return false;
}

/*
 * The second half is its own phrase after an antecedent, and a part of the theme's one phrase after any other
 * beginning. The container the ending's parts go in is that phrase, or a subphrase of the phrase when `subphrase` names
 * one. It is the node the chords are written over.
 */
static PlanResult make_container(GrowBuild *build, const char *phrase_name, const char *subphrase, NodeId *out) {
    PlanResult result = PLAN_OK;
    NodeId parent = build->phrase;
    if (parent == PLAN_NODE_NONE) {
        result = grow_add_node(build, build->request->section, PLAN_NODE_PHRASE, ENDING_START, ENDING_UNITS,
                               phrase_name, &parent);
        if (result == PLAN_OK) {
            result = plan_node_add_constraint(build->plan, parent, &build->cadence);
        }
        if (result != PLAN_OK || !subphrase) {
            *out = parent;
            return result;
        }
    }
    return result == PLAN_OK
               ? grow_add_node(build, parent, PLAN_NODE_SUBPHRASE, ENDING_START, ENDING_UNITS, subphrase, out)
               : result;
}

/* The chords over the container, and a melody span for each idea: the motive nodes m<slot> are made first. */
static PlanResult build_parts(GrowBuild *build, NodeId container, const PlanHarmonyShape *shape,
                              const char *const *names, const size_t *starts, const size_t *units, size_t first_slot,
                              size_t last_slot) {
    const GrowThemeSpec *spec = build->spec;
    NodeId ideas[GROW_IDEA_COUNT];
    NodeId motives[GROW_IDEA_COUNT];
    PlanMotiveRef refs[GROW_IDEA_COUNT];
    PlanResult result = PLAN_OK;

    for (size_t slot = first_slot; result == PLAN_OK && slot <= last_slot; slot++) {
        result = grow_add_node(build, container, PLAN_NODE_IDEA, starts[slot], units[slot], names[slot], &ideas[slot]);
    }
    /* A motive in the material for each idea, each made from m0. */
    for (size_t slot = first_slot; result == PLAN_OK && slot <= last_slot; slot++) {
        grow_idea_ref(spec, (GrowIdea)slot, grow_idea_node(build, (GrowIdea)slot), &refs[slot]);
        result = grow_add_motive(build, slot, &refs[slot], &motives[slot]);
    }
    if (result == PLAN_OK) {
        result = grow_add_chord_targets(build, container, shape, &build->cadence);
    }
    /* The melody to write, one span for each idea, the last asking for the cadence. */
    for (size_t slot = first_slot; result == PLAN_OK && slot <= last_slot; slot++) {
        PlanMotiveRef ref = refs[slot];
        ref.motive = motives[slot];
        result = grow_add_melody_target(build, ideas[slot], &ref, slot == last_slot ? &build->cadence : NULL);
    }
    return result;
}

static PlanResult build_continuation(GrowBuild *build) {
    static const char *const NAMES[GROW_IDEA_COUNT] = {NULL, NULL, "fragment 1", "fragment 2", "cadential"};
    NodeId continuation;
    PlanHarmonyShape shape;
    PlanResult result = make_container(build, "second phrase", "continuation", &continuation);
    memset(&shape, 0, sizeof(shape));
    shape.kind = build->spec->ending_shape;
    shape.starts_on = build->request->has_end_change ? PLAN_HARMONY_ANCHOR_PIVOT : PLAN_HARMONY_ANCHOR_ANY;
    shape.max_cells_per_chord = build->spec->ending_cap;
    shape.split_at_bars = build->spec->ending_split;
    return result == PLAN_OK ? build_parts(build, continuation, &shape, NAMES, IDEA_START, IDEA_UNITS,
                                           GROW_IDEA_FRAGMENT_1, GROW_IDEA_CADENTIAL)
                             : result;
}

/* The two ideas of a consequent or a cadential ending: the last two quarters of the second half, in eighths. */
static const size_t HALF_START[GROW_IDEA_COUNT] = {0, 0, 4, 6, 0};
static const size_t HALF_UNITS[GROW_IDEA_COUNT] = {0, 0, 2, 2, 0};

static PlanResult build_consequent(GrowBuild *build) {
    static const char *const NAMES[GROW_IDEA_COUNT] = {NULL, NULL, "consequent basic idea",
                                                       "consequent contrasting idea", NULL};
    NodeId consequent;
    PlanHarmonyShape shape;
    PlanResult result = make_container(build, "consequent", build->phrase == PLAN_NODE_NONE ? NULL : "consequent",
                                       &consequent);
    memset(&shape, 0, sizeof(shape));
    shape.kind = build->spec->ending_shape;
    shape.starts_on = build->request->has_end_change ? PLAN_HARMONY_ANCHOR_PIVOT : PLAN_HARMONY_ANCHOR_ANY;
    shape.split_at_bars = build->spec->ending_split;
    return result == PLAN_OK ? build_parts(build, consequent, &shape, NAMES, HALF_START, HALF_UNITS, GROW_IDEA_RESTATED,
                                           GROW_IDEA_CONSEQUENT_CONTRAST)
                             : result;
}

static PlanResult build_cadential(GrowBuild *build) {
    static const char *const NAMES[GROW_IDEA_COUNT] = {NULL, NULL, "cadential 1", "cadential 2", NULL};
    NodeId cadential;
    PlanHarmonyShape shape;
    PlanResult result = make_container(build, "second phrase", "cadential", &cadential);
    memset(&shape, 0, sizeof(shape));
    shape.kind = PLAN_HARMONY_SHAPE_CADENTIAL;
    shape.split_at_bars = build->spec->ending_split;
    return result == PLAN_OK ? build_parts(build, cadential, &shape, NAMES, HALF_START, HALF_UNITS, GROW_IDEA_CADENTIAL_1,
                                           GROW_IDEA_CADENTIAL_2)
                             : result;
}

PlanResult grow_ending_build(GrowBuild *build) {
    switch (build->spec->pairing.ending) {
    case PLAN_THEME_ENDING_CONTINUATION:
        return build_continuation(build);
    case PLAN_THEME_ENDING_CONSEQUENT:
        return build_consequent(build);
    case PLAN_THEME_ENDING_CADENTIAL:
        return build_cadential(build);
    }
    return PLAN_ERR_BAD_PAYLOAD;
}
