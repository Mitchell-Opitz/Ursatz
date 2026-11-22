#include <ursatz_composer/grow_types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/form/hybrid_rules.h>
#include <theory/form/phrase_beginning.h>
#include <theory/form/phrase_ending.h>
#include <theory/form/phrase_level_form.h>
#include <theory/tonality/tonal_key.h>

#include <ursatz_composer/form_return.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <plan/plan_state.h>
#include <ursatz_composer/variant_registry.h>

#include "structure/plan_util.h"
#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"
#include "motive/motive_internal.h"
#include "util/util.h"

/* ------------------------------------------------------------- the plan */

/* The most key spans read before a section: the ones of the piece up to its end, with a few to spare. */
#define MAX_KEY_SPANS 32

/*
 * The key lane over the section (R-069): one key, or the key it starts in and a second key from its half; and, when the
 * key lane has a span that ends where the section starts, the key before it. Spans are contiguous. False when the
 * lane does not tile the section that way.
 */
bool grow_read_keys(const Plan *plan, GrowThemeRequest *request) {
    LaneSpanId spans[MAX_KEY_SPANS];
    Rational end;
    Rational half;
    PlanSpan up_to = {{0, 1}, {0, 1}};
    if (plan_span_end(request->span, &end) != PLAN_OK || rational_make(end.num, end.den, &up_to.length) != RATIONAL_OK ||
        rational_div_int(request->span.length, 2, &half) != RATIONAL_OK ||
        rational_add(request->span.start, half, &half) != RATIONAL_OK) {
        return false;
    }
    size_t found = plan_lane_find(plan, PLAN_LANE_REGION, 0, up_to, spans, MAX_KEY_SPANS);
    if (found == 0 || found > MAX_KEY_SPANS) {
        return false;
    }
    size_t at = found;
    for (size_t i = 0; i < found; i++) {
        PlanSpan span = plan_lane_span_span(plan, spans[i]);
        Rational span_end;
        if (plan_span_end(span, &span_end) == PLAN_OK && rational_cmp(span.start, request->span.start) <= 0 &&
            rational_cmp(request->span.start, span_end) < 0) {
            at = i;
        }
    }
    if (at == found) {
        return false;
    }
    PlanSpan first = plan_lane_span_span(plan, spans[at]);
    Rational first_end;
    if (plan_span_end(first, &first_end) != PLAN_OK) {
        return false;
    }
    request->key = plan_lane_span_payload(plan, spans[at])->region;
    /* A change of mode on one tonic is not a modulation: no pivot (R-499). */
    if (at > 0 && rational_cmp(first.start, request->span.start) == 0 &&
        spelled_key_tonic_pitch_class(&plan_lane_span_payload(plan, spans[at - 1])->region) !=
            spelled_key_tonic_pitch_class(&request->key)) {
        request->has_start_change = true;
        request->from_key = plan_lane_span_payload(plan, spans[at - 1])->region;
    }
    if (rational_cmp(first_end, end) >= 0) {
        return at + 1 == found || rational_cmp(plan_lane_span_span(plan, spans[at + 1]).start, end) >= 0;
    }
    /* A second span starts at the half and holds the rest of the section. */
    if (at + 1 >= found || rational_cmp(first_end, half) != 0) {
        return false;
    }
    PlanSpan second = plan_lane_span_span(plan, spans[at + 1]);
    Rational second_end;
    if (rational_cmp(second.start, half) != 0 || plan_span_end(second, &second_end) != PLAN_OK ||
        rational_cmp(second_end, end) < 0) {
        return false;
    }
    request->has_end_change = true;
    request->end_key = plan_lane_span_payload(plan, spans[at + 1])->region;
    return true;
}

/* Whether the section starts in the first key of the plan, the home key. */
static bool starts_in_home_key(const Plan *plan, const GrowThemeRequest *request) {
    LaneSpanId spans[MAX_KEY_SPANS];
    Rational end;
    PlanSpan whole = {{0, 1}, {0, 1}};
    if (plan_span_end(request->span, &end) != PLAN_OK || rational_make(end.num, end.den, &whole.length) != RATIONAL_OK ||
        plan_lane_find(plan, PLAN_LANE_REGION, 0, whole, spans, MAX_KEY_SPANS) == 0) {
        return false;
    }
    const SpelledKey *home = &plan_lane_span_payload(plan, spans[0])->region;
    return spelled_key_tonic_pitch_class(home) == spelled_key_tonic_pitch_class(&request->key) &&
           composer_key_is_minor(home) == composer_key_is_minor(&request->key);
}

/* The kind of spec the candidate loop draws (grow_generate_specs). */
static bool theme_draw_spec(const Plan *plan, const void *request, const GrowContext *context, FillRng *rng, void *spec) {
    return grow_draw_spec(plan, (const GrowThemeRequest *)request, context, rng, (GrowThemeSpec *)spec);
}

static bool theme_same_spec(const void *a, const void *b) {
    return grow_same_spec((const GrowThemeSpec *)a, (const GrowThemeSpec *)b);
}

