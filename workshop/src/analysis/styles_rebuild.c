#include "styles.h"
#include "items/formats.h"
#include "items/layout.h"
#include "items/txn.h"
#include "base/last_error.h"
#include "items/lock.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <cJSON.h>
#include <ursatz_analyzer/analyzer_api.h>
#include <theory/style/style_compare.h>
#include <theory/style/style_features.h>
#include <theory/style/style_labels.h>
#include <theory/style/style_model.h>
#include <theory/style/style_model_file.h>
#include <theory/style/style_profile.h>
#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>
#include "analysis.h"
#include "items/collections.h"
#include "items/ids.h"
#include "items/library_metadata.h"
#include "analysis/style_model_io.h"
#include "analysis/styles_internal.h"

/* --- stored styles -------------------------------------------------------- */

void styles_free_strings(char **items, size_t count) {
    for (size_t i = 0; items && i < count; i++) {
        free(items[i]);
    }
    free(items);
}

bool styles_style_dir_path(const char *id, char *out, size_t out_size) {
    return workshop_collection_is_valid_id(id) && workshop_layout_style_dir(id, out, out_size);
}

/* A copy of a string member, "" when it is absent or not a string. */

/* Copies the string members of an array; *out is NULL when there are none. */
static bool string_array_copy(const cJSON *array, char ***out, size_t *out_count) {
    *out = NULL;
    *out_count = 0;
    int length = cJSON_IsArray(array) ? cJSON_GetArraySize(array) : 0;
    if (length <= 0) {
        return true;
    }
    char **items = calloc((size_t)length, sizeof(char *));
    if (!items) {
        return false;
    }
    size_t count = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        if (!cJSON_IsString(item) || !item->valuestring) {
            continue;
        }
        items[count] = common_strdup(item->valuestring);
        if (!items[count]) {
            styles_free_strings(items, count);
            return false;
        }
        count++;
    }
    if (count == 0) {
        free(items);
        return true;
    }
    *out = items;
    *out_count = count;
    return true;
}

static WorkshopStyleSourceKind source_kind_from_label(const char *label) {
    if (label && strcmp(label, "collection") == 0) {
        return WORKSHOP_STYLE_SOURCE_COLLECTION;
    }
    if (label && strcmp(label, "pieces") == 0) {
        return WORKSHOP_STYLE_SOURCE_PIECES;
    }
    if (label && strcmp(label, "brief") == 0) {
        return WORKSHOP_STYLE_SOURCE_BRIEF;
    }
    return WORKSHOP_STYLE_SOURCE_FILTER;
}

/* Reads the stored brief into out (owned strings). false on allocation failure. */
static bool load_brief(const cJSON *json, WorkshopStyleBrief *out) {
    memset(out, 0, sizeof(*out));
    const cJSON *scopes = cJSON_GetObjectItemCaseSensitive(json, "scopes");
    int length = cJSON_IsArray(scopes) ? cJSON_GetArraySize(scopes) : 0;
    if (length > 0) {
        out->scopes = calloc((size_t)length, sizeof(*out->scopes));
        if (!out->scopes) {
            return false;
        }
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, scopes) {
            const cJSON *kind = cJSON_GetObjectItemCaseSensitive(entry, "kind");
            const cJSON *filter = cJSON_GetObjectItemCaseSensitive(entry, "filter");
            WorkshopLinkScope *scope = &out->scopes[out->scope_count];
            if (!cJSON_IsString(kind) || !styles_scope_kind_from_label(kind->valuestring, &scope->kind)) {
                continue;
            }
            char *fields[6];
            fields[0] = common_json_string_copy(entry, "collection_id");
            fields[1] = common_json_string_copy(filter, "composer");
            fields[2] = common_json_string_copy(filter, "source");
            fields[3] = common_json_string_copy(filter, "tag");
            fields[4] = common_json_string_copy(entry, "subject_id");
            fields[5] = common_json_string_copy(entry, "project_id");
            bool filled = true;
            for (int i = 0; i < 6; i++) {
                filled = filled && fields[i] != NULL;
            }
            if (!filled) {
                for (int i = 0; i < 6; i++) {
                    free(fields[i]);
                }
                workshop_style_brief_free(out);
                return false;
            }
            scope->collection_id = fields[0];
            scope->composer = fields[1];
            scope->source = fields[2];
            scope->tag = fields[3];
            scope->subject_id = fields[4];
            scope->project_id = fields[5];
            out->scope_count++;
        }
    }
    if (!string_array_copy(cJSON_GetObjectItemCaseSensitive(json, "piece_ids"), &out->piece_ids, &out->piece_count) ||
        !string_array_copy(cJSON_GetObjectItemCaseSensitive(json, "style_ids"), &out->style_ids, &out->style_count)) {
        workshop_style_brief_free(out);
        return false;
    }
    return true;
}

