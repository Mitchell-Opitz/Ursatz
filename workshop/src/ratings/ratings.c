#include "ratings/ratings.h"

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
#include "items/store.h"
#include "items/txn.h"

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

/* --- files ------------------------------------------------------------------------------------------------------------------------ */

static bool session_path(const char *id, char *path, size_t size) {
    return workshop_layout_session_file(id, path, size);
}

/* Writes the object as the file at path in one transaction; the object stays the caller's. */
static UrsatzStatus save(const char *actor, const char *folder, const char *path, const char *format, cJSON *object, UrsatzError *error) {
    UrsatzStatus status = common_dir_make(folder, error);
    if (status != URSATZ_OK) {
        return status;
    }
    WorkshopTxn *txn = workshop_txn_begin(NULL, "ratings", actor, "", 0);
    if (!txn) {
        return refuse(error, URSATZ_ERR_IO, "", "the journal cannot be read");
    }
    status = workshop_txn_stage_json(txn, path, format, object);
    if (status != URSATZ_OK) {
        workshop_txn_abort(txn);
        return refuse(error, status, "", "the ratings cannot be saved");
    }
    UrsatzError failure = {{0}, {0}};
    status = workshop_txn_commit(txn, &failure);
    return status == URSATZ_OK ? URSATZ_OK : refuse(error, status, failure.path, "%s", failure.message[0] ? failure.message : "the ratings cannot be saved");
}

static UrsatzStatus save_session(const char *actor, const char *id, cJSON *session, UrsatzError *error) {
    char path[256];
    if (!session_path(id, path, sizeof path)) {
        return refuse(error, URSATZ_ERR_REQUEST, "/params/session", "a session id is 1 to 64 letters, digits, hyphens and underscores");
    }
    return save(actor, WORKSHOP_DIR_SESSIONS, path, WORKSHOP_FORMAT_LISTENING_SESSION, session, error);
}

UrsatzStatus workshop_ratings_session_read(const char *id, cJSON **out, UrsatzError *error) {
    char path[256];
    *out = NULL;
    if (!session_path(id, path, sizeof path)) {
        return refuse(error, URSATZ_ERR_NOT_FOUND, "/params/session", "there is no session '%s'", id ? id : "");
    }
    if (!common_path_exists(path)) {
        return refuse(error, URSATZ_ERR_NOT_FOUND, "/params/session", "there is no session '%s'", id);
    }
    return workshop_format_read(path, WORKSHOP_FORMAT_LISTENING_SESSION, true, out, error);
}

static void now_text(char *out) {
    common_utc_now(out);
}

static bool add_text(cJSON *object, const char *name, const char *text) {
    return cJSON_AddStringToObject(object, name, text ? text : "") != NULL;
}

/* --- sessions ---------------------------------------------------------------------------------------------------------------------- */

UrsatzStatus workshop_ratings_session_create(const char *what, const char *taste, const char *style, const char *note, const char *actor, cJSON **out,
                                             UrsatzError *error) {
    *out = NULL;
    char *id = workshop_new_id("ses");
    char now[32];
    now_text(now);
    cJSON *session = id ? cJSON_CreateObject() : NULL;
    bool ok = session && add_text(session, "id", id) && add_text(session, "created_at", now) && add_text(session, "actor", actor) &&
              add_text(session, "what", what) && add_text(session, "taste", taste && taste[0] ? taste : "none") &&
              (style && style[0] ? add_text(session, "style", style) : cJSON_AddNullToObject(session, "style") != NULL) && add_text(session, "note", note) &&
              cJSON_AddArrayToObject(session, "batches") && cJSON_AddArrayToObject(session, "ratings") && cJSON_AddArrayToObject(session, "picks");
    UrsatzStatus status = ok ? save_session(actor, id, session, error) : no_memory(error);
    free(id);
    if (status != URSATZ_OK) {
        cJSON_Delete(session);
        return status;
    }
    *out = session;
    return URSATZ_OK;
}

