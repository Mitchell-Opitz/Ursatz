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
static PlanHarmonyShapeKind shape_for(const GrowThemeRequest *request, CadenceType cadence) {
    size_t cells = request->bars / 2 * request->cells_per_bar;
    return cells >= harmony_phrase_room_cells(cadence, request->cells_per_bar) ? PLAN_HARMONY_SHAPE_PHRASE_MODEL
                                                                               : PLAN_HARMONY_SHAPE_CADENTIAL;
}

/* The contrasting idea is made from m0 and has to fit its room. */
static bool draw_contrasting_idea(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec) {
    /* A contrasting slot with a motive of its own gives the idea as it is (grow_idea_dev). */
    bool drawn = grow_idea_own(spec, GROW_PLACE_CONTRAST)
                     ? (spec->contrast.kind = MOTIVE_RELATION_EXACT, true)
                     : grow_draw_contrast(request, rng, spec, &spec->source, -1, NULL, &spec->contrast);
    return drawn &&
           grow_idea_develop(spec, GROW_PLACE_CONTRAST, &spec->developed[GROW_PLACE_CONTRAST]) &&
           (spec->has_developed[GROW_PLACE_CONTRAST] = true);
}

bool grow_beginning_draw(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec) {
    switch (spec->pairing.beginning) {
    case PLAN_THEME_BEGINNING_PRESENTATION: {
        /* A key opens on a phrase model's pivot chord; a presentation's prolongations have none. */
        if (request->has_start_change) {
            return false;
        }
        /* A part-length motive is the whole presentation: no repetition to draw. */
        if (spec->part_whole) {
            spec->repetition = GROW_REPEAT_EXACT;
            return true;
        }
        /* Statement-response goes from tonic to dominant and back, which needs a cell for each. */
        bool can_respond = spec->basic_cells >= 2;
        uint64_t response = fill_rng_below(rng, 2);
        spec->repetition = can_respond && response == 1 ? GROW_REPEAT_DOMINANT_FORM : GROW_REPEAT_EXACT;
        if (!grow_idea_develop(spec, GROW_PLACE_REPETITION, &spec->developed[GROW_PLACE_REPETITION]) ||
            rational_cmp(spec->developed[GROW_PLACE_REPETITION].length,
                              grow_scaled(grow_unit(request), SECOND_UNITS)) > 0) {
            return false;
        }
        spec->has_developed[GROW_PLACE_REPETITION] = true;
        return true;
    }
    case PLAN_THEME_BEGINNING_ANTECEDENT:
        spec->beginning_shape = shape_for(request, spec->first_cadence);
        spec->beginning_split = grow_split_possible(request, spec->beginning_shape, spec->first_cadence);
        if (request->has_start_change && spec->beginning_shape != PLAN_HARMONY_SHAPE_PHRASE_MODEL) {
            return false;
        }
        return spec->part_whole || draw_contrasting_idea(request, rng, spec);
    case PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA:
        if (request->has_start_change) {
            return false;
        }
        spec->compound_ends_on = fill_rng_below(rng, 2) == 0 ? PLAN_HARMONY_ANCHOR_TONIC : PLAN_HARMONY_ANCHOR_DOMINANT;
        return draw_contrasting_idea(request, rng, spec);
    }
    return false;
}

/* The one phrase the parts of a theme go in, with the cadence it ends in. */
static PlanResult make_phrase(GrowBuild *build) {
    PlanResult result =
        grow_add_node(build, build->request->section, PLAN_NODE_PHRASE, 0, 8, "phrase", &build->phrase);
    return result == PLAN_OK ? plan_node_add_constraint(build->plan, build->phrase, &build->cadence) : result;
}

/*
 * A part filled whole by a motive: one idea as long as the part, its melody the motive itself (m0, not the shorter basic
 * idea made from it), asking for the cadence the part ends in, if it has one.
 */
static PlanResult build_whole_part(GrowBuild *build, NodeId container, const PlanConstraint *cadence, NodeId *idea) {
    PlanMotiveRef ref;
    memset(&ref, 0, sizeof(ref));
    ref.motive = build->request->source;
    PlanResult result = grow_add_node(build, container, PLAN_NODE_IDEA, BASIC_START, BEGINNING_UNITS, "basic idea", idea);
    return result == PLAN_OK ? grow_add_melody_target(build, *idea, &ref, cadence) : result;
}