static void theme_label_spec(const void *spec, char *out, size_t size) {
    grow_spec_label((const GrowThemeSpec *)spec, out, size);
}

static const GrowSpecKind THEME_KIND = {sizeof(GrowThemeSpec), theme_draw_spec, theme_same_spec, theme_label_spec};

bool grow_read_request(const Plan *plan, FillTarget target, GrowThemeRequest *out) {
    GrowThemeRequest request;
    memset(&request, 0, sizeof(request));
    if (target.kind != FILL_TARGET_NODE || !plan_has_node(plan, target.id) ||
        plan_node_kind(plan, target.id) != PLAN_NODE_SECTION ||
        plan_node_state(plan, target.id) != PLAN_STATE_CONSTRAINED || plan_node_child_count(plan, target.id) != 0) {
        return false;
    }
    /* A section that returns an earlier one is form-return's, not grown afresh; a held one is the seam stage's. */
    if (form_return_handles(plan, target.id) || plan_section_or_empty(plan, target.id)->leads_in) {
        return false;
    }
    request.section = target.id;
    request.span = plan_node_span(plan, target.id);
    const SectionDescription *section = plan_section_or_empty(plan, target.id);
    const bool archetype = section->has_pairing;
    const bool pairing = section->has_pairing;
    const bool cadence = section->has_cadence;
    PlanArchetype requested = PLAN_ARCHETYPE_SENTENCE;
    if (pairing) {
        request.pairing = section->pairing;
        requested = (PlanArchetype)grow_pairing_archetype(&section->pairing);
    }
    if (cadence) {
        request.cadence = section->cadence;
    }
    if (section->has_first_cadence) {
        /* The first cadence is the theme's; the second is the weak one an antecedent ends in. */
        request.has_first = true;
        request.first_cadence = section->first_cadence;
    }
    request.pedal = section->pedal != 0;
    request.transition = section->has_function && section->function.function == SECTION_FUNCTION_TRANSITION;
    if (section->has_construction) {
        request.has_construction = true;
        request.construction = section->construction;
    }
    for (size_t i = 0; i < plan_node_constraint_count(plan, target.id); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, target.id, i);
        if (constraint->kind == PLAN_CONSTRAINT_ACCOMPANIMENT) {
            request.has_accompaniment = true;
            request.accompaniment = constraint->params.accompaniment;
        } else if (constraint->kind == PLAN_CONSTRAINT_VOICING) {
            request.has_voicing = true;
            request.voicing = constraint->params.voicing;
        } else if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && !request.has_motive_ref) {
            request.has_motive_ref = true;
            request.motive_ref = constraint->params.motive;
        }
    }
    LaneSpanId lane[2];
    if (!archetype || !pairing || !cadence || grow_pairing_archetype(&request.pairing) != (int)requested ||
        (request.pairing.beginning == SECTION_BEGINNING_ANTECEDENT) != request.has_first ||
        plan_lane_find(plan, PLAN_LANE_METER, 0, request.span, lane, 2) != 1 ||
        !plan_span_contains(plan_lane_span_span(plan, lane[0]), request.span)) {
        return false;
    }
    request.meter = plan_lane_span_payload(plan, lane[0])->meter;
    if (!grow_read_keys(plan, &request)) {
        return false;
    }
    plan_meter_bar_length(&request.meter, &request.bar);
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; slot < GROW_SLOT_COUNT; slot++) {
        request.slot_node[slot] = grow_find_slot_node(plan, slot);
    }
    /* The ideas with a motive of their own, by their place under the pairing. */
    for (size_t place = 0; place < GROW_PLACE_COUNT; place++) {
        request.idea_node[place] = PLAN_NODE_NONE;
    }
    for (int idea = GROW_IDEA_BASIC + 1; idea < GROW_IDEA_COUNT; idea++) {
        GrowPlace place;
        if (grow_idea_place(&request.pairing, (GrowIdea)idea, &place)) {
            request.idea_node[place] = grow_find_idea_node(plan, (GrowIdea)idea);
        }
    }
    request.source = request.has_motive_ref ? request.motive_ref.motive : grow_find_source(plan);
    /*
     * A reference with no step to another motive of the material (an independent transition's, R-504) names the motive
     * the section is made from as it is: the ideas, fragments and liquidation come from it, and m0 is not asked for.
     */
    if (request.has_motive_ref && !request.motive_ref.has_step) {
        NodeId m0 = grow_find_source(plan);
        if (m0 != PLAN_NODE_NONE && request.source != m0 && plan_node_kind(plan, request.source) == PLAN_NODE_MOTIVE &&
            plan_node_parent(plan, request.source) == plan_node_parent(plan, m0)) {
            request.independent = true;
            request.has_motive_ref = false;
        }
    }
    if (request.source == PLAN_NODE_NONE || !grow_cells_per_bar(&request.meter, &request.cells_per_bar) ||
        request.bar.num == 0) {
        return false;
    }
    /* The theme is 4 or 8 whole bars, or 3 to 16 with a construction (R-468). */
    Rational bars = {0, 1};
    size_t count = 0;
    size_t most = request.has_construction ? 16 : 8;
    while (rational_cmp(bars, request.span.length) < 0 && count <= most) {
        if (rational_add(bars, request.bar, &bars) != RATIONAL_OK) {
            return false;
        }
        count++;
    }
    if (rational_cmp(bars, request.span.length) != 0 ||
        (request.has_construction ? (count < 3 || count > 16) : (count != 4 && count != 8))) {
        return false;
    }
    request.bars = count;
    GrowLayout layout;
    if (!grow_layout_of(&request, &layout)) {
        return false;
    }
    PlanSpan first_half = request.span;
    first_half.length = layout.beginning;
    static const PlanLaneKind GIVEN_LANES[] = {PLAN_LANE_MELODY, PLAN_LANE_HARMONY, PLAN_LANE_BASS};
    for (size_t l = 0; l < sizeof(GIVEN_LANES) / sizeof(GIVEN_LANES[0]); l++) {
        LaneSpanId held[GROW_MAX_GAPS];
        size_t found = plan_lane_find(plan, GIVEN_LANES[l], 0, first_half, held, GROW_MAX_GAPS);
        for (size_t i = 0; i < found && i < GROW_MAX_GAPS; i++) {
            request.given_first_half = request.given_first_half || plan_lane_span_state(plan, held[i]) == PLAN_STATE_GIVEN;
        }
    }
    PlanSpan second_half = request.span;
    if (rational_add(request.span.start, layout.beginning, &second_half.start) == RATIONAL_OK &&
        rational_sub(request.span.length, layout.beginning, &second_half.length) == RATIONAL_OK && second_half.length.num > 0) {
        for (size_t l = 0; l < sizeof(GIVEN_LANES) / sizeof(GIVEN_LANES[0]); l++) {
            LaneSpanId held[GROW_MAX_GAPS];
            size_t found = plan_lane_find(plan, GIVEN_LANES[l], 0, second_half, held, GROW_MAX_GAPS);
            for (size_t i = 0; i < found && i < GROW_MAX_GAPS; i++) {
                request.given_second_half = request.given_second_half || plan_lane_span_state(plan, held[i]) == PLAN_STATE_GIVEN;
            }
        }
    }
    /* A pedal is kept only under a presentation that opens in the home key, with no Given material in its first half. */
    request.pedal = request.pedal && request.pairing.beginning == SECTION_BEGINNING_PRESENTATION &&
                    !request.has_start_change && !request.given_first_half && starts_in_home_key(plan, &request);
    *out = request;
    return true;
}