static bool json_string_or_null(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return !item || cJSON_IsNull(item) || cJSON_IsString(item);
}

/* A scope as the links routes take it: a known kind and string or null members. */
static bool scope_json_valid(const cJSON *json) {
    WorkshopLinkScopeKind kind;
    const cJSON *label = cJSON_GetObjectItemCaseSensitive(json, "kind");
    const cJSON *filter = cJSON_GetObjectItemCaseSensitive(json, "filter");
    return cJSON_IsObject(json) && cJSON_IsString(label) && styles_scope_kind_from_label(label->valuestring, &kind) &&
           json_string_or_null(json, "collection_id") && json_string_or_null(json, "subject_id") &&
           json_string_or_null(json, "project_id") &&
           (!filter || cJSON_IsNull(filter) ||
            (cJSON_IsObject(filter) && json_string_or_null(filter, "composer") && json_string_or_null(filter, "source") &&
             json_string_or_null(filter, "tag")));
}

/* An optional array of at most max valid ids. NULL when it holds, else the failure message. */
static const char *brief_ids_failure(const cJSON *json, const char *key, size_t max) {
    const cJSON *array = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!array) {
        return NULL;
    }
    if (!cJSON_IsArray(array)) {
        return "invalid brief";
    }
    if ((size_t)cJSON_GetArraySize(array) > max) {
        return "too many entries in the brief";
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        if (!cJSON_IsString(item) || !workshop_collection_is_valid_id(item->valuestring)) {
            return "invalid brief";
        }
    }
    return NULL;
}

const char *workshop_style_brief_parse(const cJSON *json, WorkshopStyleBrief *out) {
    memset(out, 0, sizeof(*out));
    if (!cJSON_IsObject(json)) {
        return "invalid brief";
    }
    const cJSON *scopes = cJSON_GetObjectItemCaseSensitive(json, "scopes");
    if (scopes) {
        if (!cJSON_IsArray(scopes)) {
            return "invalid brief";
        }
        if ((size_t)cJSON_GetArraySize(scopes) > WORKSHOP_STYLE_BRIEF_MAX_SCOPES) {
            return "too many entries in the brief";
        }
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, scopes) {
            if (!scope_json_valid(entry)) {
                return "invalid scope";
            }
        }
    }
    const char *failure = brief_ids_failure(json, "piece_ids", WORKSHOP_STYLE_BRIEF_MAX_PIECES);
    if (!failure) {
        failure = brief_ids_failure(json, "style_ids", WORKSHOP_STYLE_BRIEF_MAX_STYLES);
    }
    if (failure) {
        return failure;
    }
    if (!load_brief(json, out)) {
        return "out of memory";
    }
    if (out->scope_count + out->piece_count + out->style_count == 0) {
        workshop_style_brief_free(out);
        return "invalid brief";
    }
    return NULL;
}

/* Reads styles/<id>/meta.json: the stored tree, whose "id" is set from the directory name (a copy of it inside the file is not
   trusted). NOT_FOUND when the style is absent. */
