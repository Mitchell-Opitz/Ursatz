#include <ursatz_composer/piece.h>

#include <string.h>

#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan_keys.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"

/* Each section's own motive and the four motives grow-theme makes for its ideas. */
#define MATERIAL_BARS_PER_SECTION 5
/* The placeholder note an open melody span holds until motive-invent writes it. */
#define INVENT_PLACEHOLDER 60
/* The weak cadence an antecedent ends in. */
#define ANTECEDENT_CADENCE PLAN_CADENCE_HC

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
    return 1 + MATERIAL_BARS_PER_SECTION * section_count;
}

static PieceResult from_plan(PlanResult result) {
    return result == PLAN_OK ? PIECE_OK : (result == PLAN_ERR_ALLOC ? PIECE_ERR_ALLOC : PIECE_ERR_BAD_REQUEST);
}

static PlanTime bars_of(PlanTime bar, size_t count) {
    PlanTime out = {0, 1};
    if (plan_time_make(bar.num * (long)count, bar.den, &out) != PLAN_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

/* The accompaniment the sections ask for: the request's, or for a layered form, any pattern when it has none. */
static bool piece_accompaniment(const PieceRequest *request, PlanAccompaniment *out) {
    if (request->has_accompaniment) {
        *out = request->accompaniment;
        return true;
    }
    if (request->form == FORM_TEMPLATE_LAYERED) {
        memset(out, 0, sizeof(*out));
        out->pattern = PLAN_ACCOMPANIMENT_ANY;
        out->texture = -1;
        return true;
    }
    return false;
}

static PieceResult add_constraint(PiecePlan *plan, NodeId id, const PlanConstraint *constraint) {
    return from_plan(plan_node_add_constraint(plan, id, constraint));
}

/* One section as grow-theme builds it, with its letter, its return and its layers; its motive comes after m0. */
static PieceResult add_section(PiecePlan *plan, const PieceRequest *request, const FormTemplateSection *section,
                               PlanSpan span, NodeId form, const NodeId *sections,
                               const PlanAccompaniment *accompaniment, NodeId *out) {
    PlanConstraint constraint;
    PieceResult result = from_plan(plan_add_node(plan, form, PLAN_NODE_SECTION, span, section->name, out));
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_ARCHETYPE;
        constraint.params.archetype = (PlanArchetype)grow_pairing_archetype(&section->pairing);
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_THEME_PAIRING;
        constraint.params.theme_pairing = section->pairing;
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CADENCE;
        constraint.params.cadence = section->cadence;
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CADENCE;
        constraint.params.cadence = ANTECEDENT_CADENCE;
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_KEY;
        constraint.params.key = request->key;
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && accompaniment) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_ACCOMPANIMENT;
        constraint.params.accompaniment = *accompaniment;
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_SECTION_LETTER;
        memcpy(constraint.params.letter, section->letter, sizeof(constraint.params.letter));
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->returns != FORM_TEMPLATE_NO_RETURN) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_RETURN;
        constraint.params.section_return.section = sections[section->returns];
        constraint.params.section_return.variation = section->variation;
        result = add_constraint(plan, *out, &constraint);
    }
    if (result == PIECE_OK && section->layers != 0) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_LAYERS;
        constraint.params.layers.lanes = section->layers;
        result = add_constraint(plan, *out, &constraint);
    }
    return result;
}

/* A section whose motive is not m0 itself asks for it as a MOTIVE_REF to m0. */
static PieceResult add_section_motive(PiecePlan *plan, const FormTemplateSection *section, NodeId id, NodeId source) {
    PlanConstraint constraint;
    if (section->motive == PLAN_TRANSFORM_NONE) {
        return PIECE_OK;
    }
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    constraint.params.motive.motive = source;
    constraint.params.motive.transform = section->motive;
    constraint.params.motive.tonal = section->motive_tonal;
    return add_constraint(plan, id, &constraint);
}