UrsatzStatus workshop_ratings_sessions(cJSON **out, UrsatzError *error) {
    *out = NULL;
    cJSON *list = cJSON_CreateArray();
    CommonNames names;
    memset(&names, 0, sizeof(names));
    if (!list) {
        return no_memory(error);
    }
    if (common_dir_list(WORKSHOP_DIR_SESSIONS, &names, NULL) == URSATZ_OK) {
        /* newest first: the ids are random, so the created_at decides; the names are read, then inserted in order */
        for (size_t i = 0; i < names.count; i++) {
            size_t length = strlen(names.items[i]);
            if (length <= 5 || strcmp(names.items[i] + length - 5, ".json") != 0 || names.items[i][0] == '.') {
                continue;
            }
            char id[96];
            snprintf(id, sizeof id, "%.*s", (int)(length - 5), names.items[i]);
            cJSON *session = NULL;
            if (workshop_ratings_session_read(id, &session, NULL) != URSATZ_OK) {
                continue;
            }
            cJSON *row = cJSON_CreateObject();
            bool ok = row && add_text(row, "id", id) && add_text(row, "created_at", common_json_string(session, "created_at")) && add_text(row, "what", common_json_string(session, "what")) &&
                      add_text(row, "taste", common_json_string(session, "taste")) &&
                      cJSON_AddNumberToObject(row, "batches", cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(session, "batches"))) &&
                      cJSON_AddNumberToObject(row, "ratings", cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(session, "ratings"))) &&
                      cJSON_AddNumberToObject(row, "picks", cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(session, "picks")));
            cJSON_Delete(session);
            /* insert before the first row that is older or equal */
            int at = 0;
            const cJSON *other;
            cJSON_ArrayForEach(other, list) {
                if (strcmp(common_json_string(row, "created_at"), common_json_string(other, "created_at")) >= 0) {
                    break;
                }
                at++;
            }
            if (!ok || !cJSON_InsertItemInArray(list, at, row)) {
                cJSON_Delete(row);
                common_names_free(&names);
                cJSON_Delete(list);
                return no_memory(error);
            }
        }
    }
    common_names_free(&names);
    *out = list;
    return URSATZ_OK;
}

/* The batch of the session for (round, batch), or NULL. */
static const cJSON *find_batch(const cJSON *session, int round, const char *batch) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(session, "batches")) {
        const cJSON *number = cJSON_GetObjectItemCaseSensitive(entry, "round");
        if (cJSON_IsNumber(number) && number->valueint == round && strcmp(common_json_string(entry, "batch"), batch) == 0) {
            return entry;
        }
    }
    return NULL;
}

static bool batch_name_ok(const char *batch) {
    size_t length = batch ? strlen(batch) : 0;
    if (length < 1 || length > 8) {
        return false;
    }
    for (const char *c = batch; *c; c++) {
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9'))) {
            return false;
        }
    }
    return true;
}

UrsatzStatus workshop_ratings_batch(const char *session_id, int round, const char *batch, const cJSON *samples, const char *actor, cJSON **out, UrsatzError *error) {
    *out = NULL;
    if (!batch_name_ok(batch)) {
        return refuse(error, URSATZ_ERR_REQUEST, "/params/batch", "a batch is named with 1 to 8 letters and digits");
    }
    if (round < 1 || round > 10000) {
        return refuse(error, URSATZ_ERR_REQUEST, "/params/round", "round must be from 1 to 10000");
    }
    if (!cJSON_IsArray(samples) || cJSON_GetArraySize(samples) > 64) {
        return refuse(error, URSATZ_ERR_REQUEST, "/params/samples", "samples is a list of up to 64 samples");
    }
    int index = 0;
    const cJSON *sample;
    cJSON_ArrayForEach(sample, samples) {
        static const char *const kMembers[] = {"index", "seed", "status", "files", "settings"};
        for (size_t m = 0; m < sizeof(kMembers) / sizeof(kMembers[0]); m++) {
            if (!cJSON_IsObject(sample) || !cJSON_HasObjectItem(sample, kMembers[m])) {
                char path[64];
                snprintf(path, sizeof path, "/params/samples/%d", index);
                return refuse(error, URSATZ_ERR_REQUEST, path, "a sample has index, seed, status, files and settings");
            }
        }
        index++;
    }
    cJSON *session = NULL;
    UrsatzStatus status = workshop_ratings_session_read(session_id, &session, error);
    if (status != URSATZ_OK) {
        return status;
    }
    if (find_batch(session, round, batch)) {
        cJSON_Delete(session);
        return refuse(error, URSATZ_ERR_REQUEST, "/params/batch", "batch %s of round %d is recorded", batch, round);
    }
    char now[32];
    now_text(now);
    cJSON *entry = cJSON_CreateObject();
    bool ok = entry && cJSON_AddNumberToObject(entry, "round", round) && add_text(entry, "batch", batch) && add_text(entry, "at", now) &&
              cJSON_AddItemToObject(entry, "samples", cJSON_Duplicate(samples, true)) &&
              cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(session, "batches"), entry);
    if (!ok) {
        cJSON_Delete(entry);
        cJSON_Delete(session);
        return no_memory(error);
    }
    status = save_session(actor, session_id, session, error);
    if (status != URSATZ_OK) {
        cJSON_Delete(session);
        return status;
    }
    *out = session;
    return URSATZ_OK;
}