UrsatzStatus styles_load_meta(const char *id, cJSON **out) {
    char dir[PATH_BYTES];
    char path[PATH_BYTES + 16];
    *out = NULL;
    if (!styles_style_dir_path(id, dir, sizeof(dir))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
    }
    snprintf(path, sizeof(path), "%s/" WORKSHOP_FILE_STYLE, dir);
    cJSON *meta = NULL;
    workshop_format_read(path, WORKSHOP_FORMAT_STYLE, true, &meta, NULL);
    if (!meta) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
    }
    cJSON_DeleteItemFromObjectCaseSensitive(meta, "id");
    if (!cJSON_AddStringToObject(meta, "id", id)) {
        cJSON_Delete(meta);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out = meta;
    return URSATZ_OK;
}

/* The list entry of a stored style: {"id","name","piece_count","source","updated_at"}; takes members of meta. */
cJSON *styles_entry_of(cJSON *meta) {
    cJSON *entry = cJSON_CreateObject();
    static const char *const kMembers[] = {"id", "name", "piece_count", "source", "updated_at"};
    for (size_t i = 0; entry && i < sizeof(kMembers) / sizeof(kMembers[0]); i++) {
        cJSON *member = cJSON_DetachItemFromObjectCaseSensitive(meta, kMembers[i]);
        if (!member && strcmp(kMembers[i], "piece_count") == 0) {
            member = cJSON_CreateNumber((double)cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(meta, "pieces")));
        }
        if (!member) {
            member = cJSON_CreateString("");
        }
        if (!member || !cJSON_AddItemToObject(entry, kMembers[i], member)) {
            cJSON_Delete(member);
            cJSON_Delete(entry);
            return NULL;
        }
    }
    return entry;
}

/* --- rebuild, delete, model path ------------------------------------------ */

/* The source a stored style was built from, read from its meta.json; the strings point into meta. */
typedef struct {
    cJSON *meta;
    WorkshopStyleBrief brief;
    const char **piece_ids;
    WorkshopStyleSource input;
} StoredSource;

static void stored_source_free(StoredSource *stored) {
    workshop_style_brief_free(&stored->brief);
    free(stored->piece_ids);
    cJSON_Delete(stored->meta);
    memset(stored, 0, sizeof(*stored));
}

