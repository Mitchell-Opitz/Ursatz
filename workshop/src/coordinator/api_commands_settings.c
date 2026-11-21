#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/json.h>
#include <common/strings.h>

#include "composition/composer_client.h"
#include "coordinator/dispatch.h"
#include "items/layout.h"
#include "items/txn.h"
#include "settings/settings_keys.h"
#include "settings/settings_resolve.h"
#include "settings/settings_scopes.h"

/* The settings commands (R-671, R-676): each changes the file of one scope, through one transaction, and answers with the scope's content. A taste
 * profile changes only by settings.import and by keep as default (ratings.keep), which cite why. */

/* --- checking a value -------------------------------------------------------------------------------------------------------------- */

/* The value of a dotted key as a nested document {"melody": {"keep": 3}}; NULL on allocation failure. */
static cJSON *nested(const char *key, const cJSON *value) {
    cJSON *root = cJSON_CreateObject();
    cJSON *at = root;
    char part[96];
    while (root && strchr(key, '.')) {
        size_t length = (size_t)(strchr(key, '.') - key);
        snprintf(part, sizeof part, "%.*s", (int)length, key);
        at = cJSON_AddObjectToObject(at, part);
        if (!at) {
            break;
        }
        key += length + 1;
    }
    cJSON *copy = at ? cJSON_Duplicate(value, true) : NULL;
    if (!copy || !cJSON_AddItemToObject(at, key, copy)) {
        cJSON_Delete(copy);
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

/* "a/b/c" for the dotted key "a.b.c". */
static void slash_path(const char *key, char *out, size_t size) {
    size_t used = 0;
    for (const char *c = key; *c && used + 1 < size; c++) {
        out[used++] = *c == '.' ? '/' : *c;
    }
    out[used] = '\0';
}

/* explore.share: a ratio n/d from 0/1 to 1/2, d up to 16. */
static bool explore_share_ok(const cJSON *value) {
    if (!cJSON_IsString(value)) {
        return false;
    }
    unsigned long n = 0;
    unsigned long d = 0;
    char extra = 0;
    if (sscanf(value->valuestring, "%lu/%lu%c", &n, &d, &extra) != 2 || d < 1 || d > 16) {
        return false;
    }
    return n * 2 <= d;
}

UrsatzStatus workshop_settings_check_value(const char *key_name, const cJSON *value, const char *where, bool request_scope, UrsatzError *error) {
    const WorkshopSettingKey *key = workshop_settings_key_find(key_name);
    if (!key) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, where, "unknown setting '%s'", key_name);
    }
    if (strcmp(key->key, "motif.plan") == 0 && !request_scope) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, where, "a chord plan is intent, not a setting: give it with the request");
    }
    if (strcmp(key->key, "explore.share") == 0) {
        return explore_share_ok(value) ? URSATZ_OK
                                       : workshop_call_fail(error, URSATZ_ERR_REQUEST, where, "explore.share must be a ratio from 0/1 to 1/2 with a denominator up to 16");
    }
    if (strcmp(key->key, "explore.distance") == 0) {
        return cJSON_IsNumber(value) && value->valuedouble == (double)(long long)value->valuedouble && value->valuedouble >= 1 && value->valuedouble <= 8
                   ? URSATZ_OK
                   : workshop_call_fail(error, URSATZ_ERR_REQUEST, where, "explore.distance must be a whole number from 1 to 8");
    }
    cJSON *document = nested(key->key, value);
    if (!document) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    char path[160];
    char message[256];
    bool ok = workshop_composer_validate_preferences(document, path, sizeof path, message, sizeof message);
    cJSON_Delete(document);
    if (ok) {
        return URSATZ_OK;
    }
    /* the Composer's path is /preferences/...; ours is the same place in the call */
    /* a set reports at its value; an import at the place in its document */
    char full[200];
    snprintf(full, sizeof full, "%s%s", strncmp(where, "/params/document", 16) == 0 ? "/params/document" : where,
             strncmp(where, "/params/document", 16) == 0 && strncmp(path, "/preferences", 12) == 0 ? path + 12 : "");
    return workshop_call_fail(error, URSATZ_ERR_REQUEST, full, "%s", message);
}