UrsatzStatus workshop_ratings_rate(const char *session_id, int round, const char *batch, long long seed, int rating, const char *note, const char *actor,
                                   char **out_id, UrsatzError *error) {
    *out_id = NULL;
    cJSON *session = NULL;
    UrsatzStatus status = workshop_ratings_session_read(session_id, &session, error);
    if (status != URSATZ_OK) {
        return status;
    }
    const cJSON *recorded = find_batch(session, round, batch);
    const cJSON *found = NULL;
    const cJSON *sample;
    const cJSON *recorded_samples = recorded ? cJSON_GetObjectItemCaseSensitive(recorded, "samples") : NULL;
    cJSON_ArrayForEach(sample, recorded_samples) {
        const cJSON *number = cJSON_GetObjectItemCaseSensitive(sample, "seed");
        if (cJSON_IsNumber(number) && (long long)number->valuedouble == seed) {
            found = sample;
        }
    }
    if (!found) {
        cJSON_Delete(session);
        return refuse(error, URSATZ_ERR_REQUEST, "/params/seed", "seed %lld is not recorded in batch %s of round %d", seed, batch, round);
    }
    if (strcmp(common_json_string(found, "status"), "ok") != 0) {
        cJSON_Delete(session);
        return refuse(error, URSATZ_ERR_REQUEST, "/params/seed", "seed %lld of batch %s was not made (%s)", seed, batch, common_json_string(found, "status"));
    }
    if (rating < 1 || rating > 5) {
        cJSON_Delete(session);
        return refuse(error, URSATZ_ERR_REQUEST, "/params/rating", "rating must be from 1 to 5");
    }
    cJSON *list = cJSON_GetObjectItemCaseSensitive(session, "ratings");
    int at = 0;
    const cJSON *held;
    cJSON_ArrayForEach(held, list) {
        const cJSON *round_item = cJSON_GetObjectItemCaseSensitive(held, "round");
        const cJSON *seed_item = cJSON_GetObjectItemCaseSensitive(held, "seed");
        if (cJSON_IsNumber(round_item) && round_item->valueint == round && strcmp(common_json_string(held, "batch"), batch) == 0 && cJSON_IsNumber(seed_item) &&
            (long long)seed_item->valuedouble == seed) {
            cJSON_DeleteItemFromArray(list, at);
            break;
        }
        at++;
    }
    char *id = workshop_new_id("rat");
    char now[32];
    now_text(now);
    cJSON *entry = id ? cJSON_CreateObject() : NULL;
    bool ok = entry && add_text(entry, "id", id) && cJSON_AddNumberToObject(entry, "round", round) && add_text(entry, "batch", batch) &&
              cJSON_AddNumberToObject(entry, "seed", (double)seed) && cJSON_AddNumberToObject(entry, "rating", rating) && add_text(entry, "note", note) &&
              add_text(entry, "actor", actor) && add_text(entry, "at", now) && cJSON_AddItemToArray(list, entry);
    if (!ok) {
        cJSON_Delete(entry);
        free(id);
        cJSON_Delete(session);
        return no_memory(error);
    }
    status = save_session(actor, session_id, session, error);
    cJSON_Delete(session);
    if (status != URSATZ_OK) {
        free(id);
        return status;
    }
    *out_id = id;
    return URSATZ_OK;
}

UrsatzStatus workshop_ratings_pick(const char *session_id, int round, long long seed, const char *better, const char *note, const char *actor, char **out_id,
                                   UrsatzError *error) {
    *out_id = NULL;
    cJSON *session = NULL;
    UrsatzStatus status = workshop_ratings_session_read(session_id, &session, error);
    if (status != URSATZ_OK) {
        return status;
    }
    if (strcmp(better, "same") != 0 && !find_batch(session, round, better)) {
        cJSON_Delete(session);
        return refuse(error, URSATZ_ERR_REQUEST, "/params/better", "better names a batch recorded for round %d, or same: there is no batch %s", round, better);
    }
    char *id = workshop_new_id("pck");
    char now[32];
    now_text(now);
    cJSON *entry = id ? cJSON_CreateObject() : NULL;
    bool ok = entry && add_text(entry, "id", id) && cJSON_AddNumberToObject(entry, "round", round) && cJSON_AddNumberToObject(entry, "seed", (double)seed) &&
              add_text(entry, "better", better) && add_text(entry, "note", note) && add_text(entry, "actor", actor) && add_text(entry, "at", now) &&
              cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(session, "picks"), entry);
    if (!ok) {
        cJSON_Delete(entry);
        free(id);
        cJSON_Delete(session);
        return no_memory(error);
    }
    status = save_session(actor, session_id, session, error);
    cJSON_Delete(session);
    if (status != URSATZ_OK) {
        free(id);
        return status;
    }
    *out_id = id;
    return URSATZ_OK;
}

/* --- ratings of item versions ------------------------------------------------------------------------------------------------------ */

