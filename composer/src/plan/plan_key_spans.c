#include <ursatz_composer/plan_json.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <model/pitch/region.h>
#include <theory/knowledge/relation.h>
#include <theory/tonality/common_practice_materials.h>
#include <theory/tonality/region_relation.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>

#include "plan/plan_internal.h"
#include "plan/plan_json_internal.h"
#include "util/plan_util.h"
#include "util/util.h"

/*
 * Key spans named by a relation (R-597). The spans are resolved in the order the key lane runs, so a span anchored on the one before
 * it finds that one resolved. The keys are computed first and written only when every span resolved, so a refusal leaves the plan
 * as it was.
 */

/* The index of key span `id` among the key spans in id order, which is its place in the "keys" array of the plan file. */
static size_t key_array_index(const PiecePlan *plan, LaneSpanId id) {
    size_t index = 0;
    for (LaneSpanId other = 1; other < id; other++) {
        if (plan_has_lane_span(plan, other) && plan_lane_span_lane(plan, other) == PLAN_LANE_KEY) {
            index++;
        }
    }
    return index;
}

static PlanResult refuse(PlanJsonError *error, const PiecePlan *plan, LaneSpanId id, const char *member, const char *format, ...) {
    char message[PLAN_JSON_MESSAGE_MAX];
    char path[PLAN_JSON_PATH_MAX];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    snprintf(path, sizeof(path), "keys[%zu].composer.payload.%s", key_array_index(plan, id), member);
    json_set_error(error, -1, path, "%s", message);
    return PLAN_ERR_FORMAT;
}

/* The region of a key: its tonic's pitch class and its material (the major, for a key with none). */
static Region key_region(const SpelledKey *key, const CommonPracticeMaterials *materials) {
    return region_tonal(spelled_key_tonic_pitch_class(key), key->material ? key->material : materials->major);
}

PlanResult plan_resolve_key_spans(PiecePlan *plan, const KnowledgeSnapshot *knowledge, PlanJsonError *error) {
    CommonPracticeMaterials materials;
    if (!plan || !common_practice_materials_resolve(knowledge, &materials, NULL, 0)) {
        json_set_error(error, -1, "", "a plan and a knowledge snapshot are required");
        return PLAN_ERR_NULL;
    }
    PlanSpan everything = {{0, 1}, {1000000000, 1}};
    size_t count = plan_lane_find(plan, PLAN_LANE_KEY, 0, everything, NULL, 0);
    size_t unresolved = 0;
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        const PlanKeyRelation *named = plan_has_lane_span(plan, id) ? plan_lane_span_key_relation(plan, id) : NULL;
        unresolved += named && !named->resolved ? 1 : 0;
    }
    if (unresolved == 0) {
        return PLAN_OK;
    }
    LaneSpanId *ids = calloc(count, sizeof(LaneSpanId));
    SpelledKey *keys = calloc(count, sizeof(SpelledKey));
    if (!ids || !keys) {
        free(ids);
        free(keys);
        json_set_error(error, -1, "keys", "out of memory");
        return PLAN_ERR_ALLOC;
    }
    plan_lane_find(plan, PLAN_LANE_KEY, 0, everything, ids, count);

    PlanResult result = PLAN_OK;
    for (size_t i = 0; result == PLAN_OK && i < count; i++) {
        keys[i] = plan_lane_span_payload(plan, ids[i])->key;
        const PlanKeyRelation *named = plan_lane_span_key_relation(plan, ids[i]);
        if (!named || named->resolved) {
            continue;
        }
        if (i == 0) {
            result = refuse(error, plan, ids[i], "anchor", "the first key span has no %s key to take '%s' from",
                            named->anchor == PLAN_KEY_ANCHOR_HOME ? "home" : named->anchor == PLAN_KEY_ANCHOR_PREVIOUS ? "previous" : "section",
                            named->relation);
            continue;
        }
        size_t anchor_at = named->anchor == PLAN_KEY_ANCHOR_HOME ? 0 : i - 1;
        if (named->anchor == PLAN_KEY_ANCHOR_SECTION) {
            /* the key span at the start of the named section, which must come before this span */
            NodeId section = plan_section_named(plan, named->anchor_section);
            size_t found = count;
            for (size_t k = 0; section != PLAN_NODE_NONE && k < i; k++) {
                PlanSpan span = plan_lane_span_span(plan, ids[k]);
                PlanSpan at = plan_node_span(plan, section);
                Rational end;
                if (rational_cmp(span.start, at.start) <= 0 && plan_span_end(span, &end) == PLAN_OK && rational_cmp(at.start, end) < 0) {
                    found = k;
                }
            }
            if (found == count) {
                result = refuse(error, plan, ids[i], "anchor", "'%s' is not an earlier section to take '%s' from", named->anchor_section,
                                named->relation);
                continue;
            }
            anchor_at = found;
        }
        const SpelledKey *anchor_key = &keys[anchor_at];
        const Relation *relation = knowledge_snapshot_relation(knowledge, named->relation);
        if (!relation) {
            result = refuse(error, plan, ids[i], "relation", "no relation has the id '%s'", named->relation);
            continue;
        }
        Region anchor = key_region(anchor_key, &materials);
        Region found;
        char message[PLAN_JSON_MESSAGE_MAX];
        if (region_resolve(knowledge, relation, &anchor, &found, message, sizeof(message)) != REGION_RELATION_OK) {
            result = refuse(error, plan, ids[i], "relation", "%s", message);
            continue;
        }
        if (found.kind != REGION_TONAL_MODAL || !composer_material_supported(&materials, found.material)) {
            result = refuse(error, plan, ids[i], "relation", "relation '%s' reaches %s, which is not a key the Composer composes in (major or natural minor)",
                            named->relation, found.material && found.material->name ? found.material->name : "no material");
            continue;
        }
        keys[i] = plan_key_of_pitch_class(found.center, found.material);
    }
    for (size_t i = 0; result == PLAN_OK && i < count; i++) {
        const PlanKeyRelation *named = plan_lane_span_key_relation(plan, ids[i]);
        if (named && !named->resolved) {
            LaneSpan *entry = plan_lane_span_at(plan, ids[i]);
            entry->payload.key = keys[i];
            entry->key_relation.resolved = true;
        }
    }
    free(ids);
    free(keys);
    return result;
}