/* m0 with no motive given: a Constrained one-bar motive node whose melody span motive-invent writes. */
static PieceResult add_open_motive(PiecePlan *plan, NodeId idea, PlanSpan span) {
    NodeId node;
    LaneSpanId melody;
    PlanLanePayload payload;
    PlanNote note;
    PlanConstraint constraint;
    PieceResult result = from_plan(plan_add_node(plan, idea, PLAN_NODE_MOTIVE, span, GROW_SOURCE_NAME, &node));
    memset(&payload, 0, sizeof(payload));
    note.midi_pitch = INVENT_PLACEHOLDER;
    note.offset = (PlanTime){0, 1};
    note.duration = span.length;
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    if (result == PIECE_OK) {
        result = from_plan(plan_add_lane_span(plan, node, PLAN_LANE_MELODY, 0, span, &payload, &melody));
    }
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_INVENT_MOTIVE;
        result = from_plan(plan_lane_span_add_constraint(plan, melody, &constraint));
    }
    return result;
}

/* The material section, its phrase and its idea, Realized, with m0 in it; the source node in *out. */
static PieceResult add_material(PiecePlan *plan, const PieceRequest *request, NodeId form, PlanSpan material,
                                PlanTime bar, NodeId *out) {
    NodeId section;
    NodeId phrase;
    NodeId idea;
    PlanConstraint constraint;
    PieceResult result = from_plan(plan_add_node(plan, form, PLAN_NODE_SECTION, material, GROW_MATERIAL_NAME, &section));
    if (result == PIECE_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_MATERIAL;
        result = add_constraint(plan, section, &constraint);
    }
    if (result == PIECE_OK) {
        result = from_plan(plan_add_node(plan, section, PLAN_NODE_PHRASE, material, GROW_MATERIAL_NAME, &phrase));
    }
    if (result == PIECE_OK) {
        result = from_plan(plan_add_node(plan, phrase, PLAN_NODE_IDEA, material, GROW_MATERIAL_NAME, &idea));
    }
    if (result == PIECE_OK && request->motive) {
        result = from_plan(motive_attach(plan, idea, material.start, request->motive, GROW_SOURCE_NAME, out, NULL));
    } else if (result == PIECE_OK) {
        PlanSpan span = {material.start, bar};
        result = add_open_motive(plan, idea, span);
        *out = result == PIECE_OK ? grow_find_source(plan) : PLAN_NODE_NONE;
    }
    if (result == PIECE_OK) {
        result = from_plan(plan_node_set_state(plan, section, PLAN_STATE_REALIZED));
    }
    if (result == PIECE_OK) {
        result = from_plan(plan_node_set_state(plan, phrase, PLAN_STATE_REALIZED));
    }
    if (result == PIECE_OK) {
        result = from_plan(plan_node_set_state(plan, idea, PLAN_STATE_REALIZED));
    }
    return result;
}

/* Every section of the template must be one grow-theme can build in the meter. */
static bool template_buildable(const FormTemplate *layout, const PlanMeter *meter) {
    for (size_t i = 0; i < layout->section_count; i++) {
        const FormTemplateSection *section = &layout->sections[i];
        if (!grow_theme_buildable(meter, section->bars, &section->pairing, section->cadence, ANTECEDENT_CADENCE)) {
            return false;
        }
    }
    return true;
}

