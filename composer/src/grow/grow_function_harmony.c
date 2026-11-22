#include <ursatz_composer/grow_types.h>

#include <string.h>

#include <plan/plan_state.h>

#include "grow/grow_internal.h"
#include "util/position.h"

/*
 * The harmony of a theme's halves by the formal functions of their ideas (R-711, functions.harmony): each idea of an antecedent and
 * of a continuation or a consequent takes the progression type its function asks for, instead of one phrase model or one
 * progression over the whole half.
 *
 *   an idea that ends the half's cadence (a cadential idea, a contrasting idea)  a `cadential` target with that cadence;
 *   a fragment (`motive_harmony` free, harmonic rhythm faster)                   a prolongation of a chord a cell, free of the
 *                                                                                motive's chords (`free_motive`);
 *   a basic idea (consequent basic idea)                                         a prolongation from the tonic, the motive's
 *                                                                                chords held.
 *
 * The progression type is the first of the function's `harmony` entries that has a shape here; a cadential one is only for an idea
 * that ends a cadence, and `sequence` has no shape and is passed over. The cadential idea needs as many cells before its last bar as
 * the cadential progression has chords before it: when it has fewer, its target starts that many cells earlier, in the idea before,
 * whose own target stops where it starts.
 */

/* The progression types that have a harmony shape, by the code of their vocabulary entry. */
static const struct {
    const char *code;
    PlanHarmonyShapeKind kind;
} SHAPES[] = {{"prolongation", PLAN_HARMONY_SHAPE_PROLONGATION},
              {"cadential", PLAN_HARMONY_SHAPE_CADENTIAL},
              {"phrase_model", PLAN_HARMONY_SHAPE_PHRASE_MODEL}};

bool grow_progression_shape(const char *code, PlanHarmonyShapeKind *kind) {
    for (size_t s = 0; code && s < sizeof(SHAPES) / sizeof(SHAPES[0]); s++) {
        if (strcmp(code, SHAPES[s].code) == 0) {
            *kind = SHAPES[s].kind;
            return true;
        }
    }
    return false;
}

/* The shape of the first progression type of the function that has one here; false when none does. */
static bool shape_of(const FormalFunction *function, bool ends_cadence, PlanHarmonyShapeKind *kind) {
    for (size_t i = 0; i < function->harmony_count; i++) {
        PlanHarmonyShapeKind found;
        if (grow_progression_shape(function->harmony[i]->code, &found) && (found != PLAN_HARMONY_SHAPE_CADENTIAL || ends_cadence)) {
            *kind = found;
            return true;
        }
    }
    return false;
}

/* The theme type of the pairing, or NULL. */
static const ThemeType *type_of(const GrowThemeRequest *request, const SectionPairing *pairing) {
    const ThemeShapeHalf *beginning = theme_shape_beginning(pairing->beginning);
    const ThemeShapeHalf *ending = theme_shape_ending(pairing->ending);
    return request->knowledge && beginning && ending
               ? knowledge_snapshot_theme_type_of(request->knowledge, beginning->library_name, ending->library_name)
               : NULL;
}

/* The idea of the theme type at a place, by the name of the theme shape row. */
static const ThemeTypeIdea *idea_at(const ThemeType *type, const SectionPairing *pairing, GrowPlace place) {
    const ThemeShapeIdea *row = theme_shape_place_in(pairing, (unsigned)place);
    for (size_t i = 0; row && type && i < type->idea_count; i++) {
        if (strcmp(type->ideas[i].name, row->name) == 0) {
            return &type->ideas[i];
        }
    }
    return NULL;
}

/* The places of the half's ideas in time order. */
static size_t half_places(const SectionPairing *pairing, bool beginning, GrowPlace places[3]) {
    if (beginning) {
        places[0] = GROW_PLACE_BASIC;
        places[1] = GROW_PLACE_CONTRAST;
        return pairing->beginning == SECTION_BEGINNING_ANTECEDENT ? 2 : 0;
    }
    if (pairing->ending == SECTION_ENDING_CONTINUATION) {
        places[0] = GROW_PLACE_FRAGMENT_1;
        places[1] = GROW_PLACE_FRAGMENT_2;
        places[2] = GROW_PLACE_CADENTIAL;
        return 3;
    }
    if (pairing->ending == SECTION_ENDING_CONSEQUENT) {
        places[0] = GROW_PLACE_RESTATED;
        places[1] = GROW_PLACE_CONSEQUENT_CONTRAST;
        return 2;
    }
    return 0;
}

void grow_function_harmony_decide(const GrowThemeRequest *request, GrowThemeSpec *spec) {
    spec->function_type = NULL;
    spec->function_harmony[0] = false;
    spec->function_harmony[1] = false;
    if (!request->functions.harmony || spec->has_construction || request->pedal) {
        return;
    }
    const ThemeType *type = type_of(request, &spec->pairing);
    for (int half = 0; type && half < 2; half++) {
        bool beginning = half == 0;
        GrowPlace places[3];
        size_t count = half_places(&spec->pairing, beginning, places);
        bool free_of_changes = beginning ? !request->given_first_half && !request->has_start_change && !spec->part_whole
                                         : !request->given_second_half && !request->has_end_change;
        bool shaped = count > 0 && free_of_changes;
        for (size_t i = 0; shaped && i < count; i++) {
            const ThemeTypeIdea *idea = idea_at(type, &spec->pairing, places[i]);
            PlanHarmonyShapeKind kind;
            shaped = idea != NULL && shape_of(idea->function, idea->cadence != THEME_IDEA_CADENCE_NONE, &kind);
        }
        if (shaped) {
            spec->function_type = type;
            spec->function_harmony[half] = true;
        }
    }
}

