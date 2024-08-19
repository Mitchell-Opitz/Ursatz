#include <ursatz_composer/grow.h>

#include <string.h>

#include <harmony_cadence_rules.h>
#include <texture_rules.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"

const char *grow_result_name(GrowResult result) {
    switch (result) {
    case GROW_OK:
        return "ok";
    case GROW_ERR_NULL:
        return "null argument";
    case GROW_ERR_ALLOC:
        return "allocation failed";
    case GROW_ERR_MOTIVE_TOO_LONG:
        return "motive too long";
    case GROW_ERR_BAD_REQUEST:
        return "bad request";
    case GROW_ERR_STUCK:
        return "stuck";
    }
    return "unknown";
}

const char *grow_stage_name(GrowStage stage) {
    static const char *const NAMES[GROW_STAGE_COUNT] = {"structure", "motives", "harmony", "bass", "melody", "texture"};
    return stage >= 0 && stage < GROW_STAGE_COUNT ? NAMES[stage] : "unknown";
}

void grow_context_default(GrowContext *context) {
    memset(context, 0, sizeof(*context));
    context->candidates = GROW_DEFAULT_CANDIDATES;
}

bool grow_cells_per_bar(const PlanMeter *meter, size_t *cells) {
    return harmony_rhythm_cells_per_bar(meter, cells);
}

PlanTime grow_bar_length(const PlanMeter *meter) {
    PlanTime bar = {0, 1};
    if (plan_time_make(meter->beats_per_bar, (unsigned long)meter->beat_unit, &bar) != PLAN_OK) {
        bar.num = 0;
        bar.den = 1;
    }
    return bar;
}

