#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>
#include <ursatz_workshop/workshop_api.h>

#include "items/formats.h"
#include "items/ids.h"
#include "items/layout.h"
#include "items/txn.h"
#include "ratings/ratings.h"

/*
 * The critic's judgments of an item and her verdicts on them (R-723): ratings/critiques/<item id>.json, format ursatz-item-critiques 1. A critique
 * is one candidate of a critique response recorded against a version of the item, its observations each with an id (obs-...) she can give a
 * verdict on (agree or disagree, vrd-...). Every write is one transaction whose target is the item id, so `ursatz history item:<id>` lists it.
 */

static UrsatzStatus refuse(UrsatzError *error, UrsatzStatus status, const char *path, const char *format, ...) __attribute__((format(printf, 4, 5)));

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

static UrsatzStatus no_memory(UrsatzError *error) {
    return refuse(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

static bool add_text(cJSON *object, const char *name, const char *text) {
    return cJSON_AddStringToObject(object, name, text ? text : "") != NULL;
}

UrsatzStatus workshop_ratings_critiques(const char *item_id, cJSON **out, UrsatzError *error) {
    *out = NULL;
    char path[256];
    if (!workshop_layout_item_critiques_file(item_id, path, sizeof path)) {
        return refuse(error, URSATZ_ERR_NOT_FOUND, "/params/item", "there is no item '%s'", item_id ? item_id : "");
    }
    if (common_path_exists(path)) {
        return workshop_format_read(path, WORKSHOP_FORMAT_ITEM_CRITIQUES, true, out, error);
    }
    cJSON *empty = cJSON_CreateObject();
    if (!empty || !add_text(empty, "item_id", item_id) || !cJSON_AddArrayToObject(empty, "critiques") || !cJSON_AddArrayToObject(empty, "verdicts")) {
        cJSON_Delete(empty);
        return no_memory(error);
    }
    *out = empty;
    return URSATZ_OK;
}

/* Writes the item's file in one transaction whose target is the item and whose command is `command`; the object stays the caller's. */
static UrsatzStatus save(const char *command, const char *item_id, cJSON *file, UrsatzError *error) {
    char path[256];
    if (!workshop_layout_item_critiques_file(item_id, path, sizeof path)) {
        return refuse(error, URSATZ_ERR_NOT_FOUND, "/params/item", "there is no item '%s'", item_id);
    }
    UrsatzStatus status = common_dir_make(WORKSHOP_DIR_ITEM_CRITIQUES, error);
    if (status != URSATZ_OK) {
        return status;
    }
    WorkshopTxn *txn = workshop_txn_begin(NULL, command, item_id, 0);
    if (!txn) {
        return refuse(error, URSATZ_ERR_IO, "", "the journal cannot be read");
    }
    status = workshop_txn_stage_json(txn, path, WORKSHOP_FORMAT_ITEM_CRITIQUES, file);
    if (status != URSATZ_OK) {
        workshop_txn_abort(txn);
        return refuse(error, status, "", "the critique cannot be saved");
    }
    UrsatzError failure = {{0}, {0}};
    status = workshop_txn_commit(txn, &failure);
    return status == URSATZ_OK ? URSATZ_OK : refuse(error, status, failure.path, "%s", failure.message[0] ? failure.message : "the critique cannot be saved");
}

/* The record of one candidate of a critique response: its ids made, the rest as the response gave it. NULL on allocation failure. */
static cJSON *record_of(int version, const cJSON *candidate, const char *note, const char *actor) {
    char now[32];
    common_utc_now(now);
    char *id = workshop_new_id("crt");
    cJSON *record = id ? cJSON_CreateObject() : NULL;
    cJSON *observations = record ? cJSON_CreateArray() : NULL;
    bool ok = record && observations && add_text(record, "id", id) && cJSON_AddNumberToObject(record, "version", version) && add_text(record, "at", now) &&
              add_text(record, "actor", actor) && add_text(record, "note", note) &&
              cJSON_AddItemToObject(record, "score", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(candidate, "score"), true)) &&
              cJSON_AddItemToObject(record, "evaluators", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(candidate, "evaluators"), true));
    free(id);
    const cJSON *observation;
    cJSON_ArrayForEach(observation, cJSON_GetObjectItemCaseSensitive(candidate, "observations")) {
        char *observation_id = ok ? workshop_new_id("obs") : NULL;
        cJSON *copy = observation_id ? cJSON_CreateObject() : NULL;
        ok = copy && cJSON_AddItemToArray(observations, copy) && add_text(copy, "id", observation_id) &&
             cJSON_AddItemToObject(copy, "rule", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(observation, "rule"), true)) &&
             cJSON_AddItemToObject(copy, "scored", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(observation, "scored"), true)) &&
             cJSON_AddItemToObject(copy, "bar", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(observation, "bar"), true)) &&
             cJSON_AddItemToObject(copy, "beat", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(observation, "beat"), true)) &&
             cJSON_AddItemToObject(copy, "message", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(observation, "message"), true));
        if (!ok && copy && cJSON_GetArraySize(observations) == 0) {
            cJSON_Delete(copy);
        }
        free(observation_id);
    }
    ok = ok && cJSON_AddItemToObject(record, "observations", observations);
    if (!ok) {
        if (observations && record && !cJSON_GetObjectItemCaseSensitive(record, "observations")) {
            cJSON_Delete(observations);
        }
        cJSON_Delete(record);
        return NULL;
    }
    return record;
}

