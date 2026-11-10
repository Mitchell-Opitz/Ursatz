#include <ursatz_composer/piece.h>

#include <string.h>

#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan_keys.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "piece/piece_internal.h"
#include "util/plan_util.h"

/* Each section's own motive, the four motives grow-theme makes for its ideas and the ones it makes beside them (GROW_MADE_NODES). */
#define MATERIAL_BARS_PER_SECTION (5 + GROW_MADE_NODES)
/* The placeholder note an open melody span holds until motive-invent writes it. */
#define INVENT_PLACEHOLDER 60
/* The weak cadence an antecedent ends in. */
#define ANTECEDENT_CADENCE CADENCE_TYPE_HALF

const char *piece_result_name(PieceResult result) {
    switch (result) {
    case PIECE_OK:
        return "ok";
    case PIECE_ERR_NULL:
        return "null argument";
    case PIECE_ERR_ALLOC:
        return "allocation failed";
    case PIECE_ERR_MOTIVE_TOO_LONG:
        return "motive too long";
    case PIECE_ERR_BAD_REQUEST:
        return "bad request";
    case PIECE_ERR_STUCK:
        return "stuck";
    case PIECE_ERR_BAD_SIZE:
        return "bad size";
    }
    return "unknown";
}

size_t piece_material_bars(size_t section_count) {
    return piece_material_bars_of(section_count, 1);
}

size_t piece_material_bars_of(size_t section_count, size_t motive_bars) {
    return motive_bars + MATERIAL_BARS_PER_SECTION * motive_bars * section_count;
}

static Rational bars_of(Rational bar, size_t count) {
    Rational out = {0, 1};
    if (rational_mul_int(bar, (long long)count, &out) != RATIONAL_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

const FormSpecSize *form_spec_default_size(const FormSpec *form) {
    for (size_t i = 0; form && i < form->size_count; i++) {
        if (form->sizes[i].is_default) {
            return &form->sizes[i];
        }
    }
    return NULL;
}

const FormSpecSize *form_spec_size(const FormSpec *form, size_t bars, size_t count) {
    if (!form || form->request_sections) {
        return NULL;
    }
    if (bars == 0 && count == 0) {
        return form_spec_default_size(form);
    }
    for (size_t i = 0; i < form->size_count; i++) {
        const FormSpecSize *size = &form->sizes[i];
        /* A count picks the size with it, and bars then must match; bars alone pick the first size with them. */
        if (count != 0 ? size->count == count && (bars == 0 || size->bars == bars) : size->bars == bars) {
            return size;
        }
    }
    return NULL;
}

/* The accompaniment the sections ask for: the request's, or for a form that requires one, any pattern when it has none. */
static bool piece_accompaniment(const FormSpec *form, const PieceRequest *request, PlanAccompaniment *out) {
    if (request->has_accompaniment) {
        *out = request->accompaniment;
        return true;
    }
    if (form->accompaniment_required) {
        memset(out, 0, sizeof(*out));
        out->pattern = PLAN_ACCOMPANIMENT_ANY;
        out->texture = -1;
        return true;
    }
    return false;
}

/* One section as grow-theme builds it, with its letter, its return and its layers; its motive comes after m0. */
static PieceResult add_section(PiecePlan *plan, const PieceRequest *request, const FormSectionSpec *section,
                               PlanSpan span, NodeId form, const NodeId *sections,
                               const PlanAccompaniment *accompaniment, NodeId *out) {
    PlanConstraint constraint;
    PieceResult result = piece_result_of_plan(plan_add_node(plan, form, PLAN_NODE_SECTION, span, section->name, out));
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_ARCHETYPE;
        constraint.params.archetype = (PlanArchetype)grow_pairing_archetype(&section->pairing);
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_THEME_PAIRING;
        constraint.params.theme_pairing = section->pairing;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CADENCE;
        constraint.params.cadence = section->cadence;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CADENCE;
        constraint.params.cadence = ANTECEDENT_CADENCE;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_KEY;
        constraint.params.key = request->key;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && accompaniment) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_ACCOMPANIMENT;
        constraint.params.accompaniment = *accompaniment;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_SECTION_LETTER;
        memcpy(constraint.params.letter, section->letter, sizeof(constraint.params.letter));
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->returns != FORM_SPEC_NO_RETURN) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_RETURN;
        constraint.params.section_return.section = sections[section->returns];
        constraint.params.section_return.variation = section->variation;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->layers != 0) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_LAYERS;
        constraint.params.layers.lanes = section->layers;
        result = piece_add_constraint(plan, *out, &constraint);
    }
    return result;
}

/* The motive a section is made with: its own, or a returning section's source's (the source's, in turn). */
static bool section_motive(const FormSpecSize *size, size_t index, MotiveRelationStep *step) {
    const FormSectionSpec *section = &size->sections[index];
    if (section->has_motive) {
        *step = section->motive;
        return true;
    }
    return section->returns != FORM_SPEC_NO_RETURN && section_motive(size, section->returns, step);
}