/* --- the scope of a call ------------------------------------------------------------------------------------------------------------ */

typedef struct {
    const char *scope;
    WorkshopScopeKind kind;
    cJSON *file;
} Scope;

/* Reads the call's scope: the file (or an empty scope) in out->file. kinds is the bits of the scope kinds the command takes. */
static UrsatzStatus open_scope(const WorkshopCall *call, unsigned kinds, const char *refusal, Scope *out, UrsatzError *error) {
    memset(out, 0, sizeof(*out));
    if (!workshop_param_require_string(call->params, "scope", &out->scope, error)) {
        return URSATZ_ERR_REQUEST;
    }
    char id[80];
    if (!workshop_scope_parse(out->scope, &out->kind, id, sizeof id)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/scope", "a scope is taste:<name>, style:<id>, project:<id> or item:<id>");
    }
    if (!((kinds >> out->kind) & 1u)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/scope", "%s", refusal);
    }
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_scope_read(out->scope, &out->file, &failure);
    if (status != URSATZ_OK) {
        return workshop_call_fail(error, status, failure.path, "%s", failure.message);
    }
    return URSATZ_OK;
}

#define KIND(k) (1u << (k))

/* The values object of the file, made when it has none. */
static cJSON *values_of(cJSON *file) {
    cJSON *values = cJSON_GetObjectItemCaseSensitive(file, "values");
    if (!cJSON_IsObject(values)) {
        cJSON_DeleteItemFromObjectCaseSensitive(file, "values");
        values = cJSON_AddObjectToObject(file, "values");
    }
    return values;
}

static void put(cJSON *object, const char *name, cJSON *item) {
    if (cJSON_GetObjectItemCaseSensitive(object, name)) {
        cJSON_ReplaceItemInObjectCaseSensitive(object, name, item);
    } else {
        cJSON_AddItemToObject(object, name, item);
    }
}

/* Stages the scope's file in one transaction and answers with its content. The file is consumed. */
static UrsatzStatus save_scope(const WorkshopCall *call, WorkshopReply *reply, Scope *scope, UrsatzError *error) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "settings", call->actor, "", 0);
    if (!txn) {
        cJSON_Delete(scope->file);
        return workshop_call_fail(error, URSATZ_ERR_IO, "", "the journal cannot be read");
    }
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_scope_stage(txn, scope->scope, scope->file, &failure);
    if (status != URSATZ_OK) {
        workshop_txn_abort(txn);
        cJSON_Delete(scope->file);
        return workshop_call_fail(error, status, failure.path, "%s", failure.message);
    }
    status = workshop_txn_commit(txn, &failure);
    if (status != URSATZ_OK) {
        cJSON_Delete(scope->file);
        return workshop_call_fail(error, status, failure.path, "%s", failure.message[0] ? failure.message : "the settings cannot be saved");
    }
    return workshop_reply_tree(reply, scope->file, error);
}

/* Returns from the handler, freeing the scope's file, when status is not OK. */
#define TAKE(expression)                \
    do {                                \
        UrsatzStatus taken = (expression); \
        if (taken != URSATZ_OK) {       \
            cJSON_Delete(scope.file);   \
            return taken;               \
        }                               \
    } while (0)

static const char *const kTasteRefusal = "a taste setting changes only by keep as default (ratings.keep)";

/* The check of a setting named by the params: its key must exist (at /params/key), then its value must hold (at where). */
static UrsatzStatus check_key_value(const char *key, const cJSON *value, UrsatzError *error) {
    if (!workshop_settings_key_find(key)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/key", "unknown setting '%s'", key);
    }
    return workshop_settings_check_value(key, value, "/params/value", false, error);
}