static PlanTime times(PlanTime time, size_t count) {
    PlanTime out = {0, 1};
    if (plan_time_make(time.num * (long)count, time.den, &out) != PLAN_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

static GrowResult from_plan(PlanResult result) {
    return result == PLAN_OK ? GROW_OK : (result == PLAN_ERR_ALLOC ? GROW_ERR_ALLOC : GROW_ERR_BAD_REQUEST);
}

/* Whether half of the theme has room for a cadence at all. */
static bool half_fits(size_t bars, size_t cells_per_bar, PlanCadenceType cadence) {
    size_t cells = bars / 2 * cells_per_bar;
    /* A cadential progression has no opening tonic, so it needs one cell fewer than the phrase model. */
    return cells + 1 >= harmony_phrase_min_cells(cadence, cells_per_bar);
}

/* Whether an antecedent can end in `first` below a theme that ends in `last`: weak, and weaker. */
static bool cadences_ordered(PlanCadenceType first, PlanCadenceType last) {
    return (first == PLAN_CADENCE_HC || first == PLAN_CADENCE_IAC) &&
           harmony_cadence_strength(harmony_cadence_type_of(first)) <
               harmony_cadence_strength(harmony_cadence_type_of(last));
}

bool grow_accompaniment_valid(const PlanMeter *meter, const PlanAccompaniment *accompaniment) {
    bool compound = meter->beat_unit == 8 && meter->beats_per_bar % 3 == 0;
    int low = accompaniment->low ? accompaniment->low : PLAN_ACCOMPANIMENT_DEFAULT_LOW;
    int high = accompaniment->high ? accompaniment->high : PLAN_ACCOMPANIMENT_DEFAULT_HIGH;
    return accompaniment->pattern >= PLAN_ACCOMPANIMENT_ANY && accompaniment->pattern <= PLAN_ACCOMPANIMENT_OOM_PAH &&
           accompaniment->texture >= -1 && accompaniment->texture < (int)STANDARD_TEXTURE_COUNT &&
           (accompaniment->texture < 0 || standard_texture_is_homophonic((StandardTexture)accompaniment->texture)) &&
           low >= 0 && high <= 127 && low < high &&
           !(accompaniment->pattern == PLAN_ACCOMPANIMENT_ALBERTI && compound);
}

bool grow_theme_buildable(const PlanMeter *meter, size_t bars, const PlanThemePairing *pairing,
                          PlanCadenceType cadence, PlanCadenceType first) {
    size_t cells_per_bar;
    bool antecedent = pairing->beginning == PLAN_THEME_BEGINNING_ANTECEDENT;
    return grow_cells_per_bar(meter, &cells_per_bar) && (bars == 4 || bars == 8) &&
           grow_pairing_archetype(pairing) >= 0 &&
           (cadence == PLAN_CADENCE_PAC || cadence == PLAN_CADENCE_IAC || cadence == PLAN_CADENCE_HC) &&
           half_fits(bars, cells_per_bar, cadence) &&
           (!antecedent || (cadences_ordered(first, cadence) && half_fits(bars, cells_per_bar, first)));
}

GrowResult grow_build_plan(const GrowRequest *request, PiecePlan **out) {
    if (!request || !request->motive || !out) {
        return GROW_ERR_NULL;
    }
    *out = NULL;
    /* An antecedent ends in a weak cadence, HC unless the request says IAC, and the theme in a stronger one. */
    bool antecedent = request->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT;
    PlanCadenceType first = request->first_cadence == PLAN_CADENCE_PAC ? PLAN_CADENCE_HC : request->first_cadence;
    int archetype = grow_pairing_archetype(&request->pairing);
    if (!grow_theme_buildable(&request->meter, request->bars, &request->pairing, request->cadence, first) ||
        request->motive->note_count < 2 || request->key.tonic_pitch_class < 0 || request->key.tonic_pitch_class > 11 ||
        (request->has_accompaniment && !grow_accompaniment_valid(&request->meter, &request->accompaniment)) ||
        (request->has_accompaniment && request->has_voicing)) {
        return GROW_ERR_BAD_REQUEST;
    }
    PlanTime bar = grow_bar_length(&request->meter);
    if (plan_time_compare(request->motive->length, bar) > 0) {
        return GROW_ERR_MOTIVE_TOO_LONG;
    }

    PiecePlan *plan = plan_create();
    if (!plan) {
        return GROW_ERR_ALLOC;
    }
    PlanSpan theme = {{0, 1}, times(bar, request->bars)};
    PlanSpan material = {theme.length, times(bar, GROW_MATERIAL_BARS)};
    PlanSpan form = {{0, 1}, {0, 1}};
    PlanTime total;
    GrowResult result = from_plan(plan_time_add(theme.length, material.length, &total));
    form.length = total;
    NodeId form_id = PLAN_NODE_NONE;
    NodeId theme_id = PLAN_NODE_NONE;
    NodeId section = PLAN_NODE_NONE;
    NodeId phrase = PLAN_NODE_NONE;
    NodeId idea = PLAN_NODE_NONE;
    PlanLanePayload payload;
    LaneSpanId lane;
    PlanConstraint constraint;

    if (result == GROW_OK) {
        result = from_plan(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, form, "form", &form_id));
    }
    /* The meter and the key are Given over the theme and the material. */
    if (result == GROW_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.meter = request->meter;
        result = from_plan(plan_add_lane_span(plan, form_id, PLAN_LANE_METER, 0, form, &payload, &lane));
        result = result == GROW_OK ? from_plan(plan_lane_span_set_state(plan, lane, PLAN_STATE_GIVEN)) : result;
    }
    if (result == GROW_OK) {
        memset(&payload, 0, sizeof(payload));
        payload.key = request->key;
        result = from_plan(plan_add_lane_span(plan, form_id, PLAN_LANE_KEY, 0, form, &payload, &lane));
        result = result == GROW_OK ? from_plan(plan_lane_span_set_state(plan, lane, PLAN_STATE_GIVEN)) : result;
    }
    if (result == GROW_OK) {
        result = from_plan(plan_node_set_state(plan, form_id, PLAN_STATE_GIVEN));
    }

    /* The theme: open, with what it is to be. */
    if (result == GROW_OK) {
        result = from_plan(plan_add_node(plan, form_id, PLAN_NODE_SECTION, theme, GROW_THEME_NAME, &theme_id));
    }
    memset(&constraint, 0, sizeof(constraint));
    if (result == GROW_OK) {
        constraint.kind = PLAN_CONSTRAINT_ARCHETYPE;
        constraint.params.archetype = (PlanArchetype)archetype;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }
    if (result == GROW_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_THEME_PAIRING;
        constraint.params.theme_pairing = request->pairing;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }
    if (result == GROW_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CADENCE;
        constraint.params.cadence = request->cadence;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }
    if (result == GROW_OK && antecedent) {
        /* The second cadence of the theme is the antecedent's. */
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_CADENCE;
        constraint.params.cadence = first;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }
    if (result == GROW_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_KEY;
        constraint.params.key = request->key;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }

    if (result == GROW_OK && request->has_accompaniment) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_ACCOMPANIMENT;
        constraint.params.accompaniment = request->accompaniment;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }

    if (result == GROW_OK && request->has_voicing) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_VOICING;
        constraint.params.voicing = request->voicing;
        result = from_plan(plan_node_add_constraint(plan, theme_id, &constraint));
    }

    /* The material: not part of the piece, holding the motive. */
    if (result == GROW_OK) {
        result = from_plan(plan_add_node(plan, form_id, PLAN_NODE_SECTION, material, GROW_MATERIAL_NAME, &section));
    }
    if (result == GROW_OK) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_MATERIAL;
        result = from_plan(plan_node_add_constraint(plan, section, &constraint));
    }
    if (result == GROW_OK) {
        result = from_plan(plan_add_node(plan, section, PLAN_NODE_PHRASE, material, GROW_MATERIAL_NAME, &phrase));
    }
    if (result == GROW_OK) {
        result = from_plan(plan_add_node(plan, phrase, PLAN_NODE_IDEA, material, GROW_MATERIAL_NAME, &idea));
    }
    if (result == GROW_OK) {
        result = from_plan(motive_attach(plan, idea, material.start, request->motive, GROW_SOURCE_NAME, NULL, NULL));
    }
    if (result == GROW_OK) {
        result = from_plan(plan_node_set_state(plan, section, PLAN_STATE_REALIZED));
    }
    if (result == GROW_OK) {
        result = from_plan(plan_node_set_state(plan, phrase, PLAN_STATE_REALIZED));
    }
    if (result == GROW_OK) {
        result = from_plan(plan_node_set_state(plan, idea, PLAN_STATE_REALIZED));
    }
    if (result != GROW_OK) {
        plan_destroy(plan);
        return result;
    }
    *out = plan;
    return GROW_OK;
}

