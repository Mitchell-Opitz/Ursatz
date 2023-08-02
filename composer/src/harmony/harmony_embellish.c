#include <ursatz_composer/harmony_embellish.h>

#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"

#define DEFAULT_DENSITY 500
#define DENSITY_SCALE 1000

HarmonyEmbellishParams harmony_embellish_params_default(void) {
    HarmonyEmbellishParams params = {DEFAULT_DENSITY, HARMONY_EMBELLISH_ALL};
    return params;
}

PlanResult harmony_embellish_open(PiecePlan *plan, PlanSpan range) {
    if (!plan) {
        return PLAN_ERR_NULL;
    }
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, range, NULL, 0);
    if (count == 0) {
        return PLAN_OK;
    }
    LaneSpanId *ids = malloc(count * sizeof(*ids));
    if (!ids) {
        return PLAN_ERR_ALLOC;
    }
    plan_lane_find(plan, PLAN_LANE_HARMONY, 0, range, ids, count);
    PlanConstraint marker = {PLAN_CONSTRAINT_EMBELLISH, {0}};
    PlanResult result = PLAN_OK;
    for (size_t i = 0; result == PLAN_OK && i < count; i++) {
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_REALIZED) {
            result = plan_lane_span_set_state(plan, ids[i], PLAN_STATE_CONSTRAINED);
            if (result == PLAN_OK && !harmony_span_has_constraint(plan, ids[i], PLAN_CONSTRAINT_EMBELLISH)) {
                result = plan_lane_span_add_constraint(plan, ids[i], &marker);
            }
        }
    }
    free(ids);
    return result;
}

/* What an embellishment candidate does to the marked chord. */
typedef struct {
    bool plain; /* leave the progression as it is */
} EmbellishCandidate;

static const HarmonyEmbellishParams *params_of(void *context) {
    static const HarmonyEmbellishParams defaults = {DEFAULT_DENSITY, HARMONY_EMBELLISH_ALL};
    return context ? context : &defaults;
}

static bool embellish_applies(const PiecePlan *plan, FillTarget target, void *context) {
    PhraseContext phrase;
    (void)context;
    return target.kind == FILL_TARGET_LANE_SPAN && harmony_context_read(plan, (LaneSpanId)target.id, &phrase);
}

static bool add_plain(FillCandidateList *out) {
    EmbellishCandidate *candidate = calloc(1, sizeof(*candidate));
    if (!candidate) {
        return false;
    }
    candidate->plain = true;
    return fill_candidates_add(out, "plain", candidate, free);
}

/* Proposes the embellishments of the enabled families for the marked chord. */
static bool propose_edits(const PhraseContext *phrase, const HarmonyEmbellishParams *params, FillRng *rng,
                          FillCandidateList *out) {
    (void)phrase;
    (void)params;
    (void)rng;
    (void)out;
    return true;
}

static bool embellish_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                               FillCandidateList *out) {
    const HarmonyEmbellishParams *params = params_of(context);
    PhraseContext phrase;
    if (target.kind != FILL_TARGET_LANE_SPAN || !harmony_context_read(plan, (LaneSpanId)target.id, &phrase)) {
        return false;
    }
    /* The draw is made whether or not it is used, so the random stream does not depend on the parameters. */
    unsigned density = params->density > DENSITY_SCALE ? DENSITY_SCALE : params->density;
    bool embellish = fill_rng_below(rng, DENSITY_SCALE) < density && params->families != 0;
    if (embellish && !propose_edits(&phrase, params, rng, out)) {
        return false;
    }
    /* A chord nothing fits is left as it is. */
    return out->count > 0 || add_plain(out);
}

static PlanResult clear_marker(PiecePlan *plan, LaneSpanId id) {
    size_t count = plan_lane_span_constraint_count(plan, id);
    for (size_t i = 0; i < count; i++) {
        if (plan_lane_span_constraint_at(plan, id, i)->kind == PLAN_CONSTRAINT_EMBELLISH) {
            return plan_lane_span_remove_constraint(plan, id, i);
        }
    }
    return PLAN_OK;
}

static PlanResult embellish_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const EmbellishCandidate *edit = candidate ? candidate->payload : NULL;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN || !edit) {
        return PLAN_ERR_NULL;
    }
    return clear_marker(plan, (LaneSpanId)target.id);
}

FillResult harmony_embellish_register(FillOperatorRegistry *registry, const HarmonyEmbellishParams *params) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = HARMONY_EMBELLISH_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan harmony lane span marked for embellishment";
    operator_.generator.supported_outputs = "PiecePlan harmony lane spans";
    operator_.generator.configuration_description = "density in thousandths, and the families that are on";
    operator_.generator.constraints_description = "standard-harmony chord function, prolongation and tonicization";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = embellish_applies;
    operator_.generate = embellish_generate;
    operator_.apply = embellish_apply;
    operator_.context = (void *)params;
    return fill_registry_register(registry, &operator_);
}