/* A section whose motive is not m0 itself asks for it as a MOTIVE_REF to m0. */
static PieceResult add_section_motive(PiecePlan *plan, const FormSpecSize *size, size_t index, NodeId id, NodeId source) {
    PlanConstraint constraint;
    MotiveRelationStep step;
    if (!section_motive(size, index, &step)) {
        return PIECE_OK;
    }
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    constraint.params.motive.motive = source;
    constraint.params.motive.has_step = true;
    constraint.params.motive.step = step;
    return piece_add_constraint(plan, id, &constraint);
}

/* m0 with no motive given: a Constrained one-bar motive node whose melody span motive-invent writes. */
static PieceResult add_open_motive(PiecePlan *plan, NodeId idea, PlanSpan span) {
    NodeId node;
    LaneSpanId melody;
    PlanLanePayload payload;
    PlanNote note;
    PlanConstraint constraint;
    PieceResult result = piece_result_of_plan(plan_add_node(plan, idea, PLAN_NODE_MOTIVE, span, GROW_SOURCE_NAME, &node));
    memset(&payload, 0, sizeof(payload));
    note.midi = INVENT_PLACEHOLDER;
    note.spelled = false;
    note.offset = (Rational){0, 1};
    note.duration = span.length;
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_add_lane_span(plan, node, PLAN_LANE_MELODY, 0, span, &payload, &melody));
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_INVENT_MOTIVE;
        result = piece_result_of_plan(plan_lane_span_add_constraint(plan, melody, &constraint));
    }
    return result;
}

/* The material section, its phrase and its idea, Realized, with m0 in it; the source node in *out. */
PieceResult piece_add_material(PiecePlan *plan, const Motive *motive, const PlanSource *motive_source, NodeId form,
                               PlanSpan material, Rational bar, NodeId *out) {
    NodeId section;
    NodeId phrase;
    NodeId idea;
    PlanConstraint constraint;
    PieceResult result = piece_result_of_plan(plan_add_node(plan, form, PLAN_NODE_SECTION, material, GROW_MATERIAL_NAME, &section));
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_MATERIAL;
        result = piece_add_constraint(plan, section, &constraint);
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_add_node(plan, section, PLAN_NODE_PHRASE, material, GROW_MATERIAL_NAME, &phrase));
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_add_node(plan, phrase, PLAN_NODE_IDEA, material, GROW_MATERIAL_NAME, &idea));
    }
    if (result == PIECE_OK && motive) {
        result = piece_result_of_plan(motive_attach_sourced(plan, idea, material.start, motive, GROW_SOURCE_NAME, motive_source,
                                                 out, NULL));
    } else if (result == PIECE_OK) {
        PlanSpan span = {material.start, bar};
        result = add_open_motive(plan, idea, span);
        *out = result == PIECE_OK ? grow_find_source(plan) : PLAN_NODE_NONE;
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_node_set_state(plan, section, PLAN_STATE_REALIZED));
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_node_set_state(plan, phrase, PLAN_STATE_REALIZED));
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_node_set_state(plan, idea, PLAN_STATE_REALIZED));
    }
    return result;
}

/*
 * Every section of the size must be one grow-theme can build in the meter: a theme-like role of whole bars (until P47
 * a sized form's section is a 4 or 8-bar theme).
 */
static bool size_buildable(const FormSpecSize *size, const PlanMeter *meter) {
    for (size_t i = 0; i < size->section_count; i++) {
        const FormSectionSpec *section = &size->sections[i];
        if (section->beats != 0 || !form_role_is_theme(section->role) ||
            !grow_theme_buildable(meter, section->bars, &section->pairing, section->cadence, ANTECEDENT_CADENCE)) {
            return false;
        }
    }
    return true;
}

bool piece_basic_idea(const PieceRequest *request, size_t *theme_bars, size_t *idea_bars) {
    FormSpec *form = NULL;
    if (!request || form_spec_builtin(request->form, &form) != FORM_SPEC_OK) {
        return false;
    }
    const FormSpecSize *size = form_spec_size(form, request->bars, request->count);
    size_t shortest = 8;
    for (size_t i = 0; size && i < size->section_count; i++) {
        shortest = size->sections[i].bars == 8 ? shortest : 4;
    }
    bool found = size != NULL;
    form_spec_destroy(form);
    if (!found) {
        return false;
    }
    *theme_bars = shortest;
    *idea_bars = shortest == 8 ? 2 : 1;
    return true;
}