/* settings.set: scope, key, value. */
WORKSHOP_HANDLER(workshop_c_settings_set) {
    Scope scope;
    const char *key = NULL;
    UrsatzStatus status = open_scope(call, KIND(WORKSHOP_SCOPE_STYLE) | KIND(WORKSHOP_SCOPE_PROJECT) | KIND(WORKSHOP_SCOPE_ITEM), kTasteRefusal, &scope, error);
    if (status != URSATZ_OK) {
        return status;
    }
    TAKE(workshop_param_require_string(call->params, "key", &key, error) ? URSATZ_OK : URSATZ_ERR_REQUEST);
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(call->params, "value");
    TAKE(check_key_value(key, value, error));
    cJSON *copy = cJSON_Duplicate(value, true);
    cJSON *values = values_of(scope.file);
    if (!copy || !values) {
        cJSON_Delete(copy);
        cJSON_Delete(scope.file);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    put(values, key, copy);
    return save_scope(call, reply, &scope, error);
}

/* settings.unset: scope, key. */
WORKSHOP_HANDLER(workshop_c_settings_unset) {
    Scope scope;
    const char *key = NULL;
    UrsatzStatus status = open_scope(call, KIND(WORKSHOP_SCOPE_STYLE) | KIND(WORKSHOP_SCOPE_PROJECT) | KIND(WORKSHOP_SCOPE_ITEM), kTasteRefusal, &scope, error);
    if (status != URSATZ_OK) {
        return status;
    }
    TAKE(workshop_param_require_string(call->params, "key", &key, error) ? URSATZ_OK : URSATZ_ERR_REQUEST);
    if (!workshop_settings_key_find(key)) {
        cJSON_Delete(scope.file);
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/key", "unknown setting '%s'", key);
    }
    cJSON_DeleteItemFromObjectCaseSensitive(values_of(scope.file), key);
    return save_scope(call, reply, &scope, error);
}

/* The leaves of a nested document, each checked as settings.set checks a value and put into values at its dotted key. */
static UrsatzStatus import_document(const cJSON *object, const char *prefix, cJSON *values, UrsatzError *error) {
    const cJSON *item;
    cJSON_ArrayForEach(item, object) {
        char key[160];
        snprintf(key, sizeof key, "%s%s%s", prefix, prefix[0] ? "." : "", item->string);
        if (cJSON_IsObject(item)) {
            UrsatzStatus status = import_document(item, key, values, error);
            if (status != URSATZ_OK) {
                return status;
            }
            continue;
        }
        char where[220];
        char slashed[160];
        slash_path(key, slashed, sizeof slashed);
        snprintf(where, sizeof where, "/params/document/%s", slashed);
        UrsatzStatus status = workshop_settings_check_value(key, item, where, false, error);
        if (status != URSATZ_OK) {
            return status;
        }
        cJSON *copy = cJSON_Duplicate(item, true);
        if (!copy) {
            return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
        put(values, key, copy);
    }
    return URSATZ_OK;
}

/* Adds {key, value, ratings, note, actor, at} to the kept list of a taste file (made when missing). */
UrsatzStatus workshop_settings_keep_record(cJSON *file, const char *key, const cJSON *value, const cJSON *ratings, const char *note, const char *actor,
                                           UrsatzError *error) {
    cJSON *kept = cJSON_GetObjectItemCaseSensitive(file, "kept");
    if (!cJSON_IsArray(kept)) {
        cJSON_DeleteItemFromObjectCaseSensitive(file, "kept");
        kept = cJSON_AddArrayToObject(file, "kept");
    }
    char now[32];
    common_utc_now(now);
    cJSON *entry = kept ? cJSON_CreateObject() : NULL;
    bool ok = entry && cJSON_AddStringToObject(entry, "key", key) &&
              cJSON_AddItemToObject(entry, "value", value ? cJSON_Duplicate(value, true) : cJSON_CreateNull()) &&
              cJSON_AddItemToObject(entry, "ratings", ratings ? cJSON_Duplicate(ratings, true) : cJSON_CreateArray()) &&
              cJSON_AddStringToObject(entry, "note", note ? note : "") && cJSON_AddStringToObject(entry, "actor", actor ? actor : "") &&
              cJSON_AddStringToObject(entry, "at", now) && cJSON_AddItemToArray(kept, entry);
    if (!ok) {
        cJSON_Delete(entry);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return URSATZ_OK;
}

/* settings.import: scope, document, replace, note. */
WORKSHOP_HANDLER(workshop_c_settings_import) {
    Scope scope;
    UrsatzStatus status = open_scope(call, KIND(WORKSHOP_SCOPE_TASTE) | KIND(WORKSHOP_SCOPE_STYLE) | KIND(WORKSHOP_SCOPE_PROJECT) | KIND(WORKSHOP_SCOPE_ITEM), "", &scope, error);
    if (status != URSATZ_OK) {
        return status;
    }
    const cJSON *document = cJSON_GetObjectItemCaseSensitive(call->params, "document");
    bool bad = false;
    const char *note = workshop_param_string(call->params, "note", &bad);
    if (scope.kind == WORKSHOP_SCOPE_TASTE && (!note || note[0] == '\0')) {
        cJSON_Delete(scope.file);
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/note", "note is required for a taste profile: say where the settings come from");
    }
    cJSON *values = values_of(scope.file);
    if (!values) {
        cJSON_Delete(scope.file);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(call->params, "replace"))) {
        cJSON_DeleteItemFromObjectCaseSensitive(scope.file, "values");
        values = values_of(scope.file);
    }
    TAKE(import_document(document, "", values, error));
    if (scope.kind == WORKSHOP_SCOPE_TASTE) {
        TAKE(workshop_settings_keep_record(scope.file, "*", NULL, NULL, note, call->actor, error));
    }
    return save_scope(call, reply, &scope, error);
}

/* settings.override: scope, rule, action, weight, note. */
WORKSHOP_HANDLER(workshop_c_settings_override) {
    Scope scope;
    UrsatzStatus status = open_scope(call, KIND(WORKSHOP_SCOPE_PROJECT) | KIND(WORKSHOP_SCOPE_ITEM), "overrides go on a project, an item or a request, never in a profile", &scope, error);
    if (status != URSATZ_OK) {
        return status;
    }
    const char *rule = NULL;
    const char *action = NULL;
    bool bad = false;
    TAKE(workshop_param_require_string(call->params, "rule", &rule, error) ? URSATZ_OK : URSATZ_ERR_REQUEST);
    TAKE(workshop_param_require_string(call->params, "action", &action, error) ? URSATZ_OK : URSATZ_ERR_REQUEST);
    const char *note = workshop_param_string(call->params, "note", &bad);
    const cJSON *weight = cJSON_GetObjectItemCaseSensitive(call->params, "weight");
    bool clear = strcmp(action, "clear") == 0;
    if (!clear) {
        TAKE(workshop_settings_override_check(rule, action, weight, "/params/rule", error));
    } else if (!workshop_settings_rule_find(rule)) {
        TAKE(workshop_settings_override_check(rule, "off", NULL, "/params/rule", error));
    }
    /* one override to a rule: a new one replaces the old */
    cJSON *list = cJSON_GetObjectItemCaseSensitive(scope.file, "overrides");
    if (!cJSON_IsArray(list)) {
        cJSON_DeleteItemFromObjectCaseSensitive(scope.file, "overrides");
        list = cJSON_AddArrayToObject(scope.file, "overrides");
    }
    int index = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, list) {
        if (strcmp(common_json_string(entry, "rule"), rule) == 0) {
            cJSON_DeleteItemFromArray(list, index);
            break;
        }
        index++;
    }
    if (!clear) {
        char now[32];
        common_utc_now(now);
        cJSON *record = cJSON_CreateObject();
        bool ok = record && cJSON_AddStringToObject(record, "rule", rule) && cJSON_AddStringToObject(record, "action", action) &&
                  (strcmp(action, "reweight") != 0 || cJSON_AddNumberToObject(record, "weight", weight->valuedouble)) &&
                  cJSON_AddStringToObject(record, "note", note ? note : "") && cJSON_AddStringToObject(record, "actor", call->actor) &&
                  cJSON_AddStringToObject(record, "at", now) && cJSON_AddItemToArray(list, record);
        if (!ok) {
            cJSON_Delete(record);
            cJSON_Delete(scope.file);
            return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
    }
    if (cJSON_GetArraySize(list) == 0) {
        cJSON_DeleteItemFromObjectCaseSensitive(scope.file, "overrides");
    }
    return save_scope(call, reply, &scope, error);
}

/* settings.switch: scope, off. */
WORKSHOP_HANDLER(workshop_c_settings_switch) {
    Scope scope;
    UrsatzStatus status = open_scope(call, KIND(WORKSHOP_SCOPE_PROJECT) | KIND(WORKSHOP_SCOPE_ITEM), "switches go on a project or an item", &scope, error);
    if (status != URSATZ_OK) {
        return status;
    }
    const cJSON *off = cJSON_GetObjectItemCaseSensitive(call->params, "off");
    cJSON *list = cJSON_CreateArray();
    const char *const words[] = {"style", "taste", "project"};
    for (size_t w = 0; list && w < 3; w++) {
        const cJSON *word;
        bool named = false;
        cJSON_ArrayForEach(word, off) {
            named = named || (cJSON_IsString(word) && strcmp(word->valuestring, words[w]) == 0);
        }
        cJSON *text = named ? cJSON_CreateString(words[w]) : NULL;
        if (named && (!text || !cJSON_AddItemToArray(list, text))) {
            cJSON_Delete(text);
            cJSON_Delete(list);
            list = NULL;
        }
    }
    int index = 0;
    const cJSON *word;
    cJSON_ArrayForEach(word, off) {
        if (list && (!cJSON_IsString(word) || (strcmp(word->valuestring, "style") != 0 && strcmp(word->valuestring, "taste") != 0 && strcmp(word->valuestring, "project") != 0))) {
            char path[64];
            snprintf(path, sizeof path, "/params/off/%d", index);
            cJSON_Delete(list);
            cJSON_Delete(scope.file);
            return workshop_call_fail(error, URSATZ_ERR_REQUEST, path, "a switch is style, taste or project");
        }
        index++;
    }
    if (!list) {
        cJSON_Delete(scope.file);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    cJSON_DeleteItemFromObjectCaseSensitive(scope.file, "off");
    if (cJSON_GetArraySize(list) > 0) {
        cJSON_AddItemToObject(scope.file, "off", list);
    } else {
        cJSON_Delete(list);
    }
    UrsatzError failure = {{0}, {0}};
    status = workshop_scope_check(scope.file, scope.kind, "/params", &failure);
    if (status != URSATZ_OK) {
        cJSON_Delete(scope.file);
        return workshop_call_fail(error, status, "/params/off", "%s", failure.message);
    }
    return save_scope(call, reply, &scope, error);
}

/* settings.use-taste: scope, taste (absent or null clears). */
WORKSHOP_HANDLER(workshop_c_settings_use_taste) {
    Scope scope;
    UrsatzStatus status = open_scope(call, KIND(WORKSHOP_SCOPE_STYLE) | KIND(WORKSHOP_SCOPE_PROJECT) | KIND(WORKSHOP_SCOPE_ITEM), "a taste profile does not name a taste profile", &scope, error);
    if (status != URSATZ_OK) {
        return status;
    }
    bool bad = false;
    const char *taste = workshop_param_string(call->params, "taste", &bad);
    cJSON_DeleteItemFromObjectCaseSensitive(scope.file, "taste");
    if (taste && taste[0]) {
        if (!workshop_layout_id_ok(taste)) {
            cJSON_Delete(scope.file);
            return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/taste", "a taste profile's name is 1 to 64 letters, digits, hyphens and underscores");
        }
        cJSON *text = cJSON_CreateString(taste);
        if (!text || !cJSON_AddItemToObject(scope.file, "taste", text)) {
            cJSON_Delete(text);
            cJSON_Delete(scope.file);
            return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
    }
    return save_scope(call, reply, &scope, error);
}