UrsatzStatus workshop_ratings_critique_add(const char *item_id, int version, const cJSON *candidate, const char *note, const char *actor, cJSON **out,
                                           UrsatzError *error) {
    *out = NULL;
    if (version < 1) {
        return refuse(error, URSATZ_ERR_REQUEST, "/params/version", "a version is counted from 1");
    }
    if (!cJSON_IsObject(candidate) || !cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(candidate, "score")) ||
        !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(candidate, "evaluators")) || !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(candidate, "observations"))) {
        return refuse(error, URSATZ_ERR_INTERNAL, "", "the critic's answer has no score, evaluators and observations");
    }
    cJSON *file = NULL;
    UrsatzStatus status = workshop_ratings_critiques(item_id, &file, error);
    if (status != URSATZ_OK) {
        return status;
    }
    cJSON *record = record_of(version, candidate, note, actor);
    if (!record || !cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(file, "critiques"), record)) {
        cJSON_Delete(record);
        cJSON_Delete(file);
        return no_memory(error);
    }
    status = save("ratings.critique", item_id, file, error);
    cJSON *copy = status == URSATZ_OK ? cJSON_Duplicate(record, true) : NULL;
    cJSON_Delete(file);
    if (status != URSATZ_OK) {
        return status;
    }
    if (!copy) {
        return no_memory(error);
    }
    *out = copy;
    return URSATZ_OK;
}

/* Whether any critique of the file holds the observation. */
static bool holds_observation(const cJSON *file, const char *id) {
    const cJSON *critique;
    cJSON_ArrayForEach(critique, cJSON_GetObjectItemCaseSensitive(file, "critiques")) {
        const cJSON *observation;
        cJSON_ArrayForEach(observation, cJSON_GetObjectItemCaseSensitive(critique, "observations")) {
            if (strcmp(common_json_string(observation, "id"), id) == 0) {
                return true;
            }
        }
    }
    return false;
}

UrsatzStatus workshop_ratings_verdict(const char *item_id, const char *observation, const char *verdict, const char *note, const char *actor, char **out_id,
                                      UrsatzError *error) {
    *out_id = NULL;
    if (!observation || !observation[0]) {
        return refuse(error, URSATZ_ERR_REQUEST, "/params/observation", "a verdict is on an observation of this item's critiques");
    }
    if (!verdict || (strcmp(verdict, "agree") != 0 && strcmp(verdict, "disagree") != 0)) {
        return refuse(error, URSATZ_ERR_REQUEST, "/params/verdict", "a verdict is agree or disagree");
    }
    cJSON *file = NULL;
    UrsatzStatus status = workshop_ratings_critiques(item_id, &file, error);
    if (status != URSATZ_OK) {
        return status;
    }
    if (!holds_observation(file, observation)) {
        cJSON_Delete(file);
        return refuse(error, URSATZ_ERR_NOT_FOUND, "/params/observation", "item %s has no observation '%s' in its critiques", item_id, observation);
    }
    char now[32];
    common_utc_now(now);
    char *id = workshop_new_id("vrd");
    cJSON *entry = id ? cJSON_CreateObject() : NULL;
    bool ok = entry && add_text(entry, "id", id) && add_text(entry, "observation", observation) && add_text(entry, "verdict", verdict) &&
              add_text(entry, "note", note) && add_text(entry, "actor", actor) && add_text(entry, "at", now) &&
              cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(file, "verdicts"), entry);
    if (!ok) {
        cJSON_Delete(entry);
        free(id);
        cJSON_Delete(file);
        return no_memory(error);
    }
    status = save("ratings.verdict", item_id, file, error);
    cJSON_Delete(file);
    if (status != URSATZ_OK) {
        free(id);
        return status;
    }
    *out_id = id;
    return URSATZ_OK;
}
