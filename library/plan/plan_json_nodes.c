#include "plan/plan_json.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <common/json.h>
#include <model/pitch/pitch_arithmetic.h>
#include <theory/harmony/harmonic_function.h>
#include <theory/tonality/tonal_key.h>
#include "plan/plan_constraint.h"
#include "plan/plan_lanes.h"
#include "plan/plan_state.h"
#include "plan/section_json.h"
#include "plan/plan_constraint_kinds.h"
#include "plan/plan_internal.h"
#include "plan/plan_json_internal.h"
#include "plan/plan_common.h"

/* The nodes of a plan file: writing a node and its form record, reading the node tree. */

static cJSON *form_record_json(const PlanFormRecord *record) {
    cJSON *object = cJSON_CreateObject();
    cJSON *checks = cJSON_CreateArray();
    bool ok = object && checks;
    for (size_t i = 0; ok && i < COUNT_OF(plan_json_FORM_CHECK_NAMES); i++) {
        ok = (record->checks & (1u << i)) == 0 || plan_json_append(checks, cJSON_CreateString(plan_json_FORM_CHECK_NAMES[i]));
    }
    ok = ok && plan_json_add(object, "name", cJSON_CreateString(record->name)) &&
         plan_json_add(object, "version", json_number((long)record->version)) &&
         plan_json_add(object, "hash", cJSON_CreateString(record->hash)) && plan_json_add(object, "checks", checks);
    if (ok) {
        checks = NULL;
        ok = plan_json_add(object, "closing", cJSON_CreateBool(record->closing));
    }
    if (!ok) {
        cJSON_Delete(object);
        cJSON_Delete(checks);
        return NULL;
    }
    return object;
}

cJSON *plan_json_node_json(const Plan *plan, NodeId id) {
    cJSON *object = cJSON_CreateObject();
    cJSON *composer = cJSON_CreateObject();
    cJSON *children = cJSON_CreateArray();
    const char *name = plan_node_name(plan, id);
    bool ok = object && composer && children;
    ok = ok && plan_json_add(composer, "id", json_number((long)plan_node_rank(plan, id))) &&
         plan_json_add(composer, "state", cJSON_CreateString(plan_json_STATE_NAMES[plan_node_state(plan, id)])) &&
         plan_json_add(composer, "constraints", plan_json_node_constraints_json(plan, id));
    if (ok && plan_node_kind(plan, id) == PLAN_NODE_SECTION) {
        ok = plan_json_add(composer, "section", section_to_json(plan_node_section(plan, id), SECTION_USE_PLAN));
    }
    if (ok && id == plan_root(plan) && plan_form_record(plan)) {
        ok = plan_json_add(composer, "form", form_record_json(plan_form_record(plan)));
    }
    ok = ok && plan_json_add(object, "kind", cJSON_CreateString(plan_json_NODE_KINDS[plan_node_kind(plan, id)])) &&
         plan_json_add(object, "label", name ? cJSON_CreateString(name) : cJSON_CreateNull()) &&
         plan_json_add(object, "span", plan_json_span_json(plan_node_span(plan, id)));
    for (size_t i = 0; ok && i < plan_node_child_count(plan, id); i++) {
        ok = plan_json_append(children, plan_json_node_json(plan, plan_node_child_at(plan, id, i)));
    }
    ok = ok && plan_json_add(object, "children", children);
    if (ok) {
        children = NULL;
        ok = plan_json_add(object, "composer", composer);
        composer = NULL;
    }
    if (!ok) {
        cJSON_Delete(object);
        cJSON_Delete(composer);
        cJSON_Delete(children);
        return NULL;
    }
    return object;
}

