#include <ursatz_composer/grow.h>

#include <stdio.h>
#include <string.h>

#include <standard-harmony/harmony_cadence_rules.h>
#include <standard-texture/texture_rules.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

#include "util/plan_util.h"
#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"
#include "motive/motive_internal.h"

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

bool grow_cells_per_bar(const PlanMeter *meter, size_t *cells) {
    return harmony_rhythm_cells_per_bar(meter, cells);
}

Rational grow_bar_length(const PlanMeter *meter) {
    Rational bar = {0, 1};
    if (rational_make(meter->beats_per_bar, (long long)meter->beat_unit, &bar) != RATIONAL_OK) {
        bar.num = 0;
        bar.den = 1;
    }
    return bar;
}

GrowMotiveFit grow_motive_fit(const Motive *motive, Rational bar, size_t idea_bars) {
    Rational span = {0, 1};
    long long bars = 0;
    if (bar.num <= 0 || rational_sub(motive->length, motive_pickup(motive), &span) != RATIONAL_OK || span.num <= 0 ||
        rational_div(span, bar, &span) != RATIONAL_OK || rational_ceil(span, &bars) != RATIONAL_OK) {
        return GROW_FIT_SHORTER;
    }
    return bars < (long long)idea_bars ? GROW_FIT_SHORTER : (bars == (long long)idea_bars ? GROW_FIT_WHOLE : GROW_FIT_LONGER);
}

bool grow_motive_fits_idea(const Motive *motive, Rational bar, size_t idea_bars) {
    Rational room = {0, 1};
    /* A pickup is part of the motive's length, so it counts toward the idea's: the motive is placed from its start. */
    return grow_motive_fit(motive, bar, idea_bars) != GROW_FIT_LONGER &&
           rational_mul_int(bar, (long long)idea_bars, &room) == RATIONAL_OK && rational_cmp(motive->length, room) <= 0;
}

size_t grow_motive_layout_bars(const Motive *motive, Rational bar) {
    Rational span = {0, 1};
    long long bars = 0;
    if (bar.num <= 0 || rational_div(motive->length, bar, &span) != RATIONAL_OK || rational_ceil(span, &bars) != RATIONAL_OK ||
        bars < 1) {
        return 1;
    }
    return (size_t)bars;
}

static Rational times(Rational time, size_t count) {
    Rational out = {0, 1};
    if (rational_mul_int(time, (long long)count, &out) != RATIONAL_OK) {
        out.num = 0;
        out.den = 1;
    }
    return out;
}

static GrowResult from_plan(PlanResult result) {
    return result == PLAN_OK ? GROW_OK : (result == PLAN_ERR_ALLOC ? GROW_ERR_ALLOC : GROW_ERR_BAD_REQUEST);
}

/* Whether half of the theme has room for a cadence at all. */
static bool half_fits(size_t bars, size_t cells_per_bar, CadenceType cadence) {
    size_t cells = bars / 2 * cells_per_bar;
    /* A cadential progression has no opening tonic, so it needs one cell fewer than the phrase model. */
    return cells + 1 >= harmony_phrase_room_cells(cadence, cells_per_bar);
}

/* Whether an antecedent can end in `first` below a theme that ends in `last`: weak, and weaker. */
static bool cadences_ordered(CadenceType first, CadenceType last) {
    return (first == CADENCE_TYPE_HALF || first == CADENCE_TYPE_IMPERFECT_AUTHENTIC) &&
           harmony_cadence_strength(first) <
               harmony_cadence_strength(last);
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
                          CadenceType cadence, CadenceType first) {
    size_t cells_per_bar;
    bool antecedent = pairing->beginning == PLAN_THEME_BEGINNING_ANTECEDENT;
    return grow_cells_per_bar(meter, &cells_per_bar) && (bars == 4 || bars == 8) &&
           grow_pairing_archetype(pairing) >= 0 &&
           (cadence == CADENCE_TYPE_PERFECT_AUTHENTIC || cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC || cadence == CADENCE_TYPE_HALF) &&
           half_fits(bars, cells_per_bar, cadence) &&
           (!antecedent || (cadences_ordered(first, cadence) && half_fits(bars, cells_per_bar, first)));
}

/* The bars of an idea that has `units` eighths of a theme of `bars` bars, at least one (a half-bar idea is a bar's). */
static size_t idea_bars(size_t bars, size_t units) {
    size_t whole = bars * units / 8;
    return whole < 1 ? 1 : whole;
}

