#include "preferences/preferences_internal.h"

#include <stdio.h>
#include <string.h>

/* The harmony group of the preferences document (P69): mixture (R-729), the share of borrowed predominant chords and the relations they come from. */

#define SHARE_DENOMINATOR_MAX 16

static bool read_from(const cJSON *list, ComposerMixture *mixture, const Refusal *refusal) {
    const cJSON *item;
    ComposerMixture read = *mixture;
    read.from_count = 0;
    if (!cJSON_IsArray(list) || cJSON_GetArraySize(list) < 1 || cJSON_GetArraySize(list) > COMPOSER_MIXTURE_FROM_MAX) {
        return preferences_refuse_in(refusal, "harmony", "mixture/from", "must be a list of 1 to 6 relation words");
    }
    cJSON_ArrayForEach(item, list) {
        char key[COMPOSER_PREFERENCES_PATH_MAX];
        snprintf(key, sizeof(key), "mixture/from/%zu", read.from_count);
        if (!cJSON_IsString(item) || item->valuestring[0] == '\0' || strlen(item->valuestring) >= COMPOSER_MIXTURE_WORD_MAX) {
            return preferences_refuse_in(refusal, "harmony", key, "must be a relation word of 1 to 31 characters");
        }
        for (size_t i = 0; i < read.from_count; i++) {
            if (strcmp(read.from[i], item->valuestring) == 0) {
                return preferences_refuse_in(refusal, "harmony", key, "repeats a word of the list");
            }
        }
        snprintf(read.from[read.from_count++], COMPOSER_MIXTURE_WORD_MAX, "%s", item->valuestring);
    }
    *mixture = read;
    return true;
}

static bool read_mixture(const cJSON *group, ComposerMixture *mixture, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(group)) {
        return preferences_refuse_in(refusal, "harmony", "mixture", "must be an object");
    }
    cJSON_ArrayForEach(item, group) {
        if (strcmp(item->string, "share") == 0) {
            if (!cJSON_IsString(item) || !preferences_parse_ratio(item->valuestring, SHARE_DENOMINATOR_MAX, &mixture->share)) {
                return preferences_refuse_in(refusal, "harmony", "mixture/share", "must be a ratio n/d with 0 <= n <= d and 1 <= d <= 16");
            }
        } else if (strcmp(item->string, "from") == 0) {
            if (!read_from(item, mixture, refusal)) {
                return false;
            }
        } else {
            char key[COMPOSER_PREFERENCES_PATH_MAX];
            snprintf(key, sizeof(key), "mixture/%s", item->string);
            return preferences_refuse_in(refusal, "harmony", key, "unknown member");
        }
    }
    return true;
}

bool preferences_read_harmony(const cJSON *group, ComposerPreferences *preferences, const Refusal *refusal) {
    const cJSON *item;
    if (!cJSON_IsObject(group)) {
        return preferences_refuse(refusal, "harmony", "must be an object");
    }
    cJSON_ArrayForEach(item, group) {
        if (strcmp(item->string, "mixture") == 0) {
            if (!read_mixture(item, &preferences->harmony.mixture, refusal)) {
                return false;
            }
        } else {
            return preferences_refuse_in(refusal, "harmony", item->string, "unknown member");
        }
    }
    return true;
}

cJSON *preferences_write_harmony(const ComposerPreferences *preferences) {
    const ComposerMixture *mixture = &preferences->harmony.mixture;
    cJSON *group = cJSON_CreateObject();
    cJSON *object = group ? cJSON_AddObjectToObject(group, "mixture") : NULL;
    cJSON *from = object ? cJSON_CreateArray() : NULL;
    char share[32];
    snprintf(share, sizeof(share), "%lld/%lld", mixture->share.num, mixture->share.den);
    bool ok = from != NULL && cJSON_AddStringToObject(object, "share", share) != NULL;
    for (size_t i = 0; ok && i < mixture->from_count; i++) {
        cJSON *word = cJSON_CreateString(mixture->from[i]);
        if (word == NULL || !cJSON_AddItemToArray(from, word)) {
            cJSON_Delete(word);
            ok = false;
        }
    }
    if (!ok || !cJSON_AddItemToObject(object, "from", from)) {
        cJSON_Delete(from);
        cJSON_Delete(group);
        return NULL;
    }
    return group;
}