/* A presentation: a basic idea and its repetition, each over a prolongation. */
static PlanResult build_presentation(GrowBuild *build) {
    const GrowThemeSpec *spec = build->spec;
    NodeId presentation;
    NodeId ideas[2];
    NodeId motive;
    PlanMotiveRef refs[2];

    PlanResult result = make_phrase(build);
    if (result == PLAN_OK) {
        result = grow_add_node(build, build->phrase, PLAN_NODE_SUBPHRASE, 0, BEGINNING_UNITS, "presentation",
                               &presentation);
    }
    if (result == PLAN_OK && spec->part_whole) {
        PlanHarmonyShape whole;
        memset(&whole, 0, sizeof(whole));
        whole.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
        whole.starts_on = PLAN_HARMONY_ANCHOR_TONIC;
        whole.ends_on = PLAN_HARMONY_ANCHOR_TONIC;
        result = build_whole_part(build, presentation, NULL, &ideas[0]);
        return result == PLAN_OK ? grow_add_chord_targets(build, ideas[0], &whole, NULL) : result;
    }
    if (result == PLAN_OK) {
        result = grow_add_node(build, presentation, PLAN_NODE_IDEA, BASIC_START, BASIC_UNITS, "basic idea", &ideas[0]);
    }
    if (result == PLAN_OK) {
        result = grow_add_node(build, presentation, PLAN_NODE_IDEA, SECOND_START, SECOND_UNITS, "repetition",
                               &ideas[1]);
    }
    for (GrowPlace idea = GROW_PLACE_BASIC; idea <= GROW_PLACE_REPETITION; idea++) {
        grow_idea_ref(spec, idea, grow_idea_node(build, idea), &refs[idea]);
    }
    if (result == PLAN_OK) {
        result = grow_add_motive(build, GROW_PLACE_REPETITION, &refs[GROW_PLACE_REPETITION], &motive);
    }

    /* The harmony and bass to write for each idea. */
    PlanHarmonyShape basic_shape;
    PlanHarmonyShape repetition_shape;
    memset(&basic_shape, 0, sizeof(basic_shape));
    memset(&repetition_shape, 0, sizeof(repetition_shape));
    basic_shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
    basic_shape.starts_on = PLAN_HARMONY_ANCHOR_TONIC;
    bool answers = spec->repetition == GROW_REPEAT_DOMINANT_FORM;
    basic_shape.ends_on = answers ? PLAN_HARMONY_ANCHOR_DOMINANT : PLAN_HARMONY_ANCHOR_TONIC;
    repetition_shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
    repetition_shape.starts_on = answers ? PLAN_HARMONY_ANCHOR_DOMINANT : PLAN_HARMONY_ANCHOR_TONIC;
    repetition_shape.ends_on = PLAN_HARMONY_ANCHOR_TONIC;
    if (result == PLAN_OK) {
        result = grow_add_chord_targets(build, ideas[0], &basic_shape, NULL);
    }
    if (result == PLAN_OK) {
        result = grow_add_chord_targets(build, ideas[1], &repetition_shape, NULL);
    }

    /* The melody to write for each idea. */
    if (result == PLAN_OK) {
        result = grow_add_melody_target(build, ideas[0], &refs[GROW_PLACE_BASIC], NULL);
    }
    if (result == PLAN_OK) {
        refs[GROW_PLACE_REPETITION].motive = motive;
        result = grow_add_melody_target(build, ideas[1], &refs[GROW_PLACE_REPETITION], NULL);
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
    for (GrowPlace idea = GROW_PLACE_BASIC; idea <= GROW_PLACE_CONTRAST; idea++) {
        grow_idea_ref(spec, idea, grow_idea_node(build, idea), &refs[idea]);
    }
    if (result == PLAN_OK) {
        result = grow_add_motive(build, GROW_PLACE_CONTRAST, &refs[GROW_PLACE_CONTRAST], &motive);
    }
    if (result == PLAN_OK) {
        result = grow_add_melody_target(build, ideas[0], &refs[GROW_PLACE_BASIC], NULL);
    }
    if (result == PLAN_OK) {
        refs[GROW_PLACE_CONTRAST].motive = motive;
        result = grow_add_melody_target(build, ideas[1], &refs[GROW_PLACE_CONTRAST], cadence);
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
    shape.starts_on = build->request->has_start_change ? PLAN_HARMONY_ANCHOR_PIVOT : PLAN_HARMONY_ANCHOR_ANY;
    shape.split_at_bars = spec->beginning_split;
    if (result == PLAN_OK) {
        result = grow_add_chord_targets(build, antecedent, &shape, &weak);
    }
    if (result == PLAN_OK && spec->part_whole) {
        return build_whole_part(build, antecedent, &weak, &ideas[0]);
    }
    return result == PLAN_OK ? build_ideas(build, antecedent, &weak, ideas) : result;
}

/* A compound basic idea: a basic idea and a contrasting idea over one prolongation, no cadence. */
static PlanResult build_compound_basic_idea(GrowBuild *build) {
    const GrowThemeSpec *spec = build->spec;
    NodeId compound;
    NodeId ideas[2];
    PlanHarmonyShape shape;

    PlanResult result = make_phrase(build);
    if (result == PLAN_OK) {
        result = grow_add_node(build, build->phrase, PLAN_NODE_SUBPHRASE, 0, BEGINNING_UNITS, "compound basic idea",
                               &compound);
    }
    memset(&shape, 0, sizeof(shape));
    shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
    shape.starts_on = PLAN_HARMONY_ANCHOR_TONIC;
    shape.ends_on = spec->compound_ends_on;
    if (result == PLAN_OK) {
        result = grow_add_chord_targets(build, compound, &shape, NULL);
    }
    return result == PLAN_OK ? build_ideas(build, compound, NULL, ideas) : result;
}

PlanResult grow_beginning_build(GrowBuild *build) {
    switch (build->spec->pairing.beginning) {
    case PLAN_THEME_BEGINNING_PRESENTATION:
        return build_presentation(build);
    case PLAN_THEME_BEGINNING_ANTECEDENT:
        return build_antecedent(build);
    case PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA:
        return build_compound_basic_idea(build);
    }
    return PLAN_ERR_BAD_PAYLOAD;
}