PieceResult piece_build_plan(const PieceRequest *request, PiecePlan **out) {
    if (!request || !out) {
        return PIECE_ERR_NULL;
    }
    *out = NULL;
    FormTemplate layout;
    if (!form_template_layout(request->form, request->bars, request->count, &layout)) {
        return PIECE_ERR_BAD_SIZE;
    }
    PlanAccompaniment accompaniment;
    bool accompanied = piece_accompaniment(request, &accompaniment);
    size_t cells_per_bar;
    if (!grow_cells_per_bar(&request->meter, &cells_per_bar) || request->key.tonic_pitch_class < 0 ||
        request->key.tonic_pitch_class > 11 ||
        (request->key.mode != PLAN_MODE_MAJOR && request->key.mode != PLAN_MODE_MINOR) ||
        (request->motive && request->motive->note_count < 2) ||
        (accompanied && !grow_accompaniment_valid(&request->meter, &accompaniment)) ||
        !template_buildable(&layout, &request->meter)) {
        return PIECE_ERR_BAD_REQUEST;
    }
    PlanTime bar = grow_bar_length(&request->meter);
    if (request->motive && plan_time_compare(request->motive->length, bar) > 0) {
        return PIECE_ERR_MOTIVE_TOO_LONG;
    }

    PiecePlan *plan = plan_create();
    if (!plan) {
        return PIECE_ERR_ALLOC;
    }
    PlanSpan piece = {{0, 1}, bars_of(bar, layout.bars)};
    PlanSpan material = {piece.length, bars_of(bar, piece_material_bars(layout.section_count) + request->given_melodies)};
    PlanSpan form = {{0, 1}, {0, 1}};
    PieceResult result = from_plan(plan_time_add(piece.length, material.length, &form.length));
    NodeId form_id = PLAN_NODE_NONE;
    NodeId source = PLAN_NODE_NONE;
    NodeId sections[FORM_TEMPLATE_MAX_SECTIONS];
    PlanLanePayload payload;
    LaneSpanId lane;

    if (result == PIECE_OK) {
        result = from_plan(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, form, form_template_name(layout.kind),
                                         &form_id));
    }
    /* The meter is Given over the piece and the material; the key lane comes with the sections (piece_plan_keys). */
    if (result == PIECE_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.meter = request->meter;
        result = from_plan(plan_add_lane_span(plan, form_id, PLAN_LANE_METER, 0, form, &payload, &lane));
        result = result == PIECE_OK ? from_plan(plan_lane_span_set_state(plan, lane, PLAN_STATE_GIVEN)) : result;
    }
    /* The sections in order, then the material; the sections' motive references name m0, so they come last. */
    PlanTime start = {0, 1};
    for (size_t i = 0; result == PIECE_OK && i < layout.section_count; i++) {
        PlanSpan span = {start, bars_of(bar, layout.sections[i].bars)};
        result = add_section(plan, request, &layout.sections[i], span, form_id, sections,
                             accompanied ? &accompaniment : NULL, &sections[i]);
        if (result == PIECE_OK) {
            result = from_plan(plan_time_add(start, span.length, &start));
        }
    }
    if (result == PIECE_OK) {
        result = add_material(plan, request, form_id, material, bar, &source);
    }
    for (size_t i = 0; result == PIECE_OK && i < layout.section_count; i++) {
        result = add_section_motive(plan, &layout.sections[i], sections[i], source);
    }
    if (result == PIECE_OK && request->has_style) {
        PlanConstraint style;
        memset(&style, 0, sizeof(style));
        style.kind = PLAN_CONSTRAINT_STYLE;
        style.params.style = request->style;
        result = from_plan(plan_node_add_constraint(plan, form_id, &style));
    }
    /* The keys and the cadences across them are planned from the request's seed, home only unless it asks to modulate;
   then the form is Given. */
    if (result == PIECE_OK) {
        PieceKeyPlan keys;
        FillRng rng;
        fill_rng_seed(&rng, request->seed);
        result = piece_plan_keys(plan, request->modulate ? &rng : NULL, &keys);
        result = result == PIECE_OK && request->modulate ? piece_plan_cadences(plan, &keys) : result;
    }
    if (result == PIECE_OK) {
        result = from_plan(plan_node_set_state(plan, form_id, PLAN_STATE_GIVEN));
    }
    if (result != PIECE_OK) {
        plan_destroy(plan);
        return result;
    }
    *out = plan;
    return PIECE_OK;
}
