#include "settings/settings_resolve.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/json.h>

#include "items/store.h"
#include "settings/settings_keys.h"
#include "settings/settings_scopes.h"

typedef struct {
    cJSON *value;
    bool owned;
    char from[96];
} Slot;

typedef struct {
    Slot *slots;
    size_t count;
    cJSON *overrides; /* the answer's overrides, in the order they were applied */
} State;

static UrsatzStatus refuse(UrsatzError *error, UrsatzStatus status, const char *path, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

static UrsatzStatus refuse(UrsatzError *error, UrsatzStatus status, const char *path, const char *format, ...) {
    if (error) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error->message, sizeof error->message, format, arguments);
        va_end(arguments);
        snprintf(error->path, sizeof error->path, "%s", path);
    }
    return status;
}

/* --- overrides --------------------------------------------------------------------------------------------------------------------- */

static void rule_names(char *out, size_t size) {
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < workshop_settings_rule_count() && used < size; i++) {
        int written = snprintf(out + used, size - used, "%s%s", i ? ", " : "", workshop_settings_rule_at(i)->name);
        used += written > 0 ? (size_t)written : 0;
    }
}

UrsatzStatus workshop_settings_override_check(const char *rule, const char *action, const cJSON *weight, const char *where, UrsatzError *error) {
    char path[200];
    const WorkshopSettingRule *found = workshop_settings_rule_find(rule);
    if (!found) {
        char names[400];
        rule_names(names, sizeof(names));
        snprintf(path, sizeof path, "%s", where);
        return refuse(error, URSATZ_ERR_REQUEST, path, "'%s' is not a rule: the rules are %s", rule ? rule : "", names);
    }
    if (!action || (strcmp(action, "off") != 0 && strcmp(action, "reverse") != 0 && strcmp(action, "reweight") != 0 && strcmp(action, "waive") != 0)) {
        snprintf(path, sizeof path, "%s", where);
        return refuse(error, URSATZ_ERR_REQUEST, path, "an override's action is off, reverse, reweight or waive");
    }
    if (strcmp(action, "reverse") == 0 && found->hard) {
        return refuse(error, URSATZ_ERR_REQUEST, where, "%s is a hard rule: only a soft rule group can be reversed", found->name);
    }
    if (strcmp(action, "reweight") == 0) {
        if (found->hard) {
            return refuse(error, URSATZ_ERR_REQUEST, where, "%s is a hard rule: only a soft rule group has a weight", found->name);
        }
        if (!cJSON_IsNumber(weight) || weight->valuedouble != (double)(long long)weight->valuedouble || weight->valuedouble < 0 || weight->valuedouble > 20) {
            return refuse(error, URSATZ_ERR_REQUEST, where, "a reweight needs a weight, a whole number from 0 to 20");
        }
    }
    return URSATZ_OK;
}

/* --- the state --------------------------------------------------------------------------------------------------------------------- */

static Slot *slot_of(State *state, const WorkshopSettingKey *key) {
    return &state->slots[key - workshop_settings_key_at(0)];
}

static bool set_slot(Slot *slot, cJSON *value, bool owned, const char *from) {
    if (slot->owned) {
        cJSON_Delete(slot->value);
    }
    slot->value = value;
    slot->owned = owned;
    snprintf(slot->from, sizeof(slot->from), "%s", from);
    return value != NULL;
}

static void state_free(State *state) {
    for (size_t i = 0; state->slots && i < state->count; i++) {
        if (state->slots[i].owned) {
            cJSON_Delete(state->slots[i].value);
        }
    }
    free(state->slots);
    cJSON_Delete(state->overrides);
    memset(state, 0, sizeof(*state));
}

