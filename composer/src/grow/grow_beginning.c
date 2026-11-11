#include <string.h>

#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"
#include "util/plan_util.h"

/* The shape of the chords of the first half: a phrase model if it has room for its cadence, else the cadential one. */
static PlanHarmonyShapeKind shape_for(const GrowThemeRequest *request, const GrowThemeSpec *spec, CadenceType cadence) {
    size_t cells = spec->layout.beginning_bars * request->cells_per_bar;
    return cells >= harmony_phrase_room_cells(cadence, request->cells_per_bar) ? PLAN_HARMONY_SHAPE_PHRASE_MODEL
                                                                               : PLAN_HARMONY_SHAPE_CADENTIAL;
}

/*
 * How often a presentation repeats its basic idea in each way (ch. V, XI), by GrowDevelopment and GrowRepetition: exact,
 * dominant form, sequence. The balanced row is the default. A 0 removes the way; when every way a request allows is 0, the
 * balanced row is used for that draw.
 */
static const unsigned REPEAT_WEIGHTS[GROW_DEVELOP_COUNT][GROW_REPEAT_COUNT] = {
    [GROW_DEVELOP_MELODY] = {3, 1, 0},
    [GROW_DEVELOP_BALANCED] = {2, 1, 1},
    [GROW_DEVELOP_THEME] = {0, 1, 2},
};

/* The dominant form and the sequence go to another chord and back or on, which needs a cell for each. */
static GrowRepetition draw_repetition(FillRng *rng, GrowDevelopment development, bool can_move, bool can_sequence) {
    unsigned weight_of[GROW_REPEAT_COUNT];
    uint64_t total = 0;
    for (int row = development; total == 0; row = GROW_DEVELOP_BALANCED) {
        for (int kind = 0; kind < GROW_REPEAT_COUNT; kind++) {
            bool allowed = kind == GROW_REPEAT_EXACT || (can_move && (kind != GROW_REPEAT_SEQUENCE || can_sequence));
            weight_of[kind] = allowed ? REPEAT_WEIGHTS[row][kind] : 0;
            total += weight_of[kind];
        }
    }
    uint64_t pick = fill_rng_below(rng, total);
    for (int kind = 0; kind < GROW_REPEAT_COUNT; kind++) {
        uint64_t weight = weight_of[kind];
        if (pick < weight) {
            return (GrowRepetition)kind;
        }
        pick -= weight;
    }
    return GROW_REPEAT_EXACT;
}

/* The contrasting idea is made from m0 and has to fit its room. */
static bool draw_contrasting_idea(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec) {
    /* A contrasting slot with a motive of its own gives the idea as it is (grow_idea_dev). */
    bool drawn = grow_idea_own(spec, GROW_PLACE_CONTRAST)
                     ? (spec->contrast.kind = MOTIVE_RELATION_EXACT, true)
                     : grow_shorten_source(spec, GROW_PLACE_CONTRAST) &&
                           grow_draw_contrast(request, rng, spec, GROW_PLACE_CONTRAST,
                                              grow_idea_source(spec, GROW_PLACE_CONTRAST), -1, NULL, &spec->contrast);
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
        /* A sequence writes its chords whole, which Given material under the presentation would not fit. */
        spec->repetition = draw_repetition(rng, request->development, spec->basic_cells >= 2,
                                           !request->given_first_half && !request->pedal);
        if (!grow_idea_develop(spec, GROW_PLACE_REPETITION, &spec->developed[GROW_PLACE_REPETITION]) ||
            rational_cmp(spec->developed[GROW_PLACE_REPETITION].length,
                              spec->layout.length[GROW_PLACE_REPETITION]) > 0) {
            return false;
        }
        spec->has_developed[GROW_PLACE_REPETITION] = true;
        return true;
    }
    case PLAN_THEME_BEGINNING_ANTECEDENT:
        spec->beginning_shape = shape_for(request, spec, spec->first_cadence);
        spec->beginning_split = grow_split_possible(request, spec->layout.beginning_bars, spec->beginning_shape, spec->first_cadence);
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
    Rational whole;
    PlanResult result = plan_result_of_rational(rational_add(build->layout.beginning, build->layout.ending, &whole));
    if (result == PLAN_OK) {
        result = grow_add_node(build, build->request->section, PLAN_NODE_PHRASE, rational_ratio(0, 1), whole, "phrase",
                               &build->phrase);
    }
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
    PlanResult result = grow_add_node(build, container, PLAN_NODE_IDEA, rational_ratio(0, 1), build->layout.beginning, "basic idea", idea);
    return result == PLAN_OK ? grow_add_melody_target(build, *idea, &ref, cadence) : result;
}