/* Whether a slot's motive is one the theme can be built from: see GrowRequest.slot_motives. */
static bool slot_motive_valid(const GrowRequest *request, GrowSlot slot, Rational bar) {
    const Motive *motive = request->slot_motives[slot];
    size_t units;
    Rational room;
    bool used = grow_slot_used(&request->pairing, slot, &units);
    if (!used || motive->form != MOTIVE_SKETCH_FORM_FULL || motive->note_count < 2 ||
        motive_meter(motive).beats_per_bar != request->meter.beats_per_bar ||
        motive_meter(motive).beat_unit != request->meter.beat_unit ||
        (motive->key_source != MOTIVE_SKETCH_KEY_UNSPECIFIED &&
         (motive->tonic_pc != analysis_key_tonic_pitch_class(&request->key) ||
          motive->minor != (request->key.mode == KEY_STRUCTURE_MODE_MINOR)))) {
        return false;
    }
    /* The first idea of the slot has units eighths of the theme; the longest is as long as that idea. */
    if (units > 0 && rational_make(bar.num * (long)request->bars * (long)units, bar.den * 8, &room) == RATIONAL_OK &&
        rational_cmp(motive->length, room) > 0) {
        return false;
    }
    return grow_motive_fits_idea(motive, bar, idea_bars(request->bars, units));
}

/* Whether the motive of a single idea is one the theme can be built from: see GrowRequest.idea_motives. */
static bool idea_motive_valid(const GrowRequest *request, GrowIdea idea, Rational bar) {
    const Motive *motive = request->idea_motives[idea];
    GrowPlace place;
    Rational room;
    if (idea == GROW_IDEA_BASIC || !grow_idea_place(&request->pairing, idea, &place) ||
        motive->form != MOTIVE_SKETCH_FORM_FULL || motive->note_count < 2 ||
        motive_meter(motive).beats_per_bar != request->meter.beats_per_bar ||
        motive_meter(motive).beat_unit != request->meter.beat_unit ||
        (motive->key_source != MOTIVE_SKETCH_KEY_UNSPECIFIED &&
         (motive->tonic_pc != analysis_key_tonic_pitch_class(&request->key) ||
          motive->minor != (request->key.mode == KEY_STRUCTURE_MODE_MINOR)))) {
        return false;
    }
    size_t units = grow_place_units(&request->pairing, place);
    if (rational_make(bar.num * (long)request->bars * (long)units, bar.den * 8, &room) == RATIONAL_OK &&
        rational_cmp(motive->length, room) > 0) {
        return false;
    }
    return grow_motive_fits_idea(motive, bar, idea_bars(request->bars, units));
}