/* The value at a key from a scope's values: files hold flat dotted keys, the request a nested document. */
static UrsatzStatus put_value(State *state, const char *key_name, const cJSON *value, const char *from, bool strict, const char *where, UrsatzError *error) {
    const WorkshopSettingKey *key = workshop_settings_key_find(key_name);
    if (!key) {
        return strict ? refuse(error, URSATZ_ERR_REQUEST, where, "unknown setting '%s'", key_name) : URSATZ_OK;
    }
    if (!set_slot(slot_of(state, key), cJSON_Duplicate(value, true), true, from)) {
        return refuse(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return URSATZ_OK;
}

/* A nested document's leaves (an object is descended into, a list or a value is a leaf). */
static UrsatzStatus put_document(State *state, const cJSON *object, const char *prefix, const char *from, const char *where, UrsatzError *error) {
    const cJSON *item;
    cJSON_ArrayForEach(item, object) {
        char key[160];
        snprintf(key, sizeof key, "%s%s%s", prefix, prefix[0] ? "." : "", item->string);
        UrsatzStatus status;
        if (cJSON_IsObject(item)) {
            status = put_document(state, item, key, from, where, error);
        } else {
            status = put_value(state, key, item, from, true, where, error);
        }
        if (status != URSATZ_OK) {
            return status;
        }
    }
    return URSATZ_OK;
}

/* The slot of key becomes a list holding what it held and `name`, in the order of the key's words. */
static UrsatzStatus list_add(State *state, const char *key_name, const char *name, const char *from, UrsatzError *error) {
    const WorkshopSettingKey *key = workshop_settings_key_find(key_name);
    if (!key || !key->words) {
        return URSATZ_OK;
    }
    Slot *slot = slot_of(state, key);
    cJSON *list = cJSON_CreateArray();
    const cJSON *word;
    bool ok = list != NULL;
    cJSON_ArrayForEach(word, key->words) {
        bool wanted = strcmp(word->valuestring, name) == 0;
        const cJSON *held;
        cJSON_ArrayForEach(held, slot->value) {
            wanted = wanted || (cJSON_IsString(held) && strcmp(held->valuestring, word->valuestring) == 0);
        }
        if (ok && wanted) {
            cJSON *text = cJSON_CreateString(word->valuestring);
            ok = text && cJSON_AddItemToArray(list, text);
            if (!ok) {
                cJSON_Delete(text);
            }
        }
    }
    if (!ok || !set_slot(slot, list, true, from)) {
        cJSON_Delete(list);
        return refuse(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return URSATZ_OK;
}

static UrsatzStatus set_weight(State *state, const WorkshopSettingRule *rule, int weight, const char *from, UrsatzError *error) {
    const char *keys[2] = {rule->key, NULL};
    if (strcmp(rule->name, "closure") == 0) {
        keys[0] = "melody.closure.final";
        keys[1] = "melody.closure.first";
    }
    for (size_t i = 0; i < 2 && keys[i]; i++) {
        const WorkshopSettingKey *key = workshop_settings_key_find(keys[i]);
        if (key && !set_slot(slot_of(state, key), cJSON_CreateNumber(weight), true, from)) {
            return refuse(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
    }
    return URSATZ_OK;
}

/* One override, applied over what the scopes before it gave (R-671): reweight sets the group's weight, off and waive on a soft group set it to
 * 0, off and waive on a hard rule add it to melody.waive, reverse adds the group to melody.reverse. */
static UrsatzStatus apply_override(State *state, const cJSON *entry, const char *from, bool strict, const char *where, UrsatzError *error) {
    const char *rule_name = common_json_string(entry, "rule");
    const char *action = common_json_string(entry, "action");
    const cJSON *weight = cJSON_GetObjectItemCaseSensitive(entry, "weight");
    const WorkshopSettingRule *rule = workshop_settings_rule_find(rule_name);
    if (!rule) {
        return strict ? workshop_settings_override_check(rule_name, action, weight, where, error) : URSATZ_OK;
    }
    if (strict) {
        UrsatzStatus checked = workshop_settings_override_check(rule_name, action, weight, where, error);
        if (checked != URSATZ_OK) {
            return checked;
        }
    }
    UrsatzStatus status = URSATZ_OK;
    if (strcmp(action, "reweight") == 0 && !rule->hard) {
        status = set_weight(state, rule, cJSON_IsNumber(weight) ? weight->valueint : 0, from, error);
    } else if (strcmp(action, "reverse") == 0 && !rule->hard) {
        status = list_add(state, strncmp(rule->key, "critic.", 7) == 0 ? "critic.reverse" : "melody.reverse", rule->name, from, error);
    } else if (strcmp(action, "off") == 0 || strcmp(action, "waive") == 0) {
        status = rule->hard ? list_add(state, "melody.waive", rule->name, from, error) : set_weight(state, rule, 0, from, error);
    }
    if (status != URSATZ_OK) {
        return status;
    }
    cJSON *record = cJSON_CreateObject();
    bool ok = record && cJSON_AddStringToObject(record, "rule", rule->name) && cJSON_AddStringToObject(record, "action", action) &&
              (strcmp(action, "reweight") != 0 || cJSON_AddNumberToObject(record, "weight", cJSON_IsNumber(weight) ? weight->valuedouble : 0)) &&
              cJSON_AddStringToObject(record, "from", from) && cJSON_AddItemToArray(state->overrides, record);
    if (!ok) {
        cJSON_Delete(record);
        return refuse(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return URSATZ_OK;
}

static UrsatzStatus apply_scope(State *state, const cJSON *file, const char *from, UrsatzError *error) {
    const cJSON *values = cJSON_GetObjectItemCaseSensitive(file, "values");
    const cJSON *entry;
    cJSON_ArrayForEach(entry, values) {
        UrsatzStatus status = put_value(state, entry->string, entry, from, false, "", error);
        if (status != URSATZ_OK) {
            return status;
        }
    }
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(file, "overrides")) {
        UrsatzStatus status = apply_override(state, entry, from, false, "", error);
        if (status != URSATZ_OK) {
            return status;
        }
    }
    return URSATZ_OK;
}

/* --- the switches ------------------------------------------------------------------------------------------------------------------ */

typedef struct {
    bool style;
    bool taste;
    bool project;
} Switches;

static void add_switches(Switches *off, const cJSON *list) {
    const cJSON *word;
    cJSON_ArrayForEach(word, list) {
        if (cJSON_IsString(word)) {
            off->style = off->style || strcmp(word->valuestring, "style") == 0;
            off->taste = off->taste || strcmp(word->valuestring, "taste") == 0;
            off->project = off->project || strcmp(word->valuestring, "project") == 0;
        }
    }
}

static UrsatzStatus check_request_off(const cJSON *off, UrsatzError *error) {
    if (!off || cJSON_IsNull(off)) {
        return URSATZ_OK;
    }
    if (!cJSON_IsArray(off)) {
        return refuse(error, URSATZ_ERR_REQUEST, "/params/off", "off must be a list of style, taste or project");
    }
    int index = 0;
    const cJSON *word;
    cJSON_ArrayForEach(word, off) {
        if (!cJSON_IsString(word) || (strcmp(word->valuestring, "style") != 0 && strcmp(word->valuestring, "taste") != 0 && strcmp(word->valuestring, "project") != 0)) {
            char path[64];
            snprintf(path, sizeof path, "/params/off/%d", index);
            return refuse(error, URSATZ_ERR_REQUEST, path, "a switch is style, taste or project");
        }
        index++;
    }
    return URSATZ_OK;
}

/* --- exploring (R-673) ------------------------------------------------------------------------------------------------------------- */

static unsigned long long xorshift64(unsigned long long *state) {
    unsigned long long x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

typedef struct {
    const char *key;
    size_t index;
} Candidate;

static int by_key(const void *a, const void *b) {
    return strcmp(((const Candidate *)a)->key, ((const Candidate *)b)->key);
}

static bool group_read(const char *kind, const char *key) {
    if (strncmp(key, "motif.", 6) == 0) {
        return true;
    }
    return strcmp(kind, "theme") == 0 && (strncmp(key, "melody.", 7) == 0 || strncmp(key, "development.", 12) == 0);
}

/* Picks `distance` of the settings the kind may move and moves them; *moves is the array [{key, from, to}] (the caller's). */
static UrsatzStatus explore(State *state, const char *kind, unsigned long long seed, int distance, cJSON **moves, UrsatzError *error) {
    Candidate *candidates = calloc(state->count ? state->count : 1, sizeof(*candidates));
    size_t count = 0;
    *moves = cJSON_CreateArray();
    if (!candidates || !*moves) {
        free(candidates);
        return refuse(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    for (size_t i = 0; i < state->count; i++) {
        const WorkshopSettingKey *key = workshop_settings_key_at(i);
        bool boolean = strcmp(key->type, "boolean") == 0;
        bool weight = strcmp(key->type, "integer") == 0 && key->min == 0 && key->max == 20;
        if (!key->hard && (boolean || weight) && group_read(kind, key->key)) {
            candidates[count].key = key->key;
            candidates[count].index = i;
            count++;
        }
    }
    qsort(candidates, count, sizeof(*candidates), by_key);
    unsigned long long generator = seed ? seed : 1;
    UrsatzStatus status = URSATZ_OK;
    for (int pick = 0; pick < distance && count > 0 && status == URSATZ_OK; pick++) {
        size_t chosen = (size_t)(xorshift64(&generator) % count);
        const WorkshopSettingKey *key = workshop_settings_key_at(candidates[chosen].index);
        Slot *slot = &state->slots[candidates[chosen].index];
        cJSON *from = slot->value ? cJSON_Duplicate(slot->value, true) : cJSON_CreateNull();
        cJSON *to = NULL;
        if (strcmp(key->type, "boolean") == 0) {
            to = cJSON_CreateBool(!cJSON_IsTrue(slot->value));
        } else {
            int value = cJSON_IsNumber(slot->value) ? slot->value->valueint : 0;
            int moved = value >= 3 ? 0 : (value + 3 < key->max ? value + 3 : key->max);
            to = cJSON_CreateNumber(moved);
        }
        cJSON *record = cJSON_CreateObject();
        bool ok = from && to && record && cJSON_AddStringToObject(record, "key", key->key) && cJSON_AddItemToObject(record, "from", from) &&
                  cJSON_AddItemToObject(record, "to", to);
        if (ok) {
            from = NULL;
            to = NULL;
        }
        ok = ok && cJSON_AddItemToArray(*moves, record);
        if (ok) {
            ok = set_slot(slot, cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(record, "to"), true), true, "explore");
        } else {
            cJSON_Delete(from);
            cJSON_Delete(to);
            cJSON_Delete(record);
        }
        if (!ok) {
            status = refuse(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
        /* a setting moves once: the last candidate takes the place of the one picked */
        candidates[chosen] = candidates[count - 1];
        count--;
        qsort(candidates, count, sizeof(*candidates), by_key);
    }
    free(candidates);
    return status;
}

/* --- the answer -------------------------------------------------------------------------------------------------------------------- */

/* Sets the value at a dotted key of a nested document, making the objects on the way. */
static bool nest(cJSON *document, const char *key, const cJSON *value) {
    char part[96];
    cJSON *at = document;
    while (strchr(key, '.')) {
        size_t length = (size_t)(strchr(key, '.') - key);
        snprintf(part, sizeof part, "%.*s", (int)length, key);
        cJSON *next = cJSON_GetObjectItemCaseSensitive(at, part);
        if (!next) {
            next = cJSON_AddObjectToObject(at, part);
        }
        if (!next) {
            return false;
        }
        at = next;
        key += length + 1;
    }
    cJSON *copy = cJSON_Duplicate(value, true);
    return copy && cJSON_AddItemToObject(at, key, copy);
}

static UrsatzStatus answer(const State *state, const char *taste, const Switches *off, bool all, cJSON *explore_record, cJSON **out, UrsatzError *error) {
    cJSON *root = cJSON_CreateObject();
    cJSON *off_list = root ? cJSON_CreateArray() : NULL;
    cJSON *values = off_list ? cJSON_CreateArray() : NULL;
    cJSON *document = values ? cJSON_CreateObject() : NULL;
    bool ok = document && cJSON_AddStringToObject(root, "taste", taste);
    if (ok) {
        const struct {
            const char *word;
            bool set;
        } words[] = {{"style", off->style}, {"taste", off->taste}, {"project", off->project}};
        for (size_t i = 0; ok && i < 3; i++) {
            cJSON *text = words[i].set ? cJSON_CreateString(words[i].word) : NULL;
            ok = !words[i].set || (text && cJSON_AddItemToArray(off_list, text));
            if (!ok) {
                cJSON_Delete(text);
            }
        }
    }
    ok = ok && cJSON_AddItemToObject(root, "off", off_list);
    off_list = ok ? NULL : off_list;
    size_t document_keys = 0;
    for (size_t i = 0; ok && i < state->count; i++) {
        const Slot *slot = &state->slots[i];
        const WorkshopSettingKey *key = workshop_settings_key_at(i);
        if (!all && strcmp(slot->from, "default") == 0) {
            continue;
        }
        cJSON *row = cJSON_CreateObject();
        ok = row && cJSON_AddStringToObject(row, "key", key->key) && cJSON_AddItemToObject(row, "value", slot->value ? cJSON_Duplicate(slot->value, true) : cJSON_CreateNull()) &&
             cJSON_AddStringToObject(row, "from", slot->from) && cJSON_AddItemToArray(values, row);
        if (!ok) {
            cJSON_Delete(row);
            break;
        }
        if (strcmp(slot->from, "default") != 0 && strncmp(key->key, "explore.", 8) != 0) {
            ok = nest(document, key->key, slot->value);
            document_keys++;
        }
    }
    ok = ok && cJSON_AddItemToObject(root, "values", values);
    values = ok ? NULL : values;
    cJSON *overrides = ok ? cJSON_Duplicate(state->overrides, true) : NULL;
    ok = overrides && cJSON_AddItemToObject(root, "overrides", overrides);
    if (ok && document_keys > 0) {
        ok = cJSON_AddItemToObject(root, "document", document);
        document = ok ? NULL : document;
    } else if (ok) {
        ok = cJSON_AddNullToObject(root, "document") != NULL;
    }
    if (ok && explore_record) {
        ok = cJSON_AddItemToObject(root, "explore", explore_record);
        explore_record = ok ? NULL : explore_record;
    }
    cJSON_Delete(explore_record);
    cJSON_Delete(off_list);
    cJSON_Delete(values);
    cJSON_Delete(document);
    if (!ok) {
        cJSON_Delete(root);
        return refuse(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    *out = root;
    return URSATZ_OK;
}

/* --- the resolution ---------------------------------------------------------------------------------------------------------------- */

UrsatzStatus workshop_settings_resolve_request(const WorkshopSettingsRequest *request, bool all, cJSON **out, UrsatzError *error) {
    *out = NULL;
    if (workshop_settings_key_count() == 0) {
        return refuse(error, URSATZ_ERR_INTERNAL, "", "the setting keys are not installed");
    }
    UrsatzStatus status = check_request_off(request->off, error);
    if (status != URSATZ_OK) {
        return status;
    }
    if (request->values && !cJSON_IsNull(request->values) && !cJSON_IsObject(request->values)) {
        return refuse(error, URSATZ_ERR_REQUEST, "/params/settings", "settings must be an object: a preferences document");
    }
    /* the project of an item is the item's own */
    char project_id[96] = "";
    if (request->project_id && request->project_id[0]) {
        snprintf(project_id, sizeof project_id, "%s", request->project_id);
    } else if (request->item_id && request->item_id[0]) {
        cJSON *meta = NULL;
        if (workshop_material_item_get(request->item_id, &meta, NULL) != URSATZ_OK) {
            return refuse(error, URSATZ_ERR_NOT_FOUND, "/params/item_id", "there is no item '%s'", request->item_id);
        }
        snprintf(project_id, sizeof project_id, "%s", common_json_string(meta, "project_id"));
        cJSON_Delete(meta);
    }
    char scope[128];
    cJSON *style_file = NULL;
    cJSON *project_file = NULL;
    cJSON *item_file = NULL;
    cJSON *taste_all = NULL;
    cJSON *taste_file = NULL;
    State state;
    memset(&state, 0, sizeof(state));
    char profile[96] = "none";
    Switches off;
    memset(&off, 0, sizeof(off));
    add_switches(&off, request->off);
    status = URSATZ_OK;
#define SCOPE_FILE(var, text)                      \
    do {                                           \
        status = workshop_scope_read(text, &var, error); \
        if (status != URSATZ_OK) {                 \
            goto done;                             \
        }                                          \
    } while (0)
    if (request->style_id && request->style_id[0]) {
        snprintf(scope, sizeof scope, "style:%s", request->style_id);
        SCOPE_FILE(style_file, scope);
    }
    if (request->item_id && request->item_id[0]) {
        snprintf(scope, sizeof scope, "item:%s", request->item_id);
        SCOPE_FILE(item_file, scope);
        add_switches(&off, cJSON_GetObjectItemCaseSensitive(item_file, "off"));
    }
    if (project_id[0]) {
        snprintf(scope, sizeof scope, "project:%s", project_id);
        SCOPE_FILE(project_file, scope);
        if (!off.project) {
            add_switches(&off, cJSON_GetObjectItemCaseSensitive(project_file, "off"));
        }
    }
    /* the profile: the request's, else the item's, the project's, the style's, else none */
    {
        const cJSON *named[3] = {item_file ? cJSON_GetObjectItemCaseSensitive(item_file, "taste") : NULL,
                                 project_file ? cJSON_GetObjectItemCaseSensitive(project_file, "taste") : NULL,
                                 style_file ? cJSON_GetObjectItemCaseSensitive(style_file, "taste") : NULL};
        if (request->taste && request->taste[0]) {
            snprintf(profile, sizeof profile, "%s", request->taste);
        } else {
            for (size_t i = 0; i < 3; i++) {
                if (cJSON_IsString(named[i]) && named[i]->valuestring[0]) {
                    snprintf(profile, sizeof profile, "%s", named[i]->valuestring);
                    break;
                }
            }
        }
    }
    snprintf(scope, sizeof scope, "taste:%s", profile);
    SCOPE_FILE(taste_file, scope);
    if (strcmp(profile, "all") != 0) {
        SCOPE_FILE(taste_all, "taste:all");
    }
    state.count = workshop_settings_key_count();
    state.slots = calloc(state.count, sizeof(*state.slots));
    state.overrides = cJSON_CreateArray();
    if (!state.slots || !state.overrides) {
        status = refuse(error, URSATZ_ERR_ALLOC, "", "out of memory");
        goto done;
    }
    for (size_t i = 0; i < state.count; i++) {
        state.slots[i].value = workshop_settings_key_at(i)->dflt;
        snprintf(state.slots[i].from, sizeof(state.slots[i].from), "default");
    }
    if (style_file && !off.style) {
        snprintf(scope, sizeof scope, "style:%s", request->style_id);
        status = apply_scope(&state, style_file, scope, error);
    }
    if (status == URSATZ_OK && taste_all && !off.taste) {
        status = apply_scope(&state, taste_all, "taste:all", error);
    }
    if (status == URSATZ_OK && !off.taste) {
        snprintf(scope, sizeof scope, "taste:%s", profile);
        status = apply_scope(&state, taste_file, scope, error);
    }
    if (status == URSATZ_OK && project_file && !off.project) {
        snprintf(scope, sizeof scope, "project:%s", project_id);
        status = apply_scope(&state, project_file, scope, error);
    }
    if (status == URSATZ_OK && item_file) {
        snprintf(scope, sizeof scope, "item:%s", request->item_id);
        status = apply_scope(&state, item_file, scope, error);
    }
    if (status == URSATZ_OK && cJSON_IsObject(request->values)) {
        status = put_document(&state, request->values, "", "request", "/params/settings", error);
    }
    if (status == URSATZ_OK && request->overrides && !cJSON_IsNull(request->overrides)) {
        if (!cJSON_IsArray(request->overrides)) {
            status = refuse(error, URSATZ_ERR_REQUEST, "/params/overrides", "overrides must be a list of {rule, action, weight}");
        }
        int index = 0;
        const cJSON *entry;
        cJSON_ArrayForEach(entry, request->overrides) {
            char where[64];
            snprintf(where, sizeof where, "/params/overrides/%d", index++);
            status = status == URSATZ_OK && cJSON_IsObject(entry) ? apply_override(&state, entry, "request", true, where, error)
                                                                   : (status == URSATZ_OK ? refuse(error, URSATZ_ERR_REQUEST, where, "an override is an object {rule, action, weight}") : status);
            if (status != URSATZ_OK) {
                break;
            }
        }
    }
    cJSON *explore_record = NULL;
    if (status == URSATZ_OK && request->explore) {
        const WorkshopSettingKey *distance_key = workshop_settings_key_find("explore.distance");
        const cJSON *resolved = distance_key ? slot_of(&state, distance_key)->value : NULL;
        int distance = request->explore_distance > 0 ? request->explore_distance : (cJSON_IsNumber(resolved) ? resolved->valueint : 2);
        cJSON *moves = NULL;
        status = explore(&state, request->kind ? request->kind : "theme", request->explore_seed, distance, &moves, error);
        explore_record = cJSON_CreateObject();
        if (status == URSATZ_OK && (!explore_record || !cJSON_AddNumberToObject(explore_record, "seed", (double)request->explore_seed) ||
                                    !cJSON_AddNumberToObject(explore_record, "distance", distance) || !cJSON_AddItemToObject(explore_record, "moves", moves))) {
            status = refuse(error, URSATZ_ERR_ALLOC, "", "out of memory");
            cJSON_Delete(moves);
        } else if (status != URSATZ_OK) {
            cJSON_Delete(moves);
        }
    }
    if (status == URSATZ_OK) {
        status = answer(&state, profile, &off, all, explore_record, out, error);
        explore_record = NULL;
    }
    cJSON_Delete(explore_record);
done:
    state_free(&state);
    cJSON_Delete(style_file);
    cJSON_Delete(project_file);
    cJSON_Delete(item_file);
    cJSON_Delete(taste_all);
    cJSON_Delete(taste_file);
    return status;
}