UrsatzStatus workshop_ratings_item(const char *item_id, cJSON **out, UrsatzError *error) {
    *out = NULL;
    char path[256];
    if (!workshop_layout_item_ratings_file(item_id, path, sizeof path)) {
        return refuse(error, URSATZ_ERR_NOT_FOUND, "/params/item_id", "there is no item '%s'", item_id ? item_id : "");
    }
    if (common_path_exists(path)) {
        return workshop_format_read(path, WORKSHOP_FORMAT_ITEM_RATINGS, true, out, error);
    }
    cJSON *empty = cJSON_CreateObject();
    if (!empty || !add_text(empty, "item_id", item_id) || !cJSON_AddArrayToObject(empty, "ratings")) {
        cJSON_Delete(empty);
        return no_memory(error);
    }
    *out = empty;
    return URSATZ_OK;
}

UrsatzStatus workshop_ratings_rate_version(const char *item_id, int version, int rating, const char *note, const char *actor, char **out_id, UrsatzError *error) {
    *out_id = NULL;
    cJSON *meta = NULL;
    if (workshop_material_item_get(item_id, &meta, NULL) != URSATZ_OK) {
        return refuse(error, URSATZ_ERR_NOT_FOUND, "/target", "there is no item '%s'", item_id);
    }
    int current = cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(meta, "current_version")) ? cJSON_GetObjectItemCaseSensitive(meta, "current_version")->valueint : 0;
    cJSON_Delete(meta);
    if (version < 1 || version > current) {
        return refuse(error, URSATZ_ERR_NOT_FOUND, "/params/version", "item %s has no version %d", item_id, version);
    }
    if (rating < 1 || rating > 5) {
        return refuse(error, URSATZ_ERR_REQUEST, "/params/rating", "rating must be from 1 to 5");
    }
    cJSON *file = NULL;
    UrsatzStatus status = workshop_ratings_item(item_id, &file, error);
    if (status != URSATZ_OK) {
        return status;
    }
    char *id = workshop_new_id("rat");
    char now[32];
    now_text(now);
    cJSON *entry = id ? cJSON_CreateObject() : NULL;
    bool ok = entry && add_text(entry, "id", id) && cJSON_AddNumberToObject(entry, "version", version) && cJSON_AddNumberToObject(entry, "rating", rating) &&
              add_text(entry, "note", note) && add_text(entry, "actor", actor) && add_text(entry, "at", now) &&
              cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(file, "ratings"), entry);
    if (!ok) {
        cJSON_Delete(entry);
        free(id);
        cJSON_Delete(file);
        return no_memory(error);
    }
    char path[256];
    workshop_layout_item_ratings_file(item_id, path, sizeof path);
    status = save(actor, WORKSHOP_DIR_ITEM_RATINGS, path, WORKSHOP_FORMAT_ITEM_RATINGS, file, error);
    cJSON_Delete(file);
    if (status != URSATZ_OK) {
        free(id);
        return status;
    }
    *out_id = id;
    return URSATZ_OK;
}

/* --- what is recorded --------------------------------------------------------------------------------------------------------------- */

static bool list_has_id(const cJSON *list, const char *id) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, list) {
        if (strcmp(common_json_string(entry, "id"), id) == 0) {
            return true;
        }
    }
    return false;
}

static bool folder_has_id(const char *folder, const char *id, bool sessions) {
    CommonNames names;
    memset(&names, 0, sizeof(names));
    bool found = false;
    if (common_dir_list(folder, &names, NULL) != URSATZ_OK) {
        common_names_free(&names);
        return false;
    }
    for (size_t i = 0; !found && i < names.count; i++) {
        size_t length = strlen(names.items[i]);
        if (length <= 5 || strcmp(names.items[i] + length - 5, ".json") != 0 || names.items[i][0] == '.') {
            continue;
        }
        char path[320];
        snprintf(path, sizeof path, "%s/%s", folder, names.items[i]);
        cJSON *file = NULL;
        if (workshop_format_read(path, sessions ? WORKSHOP_FORMAT_LISTENING_SESSION : WORKSHOP_FORMAT_ITEM_RATINGS, true, &file, NULL) != URSATZ_OK) {
            continue;
        }
        found = list_has_id(cJSON_GetObjectItemCaseSensitive(file, "ratings"), id) || (sessions && list_has_id(cJSON_GetObjectItemCaseSensitive(file, "picks"), id));
        cJSON_Delete(file);
    }
    common_names_free(&names);
    return found;
}

bool workshop_ratings_id_recorded(const char *id) {
    return id && id[0] && (folder_has_id(WORKSHOP_DIR_SESSIONS, id, true) || folder_has_id(WORKSHOP_DIR_ITEM_RATINGS, id, false));
}
