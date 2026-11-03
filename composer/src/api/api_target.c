#include <stdlib.h>
#include <string.h>

#include "api_internal.h"
#include "util/plan_util.h"
#include "util/request_parse.h"

/* The lanes a request or a command names, and the spans of a node a regenerate may write again. */

static const char *const LANE_LABELS[] = {"meter", "key", "harmony", "bass", "melody", "inner voice"};

bool api_lane_from_name(const char *name, PlanLaneKind *out) {
    if (request_equals_ignore_case(name, "harmony")) {
        *out = PLAN_LANE_HARMONY;
    } else if (request_equals_ignore_case(name, "bass")) {
        *out = PLAN_LANE_BASS;
    } else if (request_equals_ignore_case(name, "melody")) {
        *out = PLAN_LANE_MELODY;
    } else if (request_equals_ignore_case(name, "inner")) {
        *out = PLAN_LANE_INNER_VOICE;
    } else {
        return false;
    }
    return true;
}

const char *api_lane_label(PlanLaneKind lane) {
    return LANE_LABELS[lane];
}

ComposerStatus api_lane_targets(const PiecePlan *plan, NodeId node, bool has_lane, PlanLaneKind lane, FillTarget **out,
                                size_t *count, ComposerError *error) {
    *out = calloc(plan_lane_span_count(plan) + 1, sizeof(FillTarget));
    *count = 0;
    if (!*out) {
        return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    if (!has_lane) {
        (*out)[(*count)++] = (FillTarget){FILL_TARGET_NODE, node};
        return COMPOSER_OK;
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (plan_has_lane_span(plan, id) && plan_lane_span_lane(plan, id) == lane &&
            plan_in_subtree(plan, plan_lane_span_owner(plan, id), node)) {
            (*out)[(*count)++] = (FillTarget){FILL_TARGET_LANE_SPAN, id};
        }
    }
    if (*count == 0) {
        free(*out);
        *out = NULL;
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "node %zu has no %s spans", (size_t)node, api_lane_label(lane));
    }
    return COMPOSER_OK;
}

ComposerStatus api_refused(RegenerateResult result, const char *what, ComposerError *error) {
    switch (result) {
    case REGENERATE_ERR_LOCKED:
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "",
                        "%s is Given (locked) or has Given material among its phrases; locking is for good in a plan, to unlock go "
                        "back to an earlier plan file",
                        what);
    case REGENERATE_ERR_BAD_TARGET:
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "",
                        "%s cannot be regenerated: a motive, the material, the meter and key, and spans the Composer did not "
                        "open have nothing to write again",
                        what);
    case REGENERATE_ERR_NOT_FOUND:
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "%s is not in the plan", what);
    default:
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "cannot regenerate %s: %s", what, regenerate_result_name(result));
    }
}
