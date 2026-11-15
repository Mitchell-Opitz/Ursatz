#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_analyzer/motive_links.h>
#include "util/json_util.h"

static const char *const dimension_keys[MOTIVE_DIMENSION_COUNT] = {"rhythm", "intervals", "contour", "harmony"};

/* Hands *item to object under name and clears it; on a failure *item stays with the caller, who frees it. */
static bool put(cJSON *object, const char *name, cJSON **item) {
    if (!*item || !cJSON_AddItemToObject(object, name, *item)) {
        return false;
    }
    *item = NULL;
    return true;
}

static cJSON *end_json(const MotiveLinkEnd *end) {
    cJSON *object = cJSON_CreateObject();
    cJSON *measures = cJSON_CreateObject();
    bool ok = object && measures && json_add(measures, "first", cJSON_CreateNumber(end->first_measure)) &&
              json_add(measures, "last", cJSON_CreateNumber(end->last_measure)) &&
              json_add(object, "piece", cJSON_CreateString(end->piece)) &&
              json_add(object, "family", cJSON_CreateNumber((double)end->family)) && put(object, "measures", &measures) &&
              json_add(object, "beat", cJSON_CreateNumber(end->beat)) &&
              json_add(object, "degrees", cJSON_CreateString(end->degrees));
    cJSON_Delete(measures);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *link_json(const MotiveLinkFound *link) {
    cJSON *object = cJSON_CreateObject();
    cJSON *steps = cJSON_CreateArray();
    cJSON *distance = cJSON_CreateObject();
    cJSON *similarity = cJSON_CreateObject();
    char fraction[64];
    snprintf(fraction, sizeof fraction, "%llu/%llu", link->distance_numerator,
             link->distance_denominator ? link->distance_denominator : 1ULL);
    double value = link->distance_denominator ? (double)link->distance_numerator / (double)link->distance_denominator : 1.0;
    bool ok = object && steps && distance && similarity;
    for (size_t i = 0; ok && i < link->step_count; i++) {
        ok = json_append(steps, cJSON_CreateString(link->step_text[i]));
    }
    for (int d = 0; ok && d < MOTIVE_DIMENSION_COUNT; d++) {
        ok = json_add(similarity, dimension_keys[d], cJSON_CreateNumber(link->similarity[d]));
    }
    ok = ok && json_add(distance, "fraction", cJSON_CreateString(fraction)) &&
         json_add(distance, "value", cJSON_CreateNumber(value)) && json_add(object, "a", end_json(&link->a)) &&
         json_add(object, "b", end_json(&link->b)) && json_add(object, "reversed", cJSON_CreateBool(link->reversed)) &&
         put(object, "steps", &steps) && put(object, "distance", &distance) && put(object, "similarity", &similarity);
    cJSON_Delete(steps);
    cJSON_Delete(distance);
    cJSON_Delete(similarity);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *group_json(const MotiveLinkGroup *group) {
    cJSON *object = cJSON_CreateObject();
    cJSON *members = cJSON_CreateArray();
    bool ok = object && members;
    for (size_t i = 0; ok && i < group->member_count; i++) {
        cJSON *member = cJSON_CreateObject();
        bool built = member && json_add(member, "piece", cJSON_CreateString(group->members[i].piece)) &&
                     json_add(member, "family", cJSON_CreateNumber((double)group->members[i].family));
        if (built) {
            ok = json_append(members, member);
        } else {
            cJSON_Delete(member);
            ok = false;
        }
    }
    ok = ok && put(object, "members", &members);
    cJSON_Delete(members);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

MotiveLinksResult motive_links_render(const MotiveLinks *links, char **out) {
    if (!links || !out) {
        return MOTIVE_LINKS_ERR_ARGUMENT;
    }
    *out = NULL;
    cJSON *root = cJSON_CreateObject();
    cJSON *list = cJSON_CreateArray();
    cJSON *groups = cJSON_CreateArray();
    bool ok = root && list && groups && json_add(root, "schema", cJSON_CreateString("ursatz-motive-links")) &&
              json_add(root, "schema_version", cJSON_CreateNumber(1)) &&
              json_add(root, "pieces", cJSON_CreateNumber((double)links->pieces)) &&
              json_add(root, "pairs", cJSON_CreateNumber((double)links->pairs)) &&
              json_add(root, "compared", cJSON_CreateNumber((double)links->compared)) &&
              json_add(root, "cached", cJSON_CreateNumber((double)links->cached)) &&
              json_add(root, "cache_reset", cJSON_CreateBool(links->cache_reset));
    for (size_t i = 0; ok && i < links->link_count; i++) {
        ok = json_append(list, link_json(&links->links[i]));
    }
    for (size_t i = 0; ok && i < links->group_count; i++) {
        ok = json_append(groups, group_json(&links->groups[i]));
    }
    ok = ok && put(root, "links", &list) && put(root, "groups", &groups);
    cJSON_Delete(list);
    cJSON_Delete(groups);
    char *text = ok ? cJSON_Print(root) : NULL;
    cJSON_Delete(root);
    if (!text) {
        return MOTIVE_LINKS_ERR_ALLOC;
    }
    size_t length = strlen(text);
    char *result = malloc(length + 2);
    if (!result) {
        cJSON_free(text);
        return MOTIVE_LINKS_ERR_ALLOC;
    }
    memcpy(result, text, length);
    result[length] = '\n';
    result[length + 1] = '\0';
    cJSON_free(text);
    *out = result;
    return MOTIVE_LINKS_OK;
}
