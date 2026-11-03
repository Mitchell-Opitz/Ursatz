#include <ursatz_composer/grow.h>

#include <stdio.h>
#include <string.h>

#include <standard-harmony/harmony_cadence_rules.h>
#include <standard-texture/texture_rules.h>

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

const char *grow_slot_name(GrowSlot slot) {
    static const char *const NAMES[GROW_SLOT_COUNT] = {"basic", "contrasting", "continuation", "cadential"};
    return slot >= 0 && slot < GROW_SLOT_COUNT ? NAMES[slot] : "unknown";
}

const char *grow_slot_function(GrowSlot slot) {
    static const char *const FUNCTIONS[GROW_SLOT_COUNT] = {"beginning", "contrast", "continuation", "cadential"};
    return slot >= 0 && slot < GROW_SLOT_COUNT ? FUNCTIONS[slot] : "unknown";
}

const char *grow_slot_node_name(GrowSlot slot) {
    static const char *const NAMES[GROW_SLOT_COUNT] = {NULL, "s1", "s2", "s3"};
    return slot >= 0 && slot < GROW_SLOT_COUNT ? NAMES[slot] : NULL;
}

NodeId grow_find_slot_node(const PiecePlan *plan, GrowSlot slot) {
    const char *wanted = grow_slot_node_name(slot);
    for (NodeId id = 1; wanted && id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && name && strcmp(name, wanted) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

const char *grow_idea_name(GrowIdea idea) {
    static const char *const NAMES[GROW_IDEA_COUNT] = {
        "basic idea", "repetition", "contrasting idea", "consequent basic idea", "consequent contrasting idea",
        "fragment 1", "fragment 2", "cadential", "cadential 1", "cadential 2"};
    return idea >= 0 && idea < GROW_IDEA_COUNT ? NAMES[idea] : "unknown";
}

bool grow_idea_place(const PlanThemePairing *pairing, GrowIdea idea, GrowPlace *place) {
    bool presentation = pairing->beginning == PLAN_THEME_BEGINNING_PRESENTATION;
    bool continuation = pairing->ending == PLAN_THEME_ENDING_CONTINUATION;
    bool consequent = pairing->ending == PLAN_THEME_ENDING_CONSEQUENT;
    bool cadential = pairing->ending == PLAN_THEME_ENDING_CADENTIAL;
    GrowPlace found = GROW_PLACE_BASIC;
    bool has = false;
    switch (idea) {
    case GROW_IDEA_BASIC:
        has = true;
        break;
    case GROW_IDEA_REPETITION:
        found = GROW_PLACE_REPETITION;
        has = presentation;
        break;
    case GROW_IDEA_CONTRASTING:
        found = GROW_PLACE_CONTRAST;
        has = !presentation;
        break;
    case GROW_IDEA_CONSEQUENT_BASIC:
        found = GROW_PLACE_RESTATED;
        has = consequent;
        break;
    case GROW_IDEA_CONSEQUENT_CONTRASTING:
        found = GROW_PLACE_CONSEQUENT_CONTRAST;
        has = consequent;
        break;
    case GROW_IDEA_FRAGMENT_1:
        found = GROW_PLACE_FRAGMENT_1;
        has = continuation;
        break;
    case GROW_IDEA_FRAGMENT_2:
        found = GROW_PLACE_FRAGMENT_2;
        has = continuation;
        break;
    case GROW_IDEA_CADENTIAL:
        found = GROW_PLACE_CADENTIAL;
        has = continuation;
        break;
    case GROW_IDEA_CADENTIAL_1:
        found = GROW_PLACE_CADENTIAL_1;
        has = cadential;
        break;
    case GROW_IDEA_CADENTIAL_2:
        found = GROW_PLACE_CADENTIAL_2;
        has = cadential;
        break;
    case GROW_IDEA_COUNT:
        break;
    }
    if (has && place) {
        *place = found;
    }
    return has;
}

bool grow_idea_in_pairing(const PlanThemePairing *pairing, GrowIdea idea) {
    return grow_idea_place(pairing, idea, NULL);
}

GrowIdea grow_place_idea(const PlanThemePairing *pairing, GrowPlace place) {
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        GrowPlace at;
        if (grow_idea_place(pairing, (GrowIdea)idea, &at) && at == place) {
            return (GrowIdea)idea;
        }
    }
    return GROW_IDEA_BASIC;
}

size_t grow_place_units(const PlanThemePairing *pairing, GrowPlace place) {
    bool continuation = pairing->ending == PLAN_THEME_ENDING_CONTINUATION;
    return (place == GROW_PLACE_FRAGMENT_1 || place == GROW_PLACE_FRAGMENT_2) && continuation ? 1 : 2;
}

const char *grow_idea_node_name(GrowIdea idea, char *buffer, size_t size) {
    snprintf(buffer, size, "i%d", (int)idea);
    return buffer;
}

NodeId grow_find_idea_node(const PiecePlan *plan, GrowIdea idea) {
    char wanted[8];
    grow_idea_node_name(idea, wanted, sizeof(wanted));
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && name && strcmp(name, wanted) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

bool grow_idea_own(const GrowThemeSpec *spec, GrowPlace idea) {
    bool first;
    GrowSlot slot = grow_idea_slot(&spec->pairing, idea, &first);
    return (idea != GROW_PLACE_BASIC && spec->has_idea_source[idea]) ||
           (idea != GROW_PLACE_BASIC && first && spec->has_slot_source[slot]);
}

GrowSlot grow_idea_slot(const PlanThemePairing *pairing, GrowPlace idea, bool *first) {
    bool presentation = pairing->beginning == PLAN_THEME_BEGINNING_PRESENTATION;
    GrowSlot slot = GROW_SLOT_BASIC;
    bool is_first = false;
    switch (idea) {
    case GROW_PLACE_BASIC:
        is_first = true;
        break;
    case GROW_PLACE_REPETITION: /* also the contrasting idea of the other beginnings */
        slot = presentation ? GROW_SLOT_BASIC : GROW_SLOT_CONTRASTING;
        is_first = !presentation;
        break;
    case GROW_PLACE_FRAGMENT_1: /* also the consequent basic idea and cadential 1 */
        slot = pairing->ending == PLAN_THEME_ENDING_CONTINUATION ? GROW_SLOT_CONTINUATION
               : pairing->ending == PLAN_THEME_ENDING_CADENTIAL  ? GROW_SLOT_CADENTIAL
                                                                 : GROW_SLOT_BASIC;
        is_first = slot != GROW_SLOT_BASIC;
        break;
    case GROW_PLACE_FRAGMENT_2: /* also the consequent contrasting idea and cadential 2 */
        slot = pairing->ending == PLAN_THEME_ENDING_CONTINUATION ? GROW_SLOT_CONTINUATION
               : pairing->ending == PLAN_THEME_ENDING_CADENTIAL  ? GROW_SLOT_CADENTIAL
                                                                 : GROW_SLOT_CONTRASTING;
        break;
    case GROW_PLACE_CADENTIAL:
        slot = GROW_SLOT_CADENTIAL;
        is_first = true;
        break;
    default:
        break;
    }
    if (first) {
        *first = is_first;
    }
    return slot;
}

bool grow_slot_used(const PlanThemePairing *pairing, GrowSlot slot, size_t *units) {
    size_t room = 2;
    bool used = false;
    switch (slot) {
    case GROW_SLOT_BASIC:
        used = true;
        break;
    case GROW_SLOT_CONTRASTING:
        used = pairing->beginning != PLAN_THEME_BEGINNING_PRESENTATION ||
               pairing->ending == PLAN_THEME_ENDING_CONSEQUENT;
        room = pairing->beginning != PLAN_THEME_BEGINNING_PRESENTATION ? 2 : 0;
        break;
    case GROW_SLOT_CONTINUATION:
        used = pairing->ending == PLAN_THEME_ENDING_CONTINUATION;
        room = 1;
        break;
    case GROW_SLOT_CADENTIAL:
        used = pairing->ending != PLAN_THEME_ENDING_CONSEQUENT;
        break;
    default:
        break;
    }
    if (units) {
        *units = room;
    }
    return used;
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
    return cells + 1 >= harmony_phrase_room_cells(cadence, cells_per_bar);
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
    return accompaniment->pattern >= PLAN_ACCOMPANIMENT_ANY && accompaniment->pattern <= PLAN_ACCOMPANIMENT_MOTIVE &&
           (accompaniment->pattern == PLAN_ACCOMPANIMENT_MOTIVE) == (accompaniment->motive_node[0] != '\0') &&
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

/* Whether a slot's motive is one the theme can be built from: see GrowRequest.slot_motives. */
static bool slot_motive_valid(const GrowRequest *request, GrowSlot slot, PlanTime bar) {
    const Motive *motive = request->slot_motives[slot];
    size_t units;
    PlanTime room;
    bool used = grow_slot_used(&request->pairing, slot, &units);
    if (!used || motive->form != MOTIVE_FORM_FULL || motive->note_count < 2 ||
        motive->meter.beats_per_bar != request->meter.beats_per_bar ||
        motive->meter.beat_unit != request->meter.beat_unit ||
        (motive->key_source != MOTIVE_KEY_UNSPECIFIED &&
         (motive->key.tonic_pitch_class != request->key.tonic_pitch_class || motive->key.mode != request->key.mode))) {
        return false;
    }
    /* The first idea of the slot has units eighths of the theme; the longest is a bar. */
    if (units > 0 && plan_time_make(bar.num * (long)request->bars * (long)units, bar.den * 8, &room) == PLAN_OK &&
        plan_time_compare(motive->length, room) > 0) {
        return false;
    }
    return plan_time_compare(motive->length, bar) <= 0;
}

/* Whether the motive of a single idea is one the theme can be built from: see GrowRequest.idea_motives. */
static bool idea_motive_valid(const GrowRequest *request, GrowIdea idea, PlanTime bar) {
    const Motive *motive = request->idea_motives[idea];
    GrowPlace place;
    PlanTime room;
    if (idea == GROW_IDEA_BASIC || !grow_idea_place(&request->pairing, idea, &place) ||
        motive->form != MOTIVE_FORM_FULL || motive->note_count < 2 ||
        motive->meter.beats_per_bar != request->meter.beats_per_bar ||
        motive->meter.beat_unit != request->meter.beat_unit ||
        (motive->key_source != MOTIVE_KEY_UNSPECIFIED &&
         (motive->key.tonic_pitch_class != request->key.tonic_pitch_class || motive->key.mode != request->key.mode))) {
        return false;
    }
    size_t units = grow_place_units(&request->pairing, place);
    if (plan_time_make(bar.num * (long)request->bars * (long)units, bar.den * 8, &room) == PLAN_OK &&
        plan_time_compare(motive->length, room) > 0) {
        return false;
    }
    return plan_time_compare(motive->length, bar) <= 0;
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
    /* An accompaniment motive makes the accompaniment: pattern MOTIVE, naming the node "acc". */
    const Motive *accompaniment_motive = request->accompaniment_motive;
    PlanAccompaniment accompaniment = request->accompaniment;
    bool has_accompaniment = request->has_accompaniment || accompaniment_motive;
    if (accompaniment_motive) {
        if (!request->has_accompaniment) {
            memset(&accompaniment, 0, sizeof(accompaniment));
            accompaniment.texture = -1;
        }
        accompaniment.pattern = PLAN_ACCOMPANIMENT_MOTIVE;
        snprintf(accompaniment.motive_node, sizeof(accompaniment.motive_node), "%s", GROW_ACCOMPANIMENT_NAME);
    }
    if (!grow_theme_buildable(&request->meter, request->bars, &request->pairing, request->cadence, first) ||
        request->motive->note_count < 2 || request->key.tonic_pitch_class < 0 || request->key.tonic_pitch_class > 11 ||
        (has_accompaniment && !grow_accompaniment_valid(&request->meter, &accompaniment)) ||
        (has_accompaniment && request->has_voicing) ||
        (accompaniment_motive && (accompaniment_motive->role != MOTIVE_ROLE_ACCOMPANIMENT ||
                                  accompaniment_motive->note_count < 1))) {
        return GROW_ERR_BAD_REQUEST;
    }
    PlanTime bar = grow_bar_length(&request->meter);
    if (plan_time_compare(request->motive->length, bar) > 0) {
        return GROW_ERR_MOTIVE_TOO_LONG;
    }
    /* The slot motives, each a node of the material after m0. */
    size_t slot_count = 0;
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; slot < GROW_SLOT_COUNT; slot++) {
        if (request->slot_motives[slot]) {
            if (!slot_motive_valid(request, slot, bar)) {
                return GROW_ERR_BAD_REQUEST;
            }
            slot_count++;
        }
    }
    /* The motives of single ideas follow the slot motives. */
    size_t idea_count = 0;
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        if (request->idea_motives[idea]) {
            if (!idea_motive_valid(request, (GrowIdea)idea, bar)) {
                return GROW_ERR_BAD_REQUEST;
            }
            idea_count++;
        }
    }
    size_t material_bars = GROW_MATERIAL_BARS + slot_count + idea_count;
    if (accompaniment_motive && plan_time_compare(accompaniment_motive->length, bar) > 0) {
        return GROW_ERR_BAD_REQUEST;
    }

    PiecePlan *plan = plan_create();
    if (!plan) {
        return GROW_ERR_ALLOC;
    }
    PlanSpan theme = {{0, 1}, times(bar, request->bars)};
    PlanSpan material = {theme.length, times(bar, material_bars)};
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

    if (result == GROW_OK && has_accompaniment) {
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_ACCOMPANIMENT;
        constraint.params.accompaniment = accompaniment;
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
        result = from_plan(motive_attach_sourced(plan, idea, material.start, request->motive, GROW_SOURCE_NAME,
                                                 request->source, NULL, NULL));
    }
    /* The slot motives follow m0, a bar each. */
    size_t slot_bar = 1;
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; result == GROW_OK && slot < GROW_SLOT_COUNT; slot++) {
        PlanTime start;
        if (!request->slot_motives[slot]) {
            continue;
        }
        result = from_plan(plan_time_add(material.start, times(bar, slot_bar++), &start));
        result = result == GROW_OK ? from_plan(motive_attach(plan, idea, start, request->slot_motives[slot],
                                                             grow_slot_node_name(slot), NULL, NULL))
                                   : result;
    }
    /* The motives of single ideas follow the slot motives, a bar each. */
    size_t idea_bar = 1 + slot_count;
    for (int idea_at = 0; result == GROW_OK && idea_at < GROW_IDEA_COUNT; idea_at++) {
        PlanTime start;
        char name[8];
        if (!request->idea_motives[idea_at]) {
            continue;
        }
        result = from_plan(plan_time_add(material.start, times(bar, idea_bar++), &start));
        result = result == GROW_OK ? from_plan(motive_attach(plan, idea, start, request->idea_motives[idea_at],
                                                             grow_idea_node_name((GrowIdea)idea_at, name, sizeof(name)),
                                                             NULL, NULL))
                                   : result;
    }
    if (result == GROW_OK && accompaniment_motive) {
        PlanTime start;
        result = from_plan(plan_time_add(material.start, times(bar, material_bars - 1), &start));
        result = result == GROW_OK ? from_plan(motive_attach(plan, idea, start, accompaniment_motive,
                                                             GROW_ACCOMPANIMENT_NAME, NULL, NULL))
                                   : result;
    }
    if (result == GROW_OK && request->chord_count > 0) {
        result = plan_give_chords(plan, theme_id, request->chords, request->chord_count) == GIVEN_OK
                     ? GROW_OK
                     : GROW_ERR_BAD_REQUEST;
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