/* A presentation: a basic idea and its repetition, each over a prolongation. */
static PlanResult build_presentation(GrowBuild *build) {
    const GrowThemeSpec *spec = build->spec;
    NodeId presentation;
    NodeId ideas[2];
    NodeId motive;
    PlanMotiveRef refs[2];

    /* A motif as long as the basic idea with chords of its own, written as it is (R-466). */
    bool whole = spec->basic_as_written && !spec->part_whole && !spec->has_section_motive && spec->basic_has_chords &&
                 !build->request->given_first_half && !build->request->pedal &&
                 grow_motive_fit(&spec->source, build->request->bar, build->layout.basic_bars) == GROW_FIT_WHOLE;
    PlanResult result = make_phrase(build);
    if (result == PLAN_OK) {
        result = grow_add_node(build, build->phrase, PLAN_NODE_SUBPHRASE, rational_ratio(0, 1), build->layout.beginning,
                               "presentation", &presentation);
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
        result = grow_add_node(build, presentation, PLAN_NODE_IDEA, build->layout.start[GROW_PLACE_BASIC],
                               build->layout.length[GROW_PLACE_BASIC], "basic idea", &ideas[0]);
    }
    if (result == PLAN_OK) {
        result = grow_add_node(build, presentation, PLAN_NODE_IDEA, build->layout.start[GROW_PLACE_REPETITION],
                               build->layout.length[GROW_PLACE_REPETITION], "repetition", &ideas[1]);
    }
    /* A repeat of the repetition follows it (R-468). */
    NodeId repeats[PLAN_CONSTRUCTION_EXTENSIONS];
    for (size_t k = 0; k < PLAN_CONSTRUCTION_EXTENSIONS; k++) {
        repeats[k] = PLAN_NODE_NONE;
        if (result == PLAN_OK && k < build->layout.extension_count && build->layout.extensions[k].kind == PLAN_EXTENSION_REPEAT &&
            build->layout.extensions[k].place == GROW_PLACE_REPETITION) {
            result = grow_add_repeat_node(build, presentation, k, &repeats[k]);
        }
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
    if (whole) {
        basic_shape.ends_on = PLAN_HARMONY_ANCHOR_ANY; /* the motif's last chord stands */
    }
    repetition_shape.kind = PLAN_HARMONY_SHAPE_PROLONGATION;
    repetition_shape.starts_on = answers ? PLAN_HARMONY_ANCHOR_DOMINANT : PLAN_HARMONY_ANCHOR_TONIC;
    repetition_shape.ends_on = PLAN_HARMONY_ANCHOR_TONIC;
    if (build->request->pedal) {
        /* Over a dominant pedal both ideas prolong V; a sequence is never drawn then (R-446). */
        basic_shape.starts_on = PLAN_HARMONY_ANCHOR_DOMINANT;
        basic_shape.ends_on = PLAN_HARMONY_ANCHOR_DOMINANT;
        basic_shape.pedal = true;
        repetition_shape = basic_shape;
    }
    bool no_chords = whole ? !spec->basic_has_chords : spec->source.chord_count == 0;
    if (spec->repetition == GROW_REPEAT_SEQUENCE && no_chords) {
        /* A motif with no harmony of its own: the chord moves with the melody, the basic idea's tonic and the repetition's
           chord a step up (I to ii, i to iio), written whole (R-348). */
        if (result == PLAN_OK) {
            result = grow_add_chord_written(build, ideas[0], 1);
        }
        if (result == PLAN_OK) {
            result = grow_add_chord_written(build, ideas[1], 2);
        }
    } else {
        if (spec->repetition == GROW_REPEAT_SEQUENCE) {
            /* The motif's harmony moves with the melody: the chord comes from the repetition's motive, so the repetition has
               no anchor of its own for the harmony to hold. */
            repetition_shape.starts_on = PLAN_HARMONY_ANCHOR_ANY;
            repetition_shape.ends_on = PLAN_HARMONY_ANCHOR_ANY;
        }
        if (result == PLAN_OK) {
            result = grow_add_chord_targets(build, ideas[0], &basic_shape, NULL);
        }
        if (result == PLAN_OK) {
            result = grow_add_chord_targets(build, ideas[1], &repetition_shape, NULL);
        }
    }
    /* A repeat has the repetition's shape of chords. */
    for (size_t k = 0; result == PLAN_OK && k < PLAN_CONSTRUCTION_EXTENSIONS; k++) {
        if (repeats[k] != PLAN_NODE_NONE) {
            result = grow_add_chord_targets(build, repeats[k], &repetition_shape, NULL);
        }
    }

    /* The melody to write for each idea. */
    if (result == PLAN_OK) {
        result = whole ? grow_add_melody_written(build, ideas[0], &refs[GROW_PLACE_BASIC], &spec->source)
                       : grow_add_melody_target(build, ideas[0], &refs[GROW_PLACE_BASIC], NULL);
    }
    if (result == PLAN_OK) {
        refs[GROW_PLACE_REPETITION].motive = motive;
        result = grow_add_melody_target(build, ideas[1], &refs[GROW_PLACE_REPETITION], NULL);
    }
    for (size_t k = 0; result == PLAN_OK && k < PLAN_CONSTRUCTION_EXTENSIONS; k++) {
        if (repeats[k] != PLAN_NODE_NONE) {
            PlanMotiveRef ref;
            grow_extension_ref(build, k, &ref);
            result = grow_add_melody_target(build, repeats[k], &ref, NULL);
        }
    }
    return result;
}

/*
 * A basic idea and a contrasting idea in a container, with their motives and melody; the harmony is the caller's. A repeat
 * of the contrasting idea (R-468) follows it and ends the container, so it asks for the cadence the contrasting idea would.
 */
static PlanResult build_ideas(GrowBuild *build, NodeId container, const PlanConstraint *cadence, NodeId *ideas) {
    const GrowThemeSpec *spec = build->spec;
    PlanMotiveRef refs[2];
    NodeId motive;
    NodeId repeats[PLAN_CONSTRUCTION_EXTENSIONS];
    bool repeated = false;

    PlanResult result = grow_add_node(build, container, PLAN_NODE_IDEA, build->layout.start[GROW_PLACE_BASIC],
                                      build->layout.length[GROW_PLACE_BASIC], "basic idea", &ideas[0]);
    if (result == PLAN_OK) {
        result = grow_add_node(build, container, PLAN_NODE_IDEA, build->layout.start[GROW_PLACE_CONTRAST],
                               build->layout.length[GROW_PLACE_CONTRAST], "contrasting idea", &ideas[1]);
    }
    for (size_t k = 0; k < PLAN_CONSTRUCTION_EXTENSIONS; k++) {
        repeats[k] = PLAN_NODE_NONE;
        if (result == PLAN_OK && k < build->layout.extension_count && build->layout.extensions[k].kind == PLAN_EXTENSION_REPEAT &&
            build->layout.extensions[k].place == GROW_PLACE_CONTRAST) {
            result = grow_add_repeat_node(build, container, k, &repeats[k]);
            repeated = true;
        }
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
        result = grow_add_melody_target(build, ideas[1], &refs[GROW_PLACE_CONTRAST], repeated ? NULL : cadence);
    }
    for (size_t k = 0; result == PLAN_OK && k < PLAN_CONSTRUCTION_EXTENSIONS; k++) {
        if (repeats[k] != PLAN_NODE_NONE) {
            PlanMotiveRef ref;
            grow_extension_ref(build, k, &ref);
            result = grow_add_melody_target(build, repeats[k], &ref, cadence);
        }
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
    PlanResult result = grow_add_node(build, build->request->section, PLAN_NODE_PHRASE, rational_ratio(0, 1),
                                      build->layout.beginning, "antecedent", &antecedent);
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
        result = grow_add_node(build, build->phrase, PLAN_NODE_SUBPHRASE, rational_ratio(0, 1), build->layout.beginning,
                               "compound basic idea", &compound);
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