static bool grow_applies(const Plan *plan, FillTarget target, void *context) {
    GrowThemeRequest request;
    (void)context;
    return grow_read_request(plan, target, &request) || grow_free_applies(plan, target) || grow_liquidate_applies(plan, target) ||
           grow_coda_applies(plan, target);
}

static bool grow_generate(const Plan *plan, FillTarget target, FillRng *rng, void *context,
                          FillCandidateList *out) {
    const GrowContext *shared = context;
    GrowThemeRequest request;
    if (!grow_read_request(plan, target, &request)) {
        if (grow_liquidate_applies(plan, target)) {
            return grow_liquidate_generate(plan, target, rng, shared, out);
        }
        if (grow_coda_applies(plan, target)) {
            return grow_coda_generate(plan, target, rng, shared, out);
        }
        return grow_free_applies(plan, target) ? grow_free_generate(plan, target, rng, shared, out) : true;
    }
    request.development = shared ? shared->development : GROW_DEVELOP_BALANCED;
    if (shared) {
        request.continuation = shared->continuation;
        request.basic_as_written = shared->basic_as_written;
        request.contrast = shared->contrast;
        request.functions = shared->functions;
        request.knowledge = shared->knowledge;
        request.motif = shared->motif;
    }
    if (request.transition) {
        /* A transition liquidates its continuation (R-504); on the request copy only, so the context stays (R-405). */
        request.continuation.liquidate = true;
    }
    return grow_generate_specs(plan, &request, shared, rng, &THEME_KIND, out);
}

FillResult grow_register(FillOperatorRegistry *registry, const GrowContext *context) {
    FillOperator operator_;
    if (!registry) {
        return FILL_ERR_NULL;
    }
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = GROW_THEME_OPERATOR_NAME;
    operator_.generator.supported_inputs = "Plan section with an archetype and a theme pairing";
    operator_.generator.supported_outputs = "Plan phrase, ideas, motive nodes and open lane spans";
    operator_.generator.configuration_description = "the number of candidates";
    operator_.generator.constraints_description = "standard-form sentence rules";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_GROW;
    operator_.applies = grow_applies;
    operator_.generate = grow_generate;
    operator_.apply = grow_apply;
    operator_.context = (void *)context;
    return fill_registry_register(registry, &operator_);
}
