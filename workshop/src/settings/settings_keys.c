#include "settings/settings_keys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WorkshopSettingKey *g_keys = NULL;
static size_t g_key_count = 0;
static WorkshopSettingRule *g_rules = NULL;
static size_t g_rule_count = 0;

void workshop_settings_keys_clear(void) {
    for (size_t i = 0; i < g_key_count; i++) {
        cJSON_Delete(g_keys[i].words);
        cJSON_Delete(g_keys[i].dflt);
    }
    free(g_keys);
    free(g_rules);
    g_keys = NULL;
    g_rules = NULL;
    g_key_count = 0;
    g_rule_count = 0;
}

static void copy_text(char *out, size_t size, const cJSON *item) {
    snprintf(out, size, "%s", cJSON_IsString(item) ? item->valuestring : "");
}

/* One key of the workshop's own: the explore group (R-673), soft, never part of the document the Composer reads. */
static void add_workshop_key(WorkshopSettingKey *row, const char *key, const char *type, int min, int max, cJSON *dflt) {
    memset(row, 0, sizeof(*row));
    snprintf(row->key, sizeof(row->key), "%s", key);
    snprintf(row->type, sizeof(row->type), "%s", type);
    row->min = min;
    row->max = max;
    row->dflt = dflt;
}

bool workshop_settings_keys_install(const cJSON *preference_keys, const cJSON *rules) {
    workshop_settings_keys_clear();
    if (!cJSON_IsArray(preference_keys) || !cJSON_IsArray(rules)) {
        return false;
    }
    size_t composer_count = (size_t)cJSON_GetArraySize(preference_keys);
    size_t rule_count = (size_t)cJSON_GetArraySize(rules);
    g_keys = calloc(composer_count + 2, sizeof(*g_keys));
    g_rules = calloc(rule_count ? rule_count : 1, sizeof(*g_rules));
    if (!g_keys || !g_rules) {
        workshop_settings_keys_clear();
        return false;
    }
    const cJSON *entry;
    cJSON_ArrayForEach(entry, preference_keys) {
        WorkshopSettingKey *row = &g_keys[g_key_count];
        const cJSON *min = cJSON_GetObjectItemCaseSensitive(entry, "min");
        const cJSON *max = cJSON_GetObjectItemCaseSensitive(entry, "max");
        const cJSON *words = cJSON_GetObjectItemCaseSensitive(entry, "words");
        const cJSON *dflt = cJSON_GetObjectItemCaseSensitive(entry, "default");
        copy_text(row->key, sizeof(row->key), cJSON_GetObjectItemCaseSensitive(entry, "key"));
        copy_text(row->type, sizeof(row->type), cJSON_GetObjectItemCaseSensitive(entry, "type"));
        row->min = cJSON_IsNumber(min) ? min->valueint : 0;
        row->max = cJSON_IsNumber(max) ? max->valueint : 0;
        row->hard = strcmp(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(entry, "class")) ? cJSON_GetObjectItemCaseSensitive(entry, "class")->valuestring : "", "hard") == 0;
        row->words = cJSON_IsArray(words) ? cJSON_Duplicate(words, true) : NULL;
        row->dflt = dflt ? cJSON_Duplicate(dflt, true) : NULL;
        g_key_count++;
        if (row->key[0] == '\0' || row->type[0] == '\0' || (cJSON_IsArray(words) && !row->words) || (dflt && !row->dflt)) {
            workshop_settings_keys_clear();
            return false;
        }
    }
    /* explore.share: a ratio from 0/1 to 1/2 with a denominator up to 16; explore.distance: how many settings an exploration moves. */
    add_workshop_key(&g_keys[g_key_count], "explore.share", "ratio", 0, 16, cJSON_CreateString("0/1"));
    g_key_count++;
    add_workshop_key(&g_keys[g_key_count], "explore.distance", "integer", 1, 8, cJSON_CreateNumber(2));
    g_key_count++;
    if (!g_keys[g_key_count - 2].dflt || !g_keys[g_key_count - 1].dflt) {
        workshop_settings_keys_clear();
        return false;
    }
    cJSON_ArrayForEach(entry, rules) {
        WorkshopSettingRule *row = &g_rules[g_rule_count++];
        copy_text(row->name, sizeof(row->name), cJSON_GetObjectItemCaseSensitive(entry, "name"));
        copy_text(row->key, sizeof(row->key), cJSON_GetObjectItemCaseSensitive(entry, "key"));
        const cJSON *class_name = cJSON_GetObjectItemCaseSensitive(entry, "class");
        row->hard = cJSON_IsString(class_name) && strcmp(class_name->valuestring, "hard") == 0;
        if (row->name[0] == '\0') {
            workshop_settings_keys_clear();
            return false;
        }
    }
    return true;
}

size_t workshop_settings_key_count(void) {
    return g_key_count;
}

const WorkshopSettingKey *workshop_settings_key_at(size_t index) {
    return index < g_key_count ? &g_keys[index] : NULL;
}

const WorkshopSettingKey *workshop_settings_key_find(const char *key) {
    for (size_t i = 0; key && i < g_key_count; i++) {
        if (strcmp(g_keys[i].key, key) == 0) {
            return &g_keys[i];
        }
    }
    return NULL;
}

size_t workshop_settings_rule_count(void) {
    return g_rule_count;
}

const WorkshopSettingRule *workshop_settings_rule_at(size_t index) {
    return index < g_rule_count ? &g_rules[index] : NULL;
}

const WorkshopSettingRule *workshop_settings_rule_find(const char *name) {
    for (size_t i = 0; name && i < g_rule_count; i++) {
        if (strcmp(g_rules[i].name, name) == 0) {
            return &g_rules[i];
        }
    }
    return NULL;
}

cJSON *workshop_settings_keys_json(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON *keys = root ? cJSON_AddArrayToObject(root, "keys") : NULL;
    cJSON *rules = keys ? cJSON_AddArrayToObject(root, "rules") : NULL;
    bool ok = rules != NULL;
    for (size_t i = 0; ok && i < g_key_count; i++) {
        const WorkshopSettingKey *row = &g_keys[i];
        cJSON *item = cJSON_CreateObject();
        ok = item && cJSON_AddStringToObject(item, "key", row->key) && cJSON_AddStringToObject(item, "type", row->type);
        if (ok && strcmp(row->type, "integer") == 0) {
            ok = cJSON_AddNumberToObject(item, "min", row->min) && cJSON_AddNumberToObject(item, "max", row->max);
        } else if (ok && strcmp(row->type, "ratio") == 0) {
            ok = cJSON_AddNumberToObject(item, "max", row->max);
        }
        ok = ok && (!row->words || cJSON_AddItemToObject(item, "words", cJSON_Duplicate(row->words, true))) &&
             cJSON_AddStringToObject(item, "class", row->hard ? "hard" : "soft") &&
             (!row->dflt || cJSON_AddItemToObject(item, "default", cJSON_Duplicate(row->dflt, true)));
        if (!ok || !cJSON_AddItemToArray(keys, item)) {
            cJSON_Delete(item);
            ok = false;
        }
    }
    for (size_t i = 0; ok && i < g_rule_count; i++) {
        cJSON *item = cJSON_CreateObject();
        ok = item && cJSON_AddStringToObject(item, "name", g_rules[i].name) &&
             cJSON_AddStringToObject(item, "class", g_rules[i].hard ? "hard" : "soft") && cJSON_AddStringToObject(item, "key", g_rules[i].key);
        if (!ok || !cJSON_AddItemToArray(rules, item)) {
            cJSON_Delete(item);
            ok = false;
        }
    }
    if (!ok) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}
