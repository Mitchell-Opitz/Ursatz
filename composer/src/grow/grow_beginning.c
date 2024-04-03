#include <string.h>

#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"

/* The ideas of the first half, in eighths of the theme. */
#define BASIC_START 0
#define BASIC_UNITS 2
#define SECOND_START 2
#define SECOND_UNITS 2
#define BEGINNING_UNITS 4

/* The shape of the chords of the first half: a phrase model if it has room for its cadence, else the cadential one. */
static PlanHarmonyShapeKind shape_for(const GrowThemeRequest *request, PlanCadenceType cadence) {
    size_t cells = request->bars / 2 * request->cells_per_bar;
    return cells >= harmony_phrase_min_cells(cadence, request->cells_per_bar) ? PLAN_HARMONY_SHAPE_PHRASE_MODEL
                                                                               : PLAN_HARMONY_SHAPE_CADENTIAL;
}

/* The contrasting idea is made from m0 and has to fit its room. */
static bool draw_contrasting_idea(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec) {
    return grow_draw_contrast(request, rng, spec, -1, NULL, &spec->contrast) &&
           grow_idea_develop(spec, GROW_IDEA_CONTRAST, &spec->developed[GROW_IDEA_CONTRAST]) &&
           (spec->has_developed[GROW_IDEA_CONTRAST] = true);
}

bool grow_beginning_draw(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec) {
    switch (spec->pairing.beginning) {
    case PLAN_THEME_BEGINNING_PRESENTATION: {
        /* Statement-response goes from tonic to dominant and back, which needs a cell for each. */
        bool can_respond = spec->basic_cells >= 2;
        uint64_t response = fill_rng_below(rng, 2);
        spec->statement_response = can_respond && response == 1;
        if (!grow_idea_develop(spec, GROW_IDEA_REPETITION, &spec->developed[GROW_IDEA_REPETITION]) ||
            plan_time_compare(spec->developed[GROW_IDEA_REPETITION].length,
                              grow_scaled(grow_unit(request), SECOND_UNITS)) > 0) {
            return false;
        }
        spec->has_developed[GROW_IDEA_REPETITION] = true;
        return true;
    }
    case PLAN_THEME_BEGINNING_ANTECEDENT:
        spec->beginning_shape = shape_for(request, spec->first_cadence);
        spec->beginning_split = grow_split_possible(request, spec->beginning_shape, spec->first_cadence);
        return draw_contrasting_idea(request, rng, spec);
    case PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA:
        break;
    }
    return false;
}

/* The one phrase the parts of a theme go in, with the cadence it ends in. */
static PlanResult make_phrase(GrowBuild *build) {
    PlanResult result =
        grow_add_node(build, build->request->section, PLAN_NODE_PHRASE, 0, 8, "phrase", &build->phrase);
    return result == PLAN_OK ? plan_node_add_constraint(build->plan, build->phrase, &build->cadence) : result;
}

/* A presentation: a basic idea and its repetition, each over a prolongation. */
static PlanResult build_presentation(GrowBuild *build) {
    const GrowThemeSpec *spec = build->spec;
    const GrowThemeRequest *request = build->request;
    NodeId presentation;
    NodeId ideas[2];
    NodeId motive;
    PlanMotiveRef refs[2];

    PlanResult result = make_phrase(build);
    if (result == PLAN_OK) {
        result = grow_add_node(build, build->phrase, PLAN_NODE_SUBPHRASE, 0, BEGINNING_UNITS, "presentation",
                               &presentation);
    }
    if (result == PLAN_OK) {
        result = grow_add_node(build, presentation, PLAN_NODE_IDEA, BASIC_START, BASIC_UNITS, "basic idea", &ideas[0]);
    }
    if (result == PLAN_OK) {
        result = grow_add_node(build, presentation, PLAN_NODE_IDEA, SECOND_START, SECOND_UNITS, "repetition",
                               &ideas[1]);
    }
    for (GrowIdea idea = GROW_IDEA_BASIC; idea <= GROW_IDEA_REPETITION; idea++) {
        grow_idea_ref(spec, idea, request->source, &refs[idea]);
    }
    if (result == PLAN_OK) {
        result = grow_add_motive(build, GROW_IDEA_REPETITION, &refs[GROW_IDEA_REPETITION], &motive);
    }

    /* The harmony and bass to write for each idea. */
    PlanHarmonyShape basic_shape;
    PlanHarmonyShape repetition_shape;
    memset(&basic_shape, 0, sizeof(basic_shape));
    memset(&repetition_shape, 0, sizeof(repetition_shape));
    basic_shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
    basic_shape.starts_on = PLAN_HARMONY_ANCHOR_TONIC;
    basic_shape.ends_on = spec->statement_response ? PLAN_HARMONY_ANCHOR_DOMINANT : PLAN_HARMONY_ANCHOR_TONIC;
    repetition_shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
    repetition_shape.starts_on = spec->statement_response ? PLAN_HARMONY_ANCHOR_DOMINANT : PLAN_HARMONY_ANCHOR_TONIC;
    repetition_shape.ends_on = PLAN_HARMONY_ANCHOR_TONIC;
    if (result == PLAN_OK) {
        result = grow_add_chord_targets(build, ideas[0], &basic_shape, NULL);
    }
    if (result == PLAN_OK) {
        result = grow_add_chord_targets(build, ideas[1], &repetition_shape, NULL);
    }

    /* The melody to write for each idea. */
    if (result == PLAN_OK) {
        result = grow_add_melody_target(build, ideas[0], &refs[GROW_IDEA_BASIC], NULL);
    }
    if (result == PLAN_OK) {
        refs[GROW_IDEA_REPETITION].motive = motive;
        result = grow_add_melody_target(build, ideas[1], &refs[GROW_IDEA_REPETITION], NULL);
    }
    return result;
}