/* The span from `start` of `cells` cells, in the plan. */
static PlanResult cells_span(const GrowBuild *build, Rational start, size_t cells, PlanSpan *out) {
    Rational cell;
    PlanResult result = plan_result_of_rational(rational_div_int(build->request->bar, (long long)build->request->cells_per_bar, &cell));
    out->start = start;
    out->length = position_times(cell, cells);
    return result;
}

PlanResult grow_add_function_harmony(GrowBuild *build, NodeId container, const NodeId *nodes, bool beginning) {
    const GrowThemeRequest *request = build->request;
    const GrowThemeSpec *spec = build->spec;
    GrowPlace places[3];
    size_t count = half_places(&spec->pairing, beginning, places);
    const ThemeType *type = spec->function_type;
    Rational cell;
    PlanResult result = plan_result_of_rational(rational_div_int(request->bar, (long long)request->cells_per_bar, &cell));
    /* The cells each idea holds, from the half's start: the cadential idea may take cells from the ideas before it. */
    size_t first_cell[3];
    size_t own_first[3];
    size_t end_cell[3];
    size_t at = 0;
    for (size_t i = 0; result == PLAN_OK && i < count; i++) {
        Rational cells;
        result = plan_result_of_rational(rational_div(build->layout.length[places[i]], cell, &cells));
        first_cell[i] = at;
        own_first[i] = at;
        at += result == PLAN_OK && cells.den == 1 && cells.num > 0 ? (size_t)cells.num : 1;
        end_cell[i] = at;
    }
    PlanHarmonyShape shapes[3];
    const PlanConstraint *cadences[3] = {NULL, NULL, NULL};
    PlanConstraint first_cadence;
    memset(&first_cadence, 0, sizeof(first_cadence));
    first_cadence.kind = PLAN_CONSTRAINT_CADENCE;
    first_cadence.params.cadence = spec->first_cadence;
    memset(shapes, 0, sizeof(shapes));
    for (size_t i = 0; result == PLAN_OK && i < count; i++) {
        const ThemeTypeIdea *idea = idea_at(type, &spec->pairing, places[i]);
        bool ends_cadence = idea->cadence != THEME_IDEA_CADENCE_NONE;
        PlanHarmonyShapeKind kind = PLAN_HARMONY_SHAPE_PROLONGATION;
        shape_of(idea->function, ends_cadence, &kind);
        PlanHarmonyShape *shape = &shapes[i];
        shape->kind = kind;
        shape->free_motive = !idea->function->keeps_motive_harmony;
        shape->split_at_bars = true;
        if (kind == PLAN_HARMONY_SHAPE_CADENTIAL) {
            CadenceType cadence = idea->cadence == THEME_IDEA_CADENCE_FIRST ? spec->first_cadence : request->cadence;
            size_t chords = grow_chords_before_last(request, PLAN_HARMONY_SHAPE_CADENTIAL, cadence);
            size_t idea_cells = end_cell[i] - first_cell[i];
            size_t before_last = idea_cells > request->cells_per_bar ? idea_cells - request->cells_per_bar : 0;
            size_t cap = (before_last + chords - 1) / chords;
            shape->max_cells_per_chord = (int)(cap > 1 ? cap : 1);
            cadences[i] = idea->cadence == THEME_IDEA_CADENCE_FIRST ? &first_cadence : &build->cadence;
            /* Too few cells before the last bar for the chords there: the target starts that many cells earlier. */
            if (before_last < chords && i > 0) {
                size_t earlier = chords - before_last;
                size_t room = end_cell[i - 1] - first_cell[i - 1];
                earlier = earlier < room ? earlier : room;
                first_cell[i] -= earlier;
                end_cell[i - 1] -= earlier;
            }
        } else if (kind == PLAN_HARMONY_SHAPE_PROLONGATION && !idea->function->keeps_motive_harmony) {
            /* A fragment: a chord a cell, the tonic and then the dominant when it holds two cells or more. */
            shape->max_cells_per_chord = 1;
            shape->starts_on = PLAN_HARMONY_ANCHOR_TONIC;
        } else {
            shape->starts_on = PLAN_HARMONY_ANCHOR_TONIC;
            shape->ends_on = PLAN_HARMONY_ANCHOR_ANY;
        }
    }
    /* The chords, in time order: each over its own cells (the cadential idea's from where it starts). */
    Rational origin = plan_node_span(build->plan, container).start;
    for (size_t i = 0; result == PLAN_OK && i < count; i++) {
        size_t cells = end_cell[i] - first_cell[i];
        PlanSpan span;
        Rational start;
        if (cells == 0) {
            continue;
        }
        if (shapes[i].kind == PLAN_HARMONY_SHAPE_PROLONGATION && shapes[i].max_cells_per_chord == 1) {
            shapes[i].ends_on = cells >= 2 ? PLAN_HARMONY_ANCHOR_DOMINANT : PLAN_HARMONY_ANCHOR_TONIC;
        }
        result = plan_result_of_rational(rational_add(origin, position_times(cell, first_cell[i]), &start));
        result = result == PLAN_OK ? cells_span(build, start, cells, &span) : result;
        /* A span that stays in the idea belongs to its node; one that reaches into the idea before belongs to the phrase. */
        NodeId owner = first_cell[i] < own_first[i] ? container : nodes[i];
        result = result == PLAN_OK ? grow_add_chord_targets_over(build, owner, span, &shapes[i], cadences[i]) : result;
    }
    return result;
}