PieceResult form_build_plan(const FormSpec *form, const PieceRequest *request, PiecePlan **out) {
    if (!form || !request || !out) {
        return PIECE_ERR_NULL;
    }
    *out = NULL;
    if (form->request_sections) {
        return PIECE_ERR_BAD_REQUEST;
    }
    const FormSpecSize *size = form_spec_size(form, request->bars, request->count);
    if (!size) {
        return PIECE_ERR_BAD_SIZE;
    }
    PlanAccompaniment accompaniment;
    bool accompanied = piece_accompaniment(form, request, &accompaniment);
    size_t cells_per_bar;
    if (!grow_cells_per_bar(&request->meter, &cells_per_bar) || analysis_key_tonic_pitch_class(&request->key) < 0 ||
        analysis_key_tonic_pitch_class(&request->key) > 11 ||
        (request->key.mode != KEY_STRUCTURE_MODE_MAJOR && request->key.mode != KEY_STRUCTURE_MODE_MINOR) ||
        (request->motive && request->motive->note_count < 2) ||
        (accompanied && !grow_accompaniment_valid(&request->meter, &accompaniment)) ||
        !size_buildable(size, &request->meter)) {
        return PIECE_ERR_BAD_REQUEST;
    }
    Rational bar = grow_bar_length(&request->meter);
    /* The motive fills the basic idea of every section: 2 bars of an 8-bar theme section, 1 of a 4-bar one. */
    size_t basic_bars = 2;
    for (size_t i = 0; i < size->section_count; i++) {
        basic_bars = size->sections[i].bars == 8 ? basic_bars : 1;
    }
    if (request->motive && !grow_motive_fits_idea(request->motive, bar, basic_bars)) {
        return PIECE_ERR_MOTIVE_TOO_LONG;
    }
    size_t motive_bars = request->motive ? grow_motive_layout_bars(request->motive, bar) : 1;

    PiecePlan *plan = plan_create();
    if (!plan) {
        return PIECE_ERR_ALLOC;
    }
    PlanSpan piece = {{0, 1}, bars_of(bar, size->bars)};
    PlanSpan material = {piece.length, bars_of(bar, piece_material_bars_of(size->section_count, motive_bars) + request->given_melodies)};
    PlanSpan extent = {{0, 1}, {0, 1}};
    PieceResult result = piece_result_of_plan(plan_result_of_rational(rational_add(piece.length, material.length, &extent.length)));
    NodeId form_id = PLAN_NODE_NONE;
    NodeId source = PLAN_NODE_NONE;
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    PlanLanePayload payload;
    LaneSpanId lane;

    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, extent, form->plan_name, &form_id));
    }
    /* The meter is Given over the piece and the material; the key lane comes with the sections (piece_plan_keys). */
    if (result == PIECE_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.meter = request->meter;
        result = piece_result_of_plan(plan_add_lane_span(plan, form_id, PLAN_LANE_METER, 0, extent, &payload, &lane));
        result = result == PIECE_OK ? piece_result_of_plan(plan_lane_span_set_state(plan, lane, PLAN_STATE_GIVEN)) : result;
    }
    /* The sections in order, then the material; the sections' motive references name m0, so they come last. */
    Rational start = {0, 1};
    for (size_t i = 0; result == PIECE_OK && i < size->section_count; i++) {
        PlanSpan span = {start, bars_of(bar, size->sections[i].bars)};
        result = add_section(plan, request, &size->sections[i], span, form_id, sections,
                             accompanied ? &accompaniment : NULL, &sections[i]);
        if (result == PIECE_OK) {
            result = piece_result_of_plan(plan_result_of_rational(rational_add(start, span.length, &start)));
        }
    }
    if (result == PIECE_OK) {
        result = piece_add_material(plan, request->motive, request->source, form_id, material, bar, &source);
    }
    for (size_t i = 0; result == PIECE_OK && i < size->section_count; i++) {
        result = add_section_motive(plan, size, i, sections[i], source);
    }
    if (result == PIECE_OK && request->has_style) {
        PlanConstraint style;
        memset(&style, 0, sizeof(style));
        style.kind = PLAN_CONSTRAINT_STYLE;
        style.params.style = request->style;
        result = piece_result_of_plan(plan_node_add_constraint(plan, form_id, &style));
    }
    /* The keys and the cadences across them are planned from the request's seed, home only unless it asks to modulate;
   then the form is Given. */
    if (result == PIECE_OK) {
        PieceKeyPlan keys;
        FillRng rng;
        fill_rng_seed(&rng, request->seed);
        result = piece_plan_keys(plan, form, size, request->modulate ? &rng : NULL, &keys);
        result = result == PIECE_OK && request->modulate ? piece_plan_cadences(plan, form, size, &keys) : result;
    }
    if (result == PIECE_OK) {
        result = piece_result_of_plan(plan_node_set_state(plan, form_id, PLAN_STATE_GIVEN));
    }
    if (result != PIECE_OK) {
        plan_destroy(plan);
        return result;
    }
    *out = plan;
    return PIECE_OK;
}

PieceResult piece_build_plan(const PieceRequest *request, PiecePlan **out) {
    if (!request || !out) {
        return PIECE_ERR_NULL;
    }
    *out = NULL;
    FormSpec *form = NULL;
    FormSpecResult found = request->form ? form_spec_builtin(request->form, &form) : FORM_SPEC_ERR_UNKNOWN;
    if (found != FORM_SPEC_OK) {
        return found == FORM_SPEC_ERR_ALLOC ? PIECE_ERR_ALLOC : PIECE_ERR_BAD_REQUEST;
    }
    PieceResult result = form_build_plan(form, request, out);
    form_spec_destroy(form);
    return result;
}
