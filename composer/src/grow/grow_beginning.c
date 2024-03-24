#include <string.h>

#include "grow/grow_internal.h"

/* The ideas of a presentation, in eighths of the theme. */
#define BASIC_START 0
#define BASIC_UNITS 2
#define REPETITION_START 2
#define REPETITION_UNITS 2

bool grow_beginning_draw(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec) {
    (void)request;
    /* Statement-response goes from tonic to dominant and back, which needs a cell for each. */
    bool can_respond = spec->basic_cells >= 2;
    uint64_t response = fill_rng_below(rng, 2);
    spec->statement_response = can_respond && response == 1;
    return true;
}

PlanResult grow_beginning_build(GrowBuild *build) {
    const GrowThemeSpec *spec = build->spec;
    const GrowThemeRequest *request = build->request;
    NodeId presentation;
    NodeId ideas[2];
    NodeId motive;
    PlanMotiveRef refs[2];

    /* The one phrase of the theme, with the cadence it ends in. */
    PlanResult result = grow_add_node(build, request->section, PLAN_NODE_PHRASE, 0, 8, "phrase", &build->phrase);
    if (result == PLAN_OK) {
        result = plan_node_add_constraint(build->plan, build->phrase, &build->cadence);
    }
    if (result == PLAN_OK) {
        result = grow_add_node(build, build->phrase, PLAN_NODE_SUBPHRASE, 0, 4, "presentation", &presentation);
    }
    if (result == PLAN_OK) {
        result = grow_add_node(build, presentation, PLAN_NODE_IDEA, BASIC_START, BASIC_UNITS, "basic idea", &ideas[0]);
    }
    if (result == PLAN_OK) {
        result = grow_add_node(build, presentation, PLAN_NODE_IDEA, REPETITION_START, REPETITION_UNITS, "repetition",
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