GrowResult grow_build_plan(const GrowRequest *request, PiecePlan **out) {
    if (!request || !request->motive || !out) {
        return GROW_ERR_NULL;
    }
    *out = NULL;
    /* An antecedent ends in a weak cadence, HC unless the request says IAC, and the theme in a stronger one. */
    bool antecedent = request->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT;
    CadenceType first = request->first_cadence == CADENCE_TYPE_PERFECT_AUTHENTIC ? CADENCE_TYPE_HALF : request->first_cadence;
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
        request->motive->note_count < 2 || analysis_key_tonic_pitch_class(&request->key) < 0 || analysis_key_tonic_pitch_class(&request->key) > 11 ||
        (has_accompaniment && !grow_accompaniment_valid(&request->meter, &accompaniment)) ||
        (has_accompaniment && request->has_voicing) ||
        (accompaniment_motive && (!accompaniment_motive->accompaniment ||
                                  accompaniment_motive->note_count < 1))) {
        return GROW_ERR_BAD_REQUEST;
    }
    Rational bar = grow_bar_length(&request->meter);
    size_t basic_units = 0;
    /* The basic motive fills the basic idea: 2 bars of an 8-bar theme, 1 of a 4-bar one. */
    if (!grow_slot_used(&request->pairing, GROW_SLOT_BASIC, &basic_units) ||
        !grow_motive_fits_idea(request->motive, bar, idea_bars(request->bars, basic_units))) {
        return GROW_ERR_MOTIVE_TOO_LONG;
    }
    /* The slot motives, each a node of the material after m0. */
    size_t slot_count = 0;
    /* The bars the motives take beyond the one each has in the base layout: a motive takes its own bars. */
    size_t extra_bars = grow_motive_layout_bars(request->motive, bar) - 1;
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; slot < GROW_SLOT_COUNT; slot++) {
        if (request->slot_motives[slot]) {
            if (!slot_motive_valid(request, slot, bar)) {
                return GROW_ERR_BAD_REQUEST;
            }
            slot_count++;
            extra_bars += grow_motive_layout_bars(request->slot_motives[slot], bar) - 1;
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
            extra_bars += grow_motive_layout_bars(request->idea_motives[idea], bar) - 1;
        }
    }
    /* The derived motives take the bars of the longest motive given: the section's own and the four of the ideas. */
    size_t longest = grow_motive_layout_bars(request->motive, bar);
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; slot < GROW_SLOT_COUNT; slot++) {
        size_t bars = request->slot_motives[slot] ? grow_motive_layout_bars(request->slot_motives[slot], bar) : 0;
        longest = bars > longest ? bars : longest;
    }
    for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
        size_t bars = request->idea_motives[idea] ? grow_motive_layout_bars(request->idea_motives[idea], bar) : 0;
        longest = bars > longest ? bars : longest;
    }
    extra_bars += GROW_PLACE_COUNT * (longest - 1);
    size_t material_bars = GROW_MATERIAL_BARS + slot_count + idea_count + extra_bars;
    if (accompaniment_motive && rational_cmp(accompaniment_motive->length, bar) > 0) {
        return GROW_ERR_BAD_REQUEST;
    }

    PiecePlan *plan = plan_create();
    if (!plan) {
        return GROW_ERR_ALLOC;
    }
    PlanSpan theme = {{0, 1}, times(bar, request->bars)};
    PlanSpan material = {theme.length, times(bar, material_bars)};
    PlanSpan form = {{0, 1}, {0, 1}};
    Rational total;
    GrowResult result = from_plan(plan_result_of_rational(rational_add(theme.length, material.length, &total)));
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
    /* The slot motives follow m0, each at its own bars. */
    size_t slot_bar = grow_motive_layout_bars(request->motive, bar);
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; result == GROW_OK && slot < GROW_SLOT_COUNT; slot++) {
        Rational start;
        if (!request->slot_motives[slot]) {
            continue;
        }
        result = from_plan(plan_result_of_rational(rational_add(material.start, times(bar, slot_bar), &start)));
        slot_bar += grow_motive_layout_bars(request->slot_motives[slot], bar);
        result = result == GROW_OK ? from_plan(motive_attach(plan, idea, start, request->slot_motives[slot],
                                                             grow_slot_node_name(slot), NULL, NULL))
                                   : result;
    }
    /* The motives of single ideas follow the slot motives, each at its own bars. */
    size_t idea_bar = slot_bar;
    for (int idea_at = 0; result == GROW_OK && idea_at < GROW_IDEA_COUNT; idea_at++) {
        Rational start;
        char name[8];
        if (!request->idea_motives[idea_at]) {
            continue;
        }
        result = from_plan(plan_result_of_rational(rational_add(material.start, times(bar, idea_bar), &start)));
        idea_bar += grow_motive_layout_bars(request->idea_motives[idea_at], bar);
        result = result == GROW_OK ? from_plan(motive_attach(plan, idea, start, request->idea_motives[idea_at],
                                                             grow_idea_node_name((GrowIdea)idea_at, name, sizeof(name)),
                                                             NULL, NULL))
                                   : result;
    }
    if (result == GROW_OK && accompaniment_motive) {
        Rational start;
        result = from_plan(plan_result_of_rational(rational_add(material.start, times(bar, material_bars - 1), &start)));
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

bool grow_read_motive(const PiecePlan *plan, NodeId node, const PlanMeter *meter, const AnalysisKey *key, Motive *out) {
    LaneSpanId ids[16];
    size_t found = plan_node_lane_spans(plan, node, ids, 16);
    for (size_t i = 0; i < found && i < 16; i++) {
        if (plan_lane_span_lane(plan, ids[i]) == PLAN_LANE_MELODY && plan_lane_span_voice(plan, ids[i]) == 0 &&
            plan_lane_span_state(plan, ids[i]) != PLAN_STATE_CONSTRAINED) {
            const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
            if (notes->note_count == 0 || notes->note_count > MOTIVE_MAX_NOTES) {
                return false;
            }
            motive_init(out, *meter);
            motive_set_key(out, key, MOTIVE_SKETCH_KEY_GIVEN);
            out->note_count = notes->note_count;
            for (size_t n = 0; n < notes->note_count; n++) {
                out->notes[n].pitch = notes->notes[n].midi;
                out->notes[n].onset = notes->notes[n].offset;
                out->notes[n].duration = notes->notes[n].duration;
            }
            MotiveSketchNote *last = &out->notes[out->note_count - 1];
            return rational_add(last->onset, last->duration, &out->length) == RATIONAL_OK;
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