/* A basic idea and a contrasting idea in a container, with their motives and melody; the harmony is the caller's. */
static PlanResult build_ideas(GrowBuild *build, NodeId container, const PlanConstraint *cadence, NodeId *ideas) {
    const GrowThemeSpec *spec = build->spec;
    PlanMotiveRef refs[2];
    NodeId motive;

    PlanResult result =
        grow_add_node(build, container, PLAN_NODE_IDEA, BASIC_START, BASIC_UNITS, "basic idea", &ideas[0]);
    if (result == PLAN_OK) {
        result = grow_add_node(build, container, PLAN_NODE_IDEA, SECOND_START, SECOND_UNITS, "contrasting idea",
                               &ideas[1]);
    }
    for (GrowIdea idea = GROW_IDEA_BASIC; idea <= GROW_IDEA_CONTRAST; idea++) {
        grow_idea_ref(spec, idea, build->request->source, &refs[idea]);
    }
    if (result == PLAN_OK) {
        result = grow_add_motive(build, GROW_IDEA_CONTRAST, &refs[GROW_IDEA_CONTRAST], &motive);
    }
    if (result == PLAN_OK) {
        result = grow_add_melody_target(build, ideas[0], &refs[GROW_IDEA_BASIC], NULL);
    }
    if (result == PLAN_OK) {
        refs[GROW_IDEA_CONTRAST].motive = motive;
        result = grow_add_melody_target(build, ideas[1], &refs[GROW_IDEA_CONTRAST], cadence);
    }
    return result;
}

/* An antecedent: its own phrase, ideas over one phrase model ending in a weak cadence. */
static PlanResult build_antecedent(GrowBuild *build) {
    const GrowThemeSpec *spec = build->spec;
    PlanConstraint weak;
    NodeId antecedent;
    NodeId ideas[2];
    PlanHarmonyShape shape;

    memset(&weak, 0, sizeof(weak));
    weak.kind = PLAN_CONSTRAINT_CADENCE;
    weak.params.cadence = spec->first_cadence;
    PlanResult result = grow_add_node(build, build->request->section, PLAN_NODE_PHRASE, 0, BEGINNING_UNITS,
                                      "antecedent", &antecedent);
    if (result == PLAN_OK) {
        result = plan_node_add_constraint(build->plan, antecedent, &weak);
    }
    memset(&shape, 0, sizeof(shape));
    shape.kind = spec->beginning_shape;
    shape.split_at_bars = spec->beginning_split;
    if (result == PLAN_OK) {
        result = grow_add_chord_targets(build, antecedent, &shape, &weak);
    }
    return result == PLAN_OK ? build_ideas(build, antecedent, &weak, ideas) : result;
}

PlanResult grow_beginning_build(GrowBuild *build) {
    switch (build->spec->pairing.beginning) {
    case PLAN_THEME_BEGINNING_PRESENTATION:
        return build_presentation(build);
    case PLAN_THEME_BEGINNING_ANTECEDENT:
        return build_antecedent(build);
    case PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA:
        break;
    }
    return PLAN_ERR_BAD_PAYLOAD;
}
