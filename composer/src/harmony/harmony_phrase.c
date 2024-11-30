#include <ursatz_composer/harmony_phrase.h>

#include <string.h>

#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"

static bool is_target(FillTarget target) {
    return target.kind == FILL_TARGET_LANE_SPAN;
}

/* The operator's context is non-NULL when it works around Given material (harmony_phrase_register_given). */
static const char AROUND_GIVEN = 1;

static bool read_request(const PiecePlan *plan, LaneSpanId target, bool around_given, PhraseRequest *out) {
    return around_given ? harmony_phrase_read_request_given(plan, target, out)
                        : harmony_phrase_read_request(plan, target, out);
}

static bool phrase_applies(const PiecePlan *plan, FillTarget target, void *context) {
    PhraseRequest request;
    return is_target(target) && read_request(plan, (LaneSpanId)target.id, context != NULL, &request);
}

static bool phrase_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                            FillCandidateList *out) {
    PhraseRequest request;
    if (!is_target(target) || !read_request(plan, (LaneSpanId)target.id, context != NULL, &request)) {
        return false;
    }
    return harmony_phrase_propose(&request, rng, out);
}

void harmony_chord_payload(const PhraseChord *chord, PlanLanePayload *payload) {
    memset(payload, 0, sizeof(*payload));
    memcpy(payload->harmony.roman_numeral, chord->roman, PLAN_ROMAN_NUMERAL_MAX);
    payload->harmony.function = chord->function;
}

/* Moves the target's cadence constraint to the final chord's span, where the cadence arrives. */
PlanResult harmony_move_cadence(PiecePlan *plan, LaneSpanId from, LaneSpanId to) {
    size_t count = plan_lane_span_constraint_count(plan, from);
    for (size_t i = 0; i < count; i++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, from, i);
        if (constraint->kind == PLAN_CONSTRAINT_CADENCE) {
            PlanConstraint cadence = *constraint;
            PlanResult result = plan_lane_span_remove_constraint(plan, from, i);
            return result == PLAN_OK ? plan_lane_span_add_constraint(plan, to, &cadence) : result;
        }
    }
    return PLAN_OK;
}

/*
 * The target span becomes the first chord (it keeps its id) and one span is
 * added after it for each chord that follows, so the chords tile the phrase.
 */
PlanResult harmony_progression_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate,
                                     ReadRequestFn read_request) {
    const PhraseProgression *progression = candidate ? candidate->payload : NULL;
    PhraseRequest request;
    if (!is_target(target) || !progression || progression->count == 0) {
        return PLAN_ERR_NULL;
    }
    LaneSpanId first = (LaneSpanId)target.id;
    if (!read_request(plan, first, &request) || request.cells != progression->request.cells) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    size_t total = 0;
    for (size_t i = 0; i < progression->count; i++) {
        total += progression->chords[i].cells;
    }
    if (total != request.cells) {
        return PLAN_ERR_BAD_PAYLOAD;
    }

    PlanSpan span;
    PlanLanePayload payload;
    PlanResult result = harmony_request_cells_span(&request, 0, progression->chords[0].cells, &span);
    if (result != PLAN_OK) {
        return result;
    }
    result = plan_lane_span_resize(plan, first, span.length);
    if (result == PLAN_OK) {
        harmony_chord_payload(&progression->chords[0], &payload);
        result = plan_lane_span_write(plan, first, &payload);
    }
    LaneSpanId last = first;
    size_t cell = progression->chords[0].cells;
    for (size_t i = 1; result == PLAN_OK && i < progression->count; i++) {
        result = harmony_request_cells_span(&request, cell, progression->chords[i].cells, &span);
        if (result != PLAN_OK) {
            break;
        }
        harmony_chord_payload(&progression->chords[i], &payload);
        result = plan_add_lane_span(plan, request.owner, PLAN_LANE_HARMONY, 0, span, &payload, &last);
        if (result == PLAN_OK) {
            result = plan_lane_span_set_state(plan, last, PLAN_STATE_REALIZED);
        }
        cell += progression->chords[i].cells;
    }
    return result == PLAN_OK ? harmony_move_cadence(plan, first, last) : result;
}

static PlanResult phrase_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    return harmony_progression_apply(plan, target, candidate,
                                     context ? harmony_phrase_read_request_given : harmony_phrase_read_request);
}

static FillResult register_phrase(FillOperatorRegistry *registry, bool around_given) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = HARMONY_PHRASE_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan harmony lane span";
    operator_.generator.supported_outputs = "PiecePlan harmony lane spans";
    operator_.generator.configuration_description = "none";
    operator_.generator.constraints_description = "standard-harmony phrase model and cadence rules";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = phrase_applies;
    operator_.generate = phrase_generate;
    operator_.apply = phrase_apply;
    operator_.context = around_given ? (void *)&AROUND_GIVEN : NULL;
    return fill_registry_register(registry, &operator_);
}

FillResult harmony_phrase_register(FillOperatorRegistry *registry) {
    return register_phrase(registry, false);
}

FillResult harmony_phrase_register_given(FillOperatorRegistry *registry) {
    return register_phrase(registry, true);
}