NodeId grow_find_source(const PiecePlan *plan) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && name && strcmp(name, GROW_SOURCE_NAME) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

bool grow_read_motive(const PiecePlan *plan, NodeId node, const PlanMeter *meter, const PlanKey *key, Motive *out) {
    LaneSpanId ids[16];
    size_t found = plan_node_lane_spans(plan, node, ids, 16);
    for (size_t i = 0; i < found && i < 16; i++) {
        if (plan_lane_span_lane(plan, ids[i]) == PLAN_LANE_MELODY && plan_lane_span_voice(plan, ids[i]) == 0 &&
            plan_lane_span_state(plan, ids[i]) != PLAN_STATE_CONSTRAINED) {
            const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
            if (notes->note_count == 0 || notes->note_count > MOTIVE_MAX_NOTES) {
                return false;
            }
            memset(out, 0, sizeof(*out));
            out->meter = *meter;
            out->key = *key;
            out->key_source = MOTIVE_KEY_GIVEN;
            out->note_count = notes->note_count;
            memcpy(out->notes, notes->notes, notes->note_count * sizeof(PlanNote));
            PlanNote *last = &out->notes[out->note_count - 1];
            return plan_time_add(last->offset, last->duration, &out->length) == PLAN_OK;
        }
    }
    return false;
}

NodeId grow_find_theme(const PiecePlan *plan, bool open_only) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        bool archetype = false;
        bool pairing = false;
        if (plan_node_kind(plan, id) != PLAN_NODE_SECTION ||
            (open_only && plan_node_state(plan, id) != PLAN_STATE_CONSTRAINED)) {
            continue;
        }
        for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
            PlanConstraintKind kind = plan_node_constraint_at(plan, id, i)->kind;
            archetype = archetype || kind == PLAN_CONSTRAINT_ARCHETYPE;
            pairing = pairing || kind == PLAN_CONSTRAINT_THEME_PAIRING;
        }
        if (archetype && pairing) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}
