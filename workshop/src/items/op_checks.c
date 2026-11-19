#include "store_internal.h"
#include "formats.h"
#include "layout.h"

/* --- operation marks ----------------------------------------------------- */

#define OP_CHECKS_MAX 64
#define OP_CHECK_COMMENT_MAX 500
#define OP_CHECK_NOTES_MAX 4000

static bool op_name_ok(const char *name) {
    size_t length = strlen(name);
    if (length == 0 || length > 40) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return true;
}

static bool check_date_ok(const char *text) {
    if (strlen(text) != 10) {
        return false;
    }
    for (int i = 0; i < 10; i++) {
        bool digit = text[i] >= '0' && text[i] <= '9';
        if (i == 4 || i == 7 ? text[i] != '-' : !digit) {
            return false;
        }
    }
    int month = (text[5] - '0') * 10 + (text[6] - '0');
    int day = (text[8] - '0') * 10 + (text[9] - '0');
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

UrsatzStatus workshop_material_op_checks_validate(const cJSON *body, UrsatzError *error) {
    const cJSON *checks = cJSON_GetObjectItemCaseSensitive(body, "checks");
    if (!cJSON_IsObject(body) || !cJSON_IsObject(checks)) {
        return store_invalid_body(error, "op-checks is {checks: {<operation>: {...}}}");
    }
    if (cJSON_GetArraySize(checks) > OP_CHECKS_MAX) {
        return store_invalid_body(error, "at most 64 operations can be marked");
    }
    const cJSON *check = NULL;
    cJSON_ArrayForEach(check, checks) {
        const cJSON *verdict = cJSON_GetObjectItemCaseSensitive(check, "verdict");
        const cJSON *comment = cJSON_GetObjectItemCaseSensitive(check, "comment");
        const cJSON *seed = cJSON_GetObjectItemCaseSensitive(check, "seed");
        const cJSON *notes = cJSON_GetObjectItemCaseSensitive(check, "notes_text");
        const cJSON *date = cJSON_GetObjectItemCaseSensitive(check, "date");
        if (!check->string || !op_name_ok(check->string)) {
            return store_invalid_body(error, "an operation name is lowercase letters, digits and hyphens");
        }
        if (!cJSON_IsObject(check) || !cJSON_IsString(verdict) ||
            (strcmp(verdict->valuestring, "works") != 0 && strcmp(verdict->valuestring, "wrong") != 0)) {
            return store_invalid_body(error, "a mark's verdict is works or wrong");
        }
        if (!cJSON_IsString(comment) || strlen(comment->valuestring) > OP_CHECK_COMMENT_MAX) {
            return store_invalid_body(error, "a mark's comment is text of at most 500 characters");
        }
        if (!cJSON_IsNumber(seed) || seed->valuedouble < 0 || seed->valuedouble != (double)(long long)seed->valuedouble) {
            return store_invalid_body(error, "a mark's seed is a whole number from 0");
        }
        if (!cJSON_IsString(notes) || strlen(notes->valuestring) > OP_CHECK_NOTES_MAX) {
            return store_invalid_body(error, "a mark's notes_text is the result checked, as text");
        }
        if (!cJSON_IsString(date) || !check_date_ok(date->valuestring)) {
            return store_invalid_body(error, "a mark's date is YYYY-MM-DD");
        }
    }
    return URSATZ_OK;
}

/* The path of the marks file of motif id; NOT_FOUND unless id is a motif. */
static UrsatzStatus op_checks_path_locked(const char *id, char *path, size_t path_size) {
    cJSON *item = NULL;
    UrsatzStatus result = store_load_item_locked(id, &item);
    if (result == URSATZ_OK) {
        if (strcmp(common_json_string(item, "type"), "motif") != 0) {
            result = workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
        } else if (!store_item_file_path(id, WORKSHOP_FILE_OP_CHECKS, path, path_size)) {
            result = workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
        }
    }
    cJSON_Delete(item);
    return result;
}

UrsatzStatus workshop_material_op_checks_get(const char *id, cJSON **out) {
    *out = NULL;
    LOCK();
    char path[PATH_BYTES];
    UrsatzStatus result = store_ready ? op_checks_path_locked(id, path, sizeof(path)) : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    if (result == URSATZ_OK) {
        cJSON *stored = NULL;
        workshop_format_read(path, WORKSHOP_FORMAT_OP_CHECKS, true, &stored, NULL);
        if (stored && workshop_material_op_checks_validate(stored, NULL) == URSATZ_OK) {
            *out = stored;
        } else {
            /* No marks yet, or a file that is not marks: an empty set. */
            cJSON_Delete(stored);
            *out = cJSON_CreateObject();
            if (!*out || !cJSON_AddObjectToObject(*out, "checks")) {
                cJSON_Delete(*out);
                *out = NULL;
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            }
        }
    }
    UNLOCK();
    return result;
}

UrsatzStatus workshop_material_op_checks_put(const char *id, const cJSON *body, UrsatzError *error, cJSON **out) {
    *out = NULL;
    UrsatzStatus result = workshop_material_op_checks_validate(body, error);
    if (result != URSATZ_OK) {
        return result;
    }
    LOCK();
    char path[PATH_BYTES];
    result = store_ready ? op_checks_path_locked(id, path, sizeof(path)) : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    if (result == URSATZ_OK) {
        /* Only the five members are kept, so a stray member never reaches the file. */
        cJSON *stored = cJSON_CreateObject();
        cJSON *checks = stored ? cJSON_AddObjectToObject(stored, "checks") : NULL;
        const cJSON *check = NULL;
        bool ok = checks != NULL;
        cJSON_ArrayForEach(check, cJSON_GetObjectItemCaseSensitive(body, "checks")) {
            cJSON *copy = cJSON_CreateObject();
            ok = ok && copy && cJSON_AddStringToObject(copy, "verdict", common_json_string(check, "verdict")) &&
                 cJSON_AddStringToObject(copy, "comment", common_json_string(check, "comment")) &&
                 cJSON_AddNumberToObject(copy, "seed", cJSON_GetObjectItemCaseSensitive(check, "seed")->valuedouble) &&
                 cJSON_AddStringToObject(copy, "notes_text", common_json_string(check, "notes_text")) &&
                 cJSON_AddStringToObject(copy, "date", common_json_string(check, "date")) &&
                 cJSON_AddItemToObject(checks, check->string, copy);
            if (!ok) {
                cJSON_Delete(copy);
                break;
            }
        }
        if (ok && (workshop_format_write(path, WORKSHOP_FORMAT_OP_CHECKS, stored, NULL) == URSATZ_OK)) {
            *out = stored;
        } else {
            cJSON_Delete(stored);
            result = ok ? workshop_fail(URSATZ_ERR_INTERNAL, "material store failure") : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
    }
    UNLOCK();
    return result;
}
