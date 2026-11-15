#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_state.h>

#include "api_internal.h"
#include "api_workflow.h"
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

/* --- the node a request works on ------------------------------------------------------------------- */

ComposerStatus api_read_target(const cJSON *request, ApiTarget *out, ComposerError *error) {
    memset(out, 0, sizeof(*out));
    const cJSON *node = cJSON_GetObjectItemCaseSensitive(request, "node");
    const cJSON *section = cJSON_GetObjectItemCaseSensitive(request, "section");
    bool has_node = node && !cJSON_IsNull(node);
    bool has_section = section && !cJSON_IsNull(section);
    if (has_node == has_section) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "", "the request needs a node: give \"node\" (an id) or \"section\" (a letter), not both");
    }
    if (has_node) {
        bool present = false;
        if (!api_member_whole(request, "node", 0, 1000000UL, &out->node, &present, error)) {
            return COMPOSER_ERR_REQUEST;
        }
        out->has_node = true;
    } else if (!cJSON_IsString(section) || !section->valuestring[0]) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/section", "section must be a section's letter");
    } else {
        out->section = section->valuestring;
    }
    const char *lane = NULL;
    bool present = false;
    if (!api_member_string(request, "lane", &lane, &present, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (lane) {
        out->has_lane = true;
        if (!api_lane_from_name(lane, &out->lane)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/lane", "lane must be harmony, bass, melody or inner, not '%s'", lane);
        }
    }
    return COMPOSER_OK;
}

static bool target_node(const PiecePlan *plan, const ApiTarget *target, NodeId *out) {
    if (target->has_node) {
        if (!plan_has_node(plan, (NodeId)target->node)) {
            return false;
        }
        *out = (NodeId)target->node;
        return true;
    }
    ApiSection sections[API_MAX_SECTIONS];
    size_t count = api_plan_sections(plan, sections, API_MAX_SECTIONS);
    count = count > API_MAX_SECTIONS ? API_MAX_SECTIONS : count;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(sections[i].letter, target->section) == 0 ||
            (target->section[1] == '\0' && request_equals_ignore_case(sections[i].letter, target->section))) {
            *out = sections[i].node;
            return true;
        }
    }
    return false;
}

ComposerStatus api_target_node(const PiecePlan *plan, const ApiTarget *target, NodeId *out, ComposerError *error) {
    if (target_node(plan, target, out)) {
        return COMPOSER_OK;
    }
    if (target->has_node) {
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/node", "the plan has no node %lu (ids run from 1 to %zu)", target->node,
                        plan_node_count(plan));
    }
    return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/section", "the plan has no section '%s'", target->section);
}

ComposerStatus api_target_list(const PiecePlan *plan, const ApiTarget *target, FillTarget **out, size_t *count,
                               ComposerError *error) {
    NodeId node;
    *out = NULL;
    *count = 0;
    ComposerStatus status = api_target_node(plan, target, &node, error);
    return status == COMPOSER_OK ? api_lane_targets(plan, node, target->has_lane, target->lane, out, count, error) : status;
}

void api_target_describe(const PiecePlan *plan, const ApiTarget *target, char *out, size_t size) {
    NodeId node;
    char what[64];
    if (!target_node(plan, target, &node)) {
        snprintf(out, size, "the node");
        return;
    }
    ApiSection sections[API_MAX_SECTIONS];
    size_t count = api_plan_sections(plan, sections, API_MAX_SECTIONS);
    count = count > API_MAX_SECTIONS ? API_MAX_SECTIONS : count;
    snprintf(what, sizeof(what), "node %zu", (size_t)node);
    for (size_t i = 0; i < count; i++) {
        if (sections[i].node == node) {
            snprintf(what, sizeof(what), "section %s", sections[i].letter);
        }
    }
    if (target->has_lane) {
        snprintf(out, size, "the %s of %s", api_lane_label(target->lane), what);
    } else {
        snprintf(out, size, "%s", what);
    }
}

/* --- lock ------------------------------------------------------------------------------------------ */

static size_t count_given(const PiecePlan *plan, size_t *nodes) {
    size_t spans = 0;
    *nodes = 0;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        *nodes += plan_has_node(plan, id) && plan_node_state(plan, id) == PLAN_STATE_GIVEN;
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        spans += plan_has_lane_span(plan, id) && plan_lane_span_state(plan, id) == PLAN_STATE_GIVEN;
    }
    return spans;
}

ComposerStatus composer_api_lock(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    (void)job;
    memset(out, 0, sizeof *out);
    cJSON *root = NULL;
    char *text = NULL;
    PiecePlan *plan = NULL;
    FillTarget *targets = NULL;
    size_t target_count = 0;
    ApiTarget target;
    ComposerStatus status = api_parse_request(request_json, &root, &out->error);
    if (status == COMPOSER_OK) {
        status = api_read_target(root, &target, &out->error);
    }
    if (status == COMPOSER_OK) {
        status = api_read_plan_text(root, "plan", &text, &out->error);
    }
    if (status == COMPOSER_OK) {
        status = api_plan_from_text(text, "plan", &plan, &out->error);
    }
    if (status == COMPOSER_OK) {
        status = api_target_list(plan, &target, &targets, &target_count, &out->error);
    }
    char what[96] = "";
    size_t nodes_before = 0;
    size_t spans_before = 0;
    if (status == COMPOSER_OK) {
        api_target_describe(plan, &target, what, sizeof what);
        spans_before = count_given(plan, &nodes_before);
    }
    for (size_t t = 0; status == COMPOSER_OK && t < target_count; t++) {
        PlanResult locked = plan_lock(plan, targets[t]);
        if (locked == PLAN_ERR_BAD_PAYLOAD) {
            status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "cannot lock %s: some of it is not written yet (run fill first)", what);
        } else if (locked != PLAN_OK) {
            status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "cannot lock %s", what);
        }
    }
    if (status == COMPOSER_OK) {
        size_t nodes_after;
        size_t spans_after = count_given(plan, &nodes_after);
        cJSON *document = cJSON_CreateObject();
        cJSON *locked = document ? cJSON_AddObjectToObject(document, "locked") : NULL;
        ApiOutputs outputs;
        memset(&outputs, 0, sizeof outputs);
        if (!locked || !cJSON_AddNumberToObject(locked, "nodes", (double)(nodes_after - nodes_before)) ||
            !cJSON_AddNumberToObject(locked, "spans", (double)(spans_after - spans_before)) ||
            !cJSON_AddStringToObject(document, "what", what)) {
            cJSON_Delete(document);
            status = api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
        } else {
            status = api_render_outputs(plan, &outputs, out);
            if (status == COMPOSER_OK) {
                status = api_respond_json(out, document);
            } else {
                cJSON_Delete(document);
            }
        }
    }
    free(targets);
    free(text);
    plan_destroy(plan);
    cJSON_Delete(root);
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}