static bool count_nodes(JsonReader *reader, const cJSON *node, const char *path, size_t *count) {
    if (!cJSON_IsObject(node)) {
        return json_bad_format(reader, path, "expected an object");
    }
    (*count)++;
    const cJSON *children = plan_json_member(reader, node, path, "children", true);
    if (!children) {
        return false;
    }
    char list_path[PLAN_JSON_PATH_MAX];
    json_child_path(list_path, path, "children");
    if (!cJSON_IsArray(children)) {
        return json_bad_format(reader, list_path, "expected an array");
    }
    size_t index = 0;
    const cJSON *child;
    cJSON_ArrayForEach(child, children) {
        char at[PLAN_JSON_PATH_MAX];
        json_index_path(at, list_path, index++);
        if (!count_nodes(reader, child, at, count)) {
            return false;
        }
    }
    return true;
}

/* Files each node under its composer.id so nodes can be added in id order. */
bool plan_json_collect_nodes(JsonReader *reader, const cJSON *node, const char *path, NodeId parent, ParsedNode *table,
                          size_t count) {
    const cJSON *composer = plan_json_member(reader, node, path, "composer", true);
    if (!composer) {
        return false;
    }
    char composer_path[PLAN_JSON_PATH_MAX];
    json_child_path(composer_path, path, "composer");
    if (!cJSON_IsObject(composer)) {
        return json_bad_format(reader, composer_path, "expected an object");
    }
    long id;
    if (!json_read_int_member(reader, composer, composer_path, "id", 1, (long)count, &id)) {
        return false;
    }
    if (table[id - 1].json) {
        char id_path[PLAN_JSON_PATH_MAX];
        json_child_path(id_path, composer_path, "id");
        return json_bad_format(reader, id_path, "id is used twice");
    }
    table[id - 1].json = node;
    table[id - 1].parent = parent;
    snprintf(table[id - 1].path, PLAN_JSON_PATH_MAX, "%s", path);

    const cJSON *children = cJSON_GetObjectItemCaseSensitive(node, "children");
    char list_path[PLAN_JSON_PATH_MAX];
    json_child_path(list_path, path, "children");
    size_t index = 0;
    const cJSON *child;
    cJSON_ArrayForEach(child, children) {
        char at[PLAN_JSON_PATH_MAX];
        json_index_path(at, list_path, index++);
        if (!plan_json_collect_nodes(reader, child, at, (NodeId)id, table, count)) {
            return false;
        }
    }
    return true;
}

bool plan_json_build_nodes(JsonReader *reader, Plan *plan, const cJSON *form) {
    size_t count = 0;
    if (!count_nodes(reader, form, "form", &count)) {
        return false;
    }
    ParsedNode *table = calloc(count, sizeof(ParsedNode));
    if (!table) {
        return json_fail(reader, PLAN_ERR_ALLOC, "form", "out of memory");
    }
    bool ok = plan_json_collect_nodes(reader, form, "form", PLAN_NODE_NONE, table, count);
    for (size_t i = 0; ok && i < count; i++) {
        const ParsedNode *parsed = &table[i];
        int kind;
        PlanSpan span;
        char at[PLAN_JSON_PATH_MAX];
        ok = json_read_enum_member(reader, parsed->json, parsed->path, "kind", plan_json_NODE_KINDS, COUNT_OF(plan_json_NODE_KINDS), &kind) &&
             json_read_span(reader, parsed->json, parsed->path, &span);
        const cJSON *label = ok ? plan_json_member(reader, parsed->json, parsed->path, "label", false) : NULL;
        if (ok && label && !cJSON_IsNull(label) && !cJSON_IsString(label)) {
            json_child_path(at, parsed->path, "label");
            ok = json_bad_format(reader, at, "expected a string or null");
        }
        if (ok) {
            NodeId id;
            const char *name = cJSON_IsString(label) ? label->valuestring : NULL;
            PlanResult result = plan_add_node(plan, parsed->parent, (PlanNodeKind)kind, span, name, &id);
            if (result == PLAN_ERR_ALLOC) {
                ok = json_fail(reader, PLAN_ERR_ALLOC, parsed->path, "out of memory");
            } else if (result != PLAN_OK) {
                ok = json_fail(reader, PLAN_ERR_FORMAT, parsed->path, "node %s", plan_json_result_name(result));
            }
        }
    }
    free(table);
    return ok;
}
