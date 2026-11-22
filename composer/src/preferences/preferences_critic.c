#include "preferences/preferences_internal.h"

#include <stdio.h>
#include <string.h>

/* The critic group of the preferences document (R-722): ranking (rank, candidates, keep_rejected), reverse and the weight of each evaluator. */

static bool read_weights(const cJSON *object, CriticSettings *critic, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(object)) {
        return preferences_refuse_in(refusal, "critic", "weights", "must be an object of whole numbers 0 to 20");
    }
    cJSON_ArrayForEach(item, object) {
        CriticEvaluator evaluator;
        char key[COMPOSER_PREFERENCES_PATH_MAX];
        snprintf(key, sizeof(key), "weights/%s", item->string);
        if (!critic_evaluator_from_code(item->string, &evaluator)) {
            return preferences_refuse_in(refusal, "critic", key, "unknown name");
        }
        unsigned weight = 0;
        if (!preferences_read_unsigned_in(item, "critic", key, 0, WEIGHT_MAX, &weight, refusal)) {
            return false;
        }
        critic->weights[evaluator] = (int)weight;
    }
    return true;
}

/* A list of evaluator codes, none twice: the evaluators whose penalty counts for a candidate (reversed). */
static bool read_reverse(const cJSON *list, CriticSettings *critic, const Refusal *refusal) {
    bool reversed[CRITIC_EVALUATOR_COUNT] = {false};
    const cJSON *item;
    int index = 0;
    if (!cJSON_IsArray(list)) {
        return preferences_refuse_in(refusal, "critic", "reverse", "must be a list of evaluator names");
    }
    cJSON_ArrayForEach(item, list) {
        CriticEvaluator evaluator;
        char key[COMPOSER_PREFERENCES_PATH_MAX];
        snprintf(key, sizeof(key), "reverse/%d", index++);
        if (!cJSON_IsString(item) || !critic_evaluator_from_code(item->valuestring, &evaluator)) {
            return preferences_refuse_in(refusal, "critic", key, "is not an evaluator of the critic");
        }
        if (reversed[evaluator]) {
            return preferences_refuse_in(refusal, "critic", key, "repeats a name of the list");
        }
        reversed[evaluator] = true;
    }
    memcpy(critic->reversed, reversed, sizeof(reversed));
    return true;
}

bool preferences_read_critic(const cJSON *group, ComposerPreferences *preferences, const Refusal *refusal) {
    CriticSettings *critic = &preferences->critic;
    const cJSON *item;
    if (!cJSON_IsObject(group)) {
        return preferences_refuse(refusal, "critic", "must be an object");
    }
    cJSON_ArrayForEach(item, group) {
        unsigned value = 0;
        if (strcmp(item->string, "rank") == 0) {
            if (!cJSON_IsBool(item)) {
                return preferences_refuse_in(refusal, "critic", "rank", "must be true or false");
            }
            critic->rank = cJSON_IsTrue(item);
        } else if (strcmp(item->string, "candidates") == 0) {
            if (!preferences_read_unsigned_in(item, "critic", "candidates", COMPOSER_PREFERENCES_POOL_MIN, COMPOSER_PREFERENCES_POOL_MAX, &value,
                                              refusal)) {
                return false;
            }
            critic->candidates = (int)value;
        } else if (strcmp(item->string, "keep_rejected") == 0) {
            if (!preferences_read_unsigned_in(item, "critic", "keep_rejected", 0, COMPOSER_PREFERENCES_KEEP_REJECTED_MAX, &value, refusal)) {
                return false;
            }
            critic->keep_rejected = (int)value;
        } else if (strcmp(item->string, "reverse") == 0) {
            if (!read_reverse(item, critic, refusal)) {
                return false;
            }
        } else if (strcmp(item->string, "weights") == 0) {
            if (!read_weights(item, critic, refusal)) {
                return false;
            }
        } else {
            return preferences_refuse_in(refusal, "critic", item->string, "unknown member");
        }
    }
    return true;
}

cJSON *preferences_write_critic(const ComposerPreferences *preferences) {
    const CriticSettings *critic = &preferences->critic;
    cJSON *group = cJSON_CreateObject();
    bool ok = group != NULL && cJSON_AddBoolToObject(group, "rank", critic->rank) != NULL &&
              preferences_add_number(group, "candidates", critic->candidates) && preferences_add_number(group, "keep_rejected", critic->keep_rejected);
    cJSON *reverse = ok ? cJSON_AddArrayToObject(group, "reverse") : NULL;
    cJSON *weights = reverse ? cJSON_AddObjectToObject(group, "weights") : NULL;
    ok = ok && reverse != NULL && weights != NULL;
    for (int i = 0; ok && i < CRITIC_EVALUATOR_COUNT; i++) {
        ok = preferences_add_number(weights, critic_evaluator_code((CriticEvaluator)i), critic->weights[i]);
        if (ok && critic->reversed[i]) {
            cJSON *name = cJSON_CreateString(critic_evaluator_code((CriticEvaluator)i));
            if (name == NULL || !cJSON_AddItemToArray(reverse, name)) {
                cJSON_Delete(name);
                ok = false;
            }
        }
    }
    if (!ok) {
        cJSON_Delete(group);
        return NULL;
    }
    return group;
}