static const char *json_text(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static UrsatzStatus stored_source_load(const char *id, StoredSource *stored) {
    memset(stored, 0, sizeof(*stored));
    UrsatzStatus result = styles_load_meta(id, &stored->meta);
    if (result != URSATZ_OK) {
        return result;
    }
    const cJSON *source = cJSON_GetObjectItemCaseSensitive(stored->meta, "source");
    const cJSON *filter = cJSON_GetObjectItemCaseSensitive(source, "filter");
    WorkshopStyleSource *input = &stored->input;
    input->kind = source_kind_from_label(json_text(source, "kind"));
    input->composer = json_text(filter, "composer");
    input->source = json_text(filter, "source");
    input->tag = json_text(filter, "tag");
    input->collection_id = json_text(source, "collection_id");
    const cJSON *ids = cJSON_GetObjectItemCaseSensitive(source, "piece_ids");
    int length = cJSON_IsArray(ids) ? cJSON_GetArraySize(ids) : 0;
    stored->piece_ids = length > 0 ? calloc((size_t)length, sizeof(*stored->piece_ids)) : NULL;
    bool ok = length == 0 || stored->piece_ids != NULL;
    const cJSON *piece = NULL;
    cJSON_ArrayForEach(piece, ids) {
        if (ok && cJSON_IsString(piece)) {
            stored->piece_ids[input->piece_count++] = piece->valuestring;
        }
    }
    input->piece_ids = stored->piece_ids;
    if (ok && input->kind == WORKSHOP_STYLE_SOURCE_BRIEF) {
        ok = load_brief(cJSON_GetObjectItemCaseSensitive(source, "brief"), &stored->brief);
    }
    input->brief = &stored->brief;
    if (!ok) {
        stored_source_free(stored);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_style_rebuild(const char *id, const UrsatzJob *job) {
    LOCK();
    StoredSource stored;
    UrsatzStatus result = stored_source_load(id, &stored);
    UNLOCK();
    if (result != URSATZ_OK) {
        return result;
    }

    NormalizedSource source;
    result = styles_normalized_source_make(&stored.input, &source);
    if (result == URSATZ_OK) {
        Collected collected;
        result = styles_collect_pieces(&source, job, &collected); /* the pieces are read holding no lock */
        if (result == URSATZ_ERR_NOT_FOUND) {
            result = workshop_fail(URSATZ_ERR_UNSATISFIABLE, "source not found");
        }
        if (result == URSATZ_OK && ursatz_job_cancelled(job)) {
            result = URSATZ_ERR_CANCELLED;
        }
        if (result == URSATZ_OK) {
            char dir[PATH_BYTES];
            char now[32];
            common_utc_now(now);
            styles_style_dir_path(id, dir, sizeof(dir));
            const char *name = json_text(stored.meta, "name");
            const char *created_at = json_text(stored.meta, "created_at");
            /* The style's folder is replaced whole: the old one is removed and the new one staged, in one transaction (the outermost
             * lock, then the styles lock). A style deleted meanwhile is not built again. */
            const char *pending = NULL;
            WorkshopTxn *txn = workshop_txn_begin(NULL, "style.rebuild", id, 0);
            LOCK();
            /* The style's settings (settings.json, R-671) are not made by the rebuild: they go into the new folder as they were. */
            char settings_path[512];
            char *kept_settings = NULL;
            size_t kept_size = 0;
            snprintf(settings_path, sizeof(settings_path), "%s/" WORKSHOP_FILE_SETTINGS, dir);
            if (common_path_exists(settings_path)) {
                (void)common_file_read(settings_path, &kept_settings, &kept_size, NULL);
            }
            if (!common_path_is_dir(dir)) {
                result = workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
            } else if (txn && workshop_txn_remove(txn, dir) == URSATZ_OK && workshop_txn_stage_folder(txn, dir, &pending) == URSATZ_OK &&
                       styles_write_style_files(pending, id, name ? name : "", created_at ? created_at : "", now, &source, &collected) &&
                       (!kept_settings || (snprintf(settings_path, sizeof(settings_path), "%s/" WORKSHOP_FILE_SETTINGS, pending) > 0 &&
                                           common_file_write_atomic(settings_path, kept_settings, kept_size, NULL) == URSATZ_OK))) {
                UrsatzError error;
                memset(&error, 0, sizeof(error));
                if (workshop_txn_commit(txn, &error) != URSATZ_OK) {
                    result = workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
                }
                txn = NULL;
            } else {
                result = workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
            }
            UNLOCK();
            free(kept_settings);
            if (txn) {
                workshop_txn_abort(txn);
            }
            styles_collected_free(&collected);
        }
        styles_normalized_source_free(&source);
    }
    stored_source_free(&stored);
    return result;
}

UrsatzStatus workshop_style_delete(const char *id) {
    char dir[PATH_BYTES];
    if (!styles_style_dir_path(id, dir, sizeof(dir))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
    }
    WorkshopTxn *txn = workshop_txn_begin(NULL, "style.delete", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
    }
    LOCK();
    UrsatzStatus result = URSATZ_OK;
    if (!common_path_is_dir(dir)) {
        result = workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
    } else if (workshop_txn_remove(txn, dir) != URSATZ_OK) {
        result = workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
    }
    UNLOCK();
    if (result != URSATZ_OK) {
        workshop_txn_abort(txn);
        return result;
    }
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    return workshop_txn_commit(txn, &error) == URSATZ_OK ? URSATZ_OK : workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
}

UrsatzStatus workshop_style_model_path(const char *id, char *buffer, size_t buffer_size) {
    char path[PATH_BYTES + 32];
    char dir[PATH_BYTES];
    if (!buffer || buffer_size == 0 || !styles_style_dir_path(id, dir, sizeof(dir))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
    }
    snprintf(path, sizeof(path), "%s/style-model.json", dir);

    LOCK();
    char resolved[4096]; /* realpath writes up to PATH_MAX bytes */
    bool found = realpath(path, resolved) != NULL;
    UNLOCK();
    if (!found) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
    }
    if (strlen(resolved) >= buffer_size) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
    }
    memcpy(buffer, resolved, strlen(resolved) + 1);
    return URSATZ_OK;
}
