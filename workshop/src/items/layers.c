#include "layers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>

#include <realization/realization_json.h>
#include <realization/work.h>
#include <reading/reading_json.h>

#include "base/knowledge.h"

#define PATH_BYTES 1024

static bool has_suffix(const char *text, const char *suffix) {
    size_t length = strlen(text);
    size_t tail = strlen(suffix);
    return length >= tail && strcmp(text + length - tail, suffix) == 0;
}

bool workshop_layers_is_realization_path(const char *final_path) {
    return strcmp(final_path, WORKSHOP_FILE_REALIZATION) == 0 || has_suffix(final_path, "/" WORKSHOP_FILE_REALIZATION);
}

bool workshop_layers_is_reading_path(const char *final_path) {
    return strstr(final_path, "/" WORKSHOP_DIR_READINGS "/") != NULL && has_suffix(final_path, ".json") &&
           strchr(strstr(final_path, "/" WORKSHOP_DIR_READINGS "/") + strlen("/" WORKSHOP_DIR_READINGS "/"), '/') == NULL;
}

/* The work of the file at path (caller destroys), with its digest. */
static UrsatzStatus read_work(const char *path, const char *named, Work **work, char digest[17], UrsatzError *error) {
    char *data = NULL;
    UrsatzStatus status = common_file_read(path, &data, NULL, error);
    if (status != URSATZ_OK) {
        return status;
    }
    RealizationJsonError detail;
    memset(&detail, 0, sizeof(detail));
    *work = NULL;
    if (realization_work_from_json(workshop_knowledge_snapshot(), data, work, &detail) != REALIZATION_OK || !*work) {
        status = ursatz_error_set(error, URSATZ_ERR_FORMAT, named, "%s is not a realization: %s at %s", named, detail.message, detail.path);
    } else if (!realization_digest(*work, digest)) {
        work_destroy(*work);
        *work = NULL;
        status = ursatz_error_set(error, URSATZ_ERR_FORMAT, named, "%s: the realization cannot be named", named);
    }
    free(data);
    return status;
}

UrsatzStatus workshop_layers_check_realization(const char *actual, const char *final_path, UrsatzError *error) {
    Work *work = NULL;
    char digest[17];
    UrsatzStatus status = read_work(actual, final_path, &work, digest, error);
    work_destroy(work);
    return status;
}

UrsatzStatus workshop_layers_check_reading(const char *actual, const char *final_path, const char *realization_actual, UrsatzError *error) {
    Work *work = NULL;
    char digest[17];
    UrsatzStatus status = read_work(realization_actual, final_path, &work, digest, error);
    work_destroy(work);
    if (status != URSATZ_OK) {
        return status;
    }
    char *data = NULL;
    status = common_file_read(actual, &data, NULL, error);
    if (status != URSATZ_OK) {
        return status;
    }
    ReadingJsonError detail;
    memset(&detail, 0, sizeof(detail));
    Reading *reading = NULL;
    if (reading_from_json(workshop_knowledge_snapshot(), data, &reading, &detail) != REALIZATION_OK || !reading) {
        status = ursatz_error_set(error, URSATZ_ERR_FORMAT, final_path, "%s is not a reading: %s at %s", final_path, detail.message, detail.path);
    } else if (strcmp(reading_target(reading)->digest, digest) != 0) {
        status = ursatz_error_set(error, URSATZ_ERR_FORMAT, final_path, "%s reads realization %s, not the one beside it (%s)", final_path,
                                  reading_target(reading)->digest, digest);
    }
    reading_destroy(reading);
    free(data);
    return status;
}

UrsatzStatus workshop_layers_describe(const char *folder, cJSON **out) {
    *out = NULL;
    char path[PATH_BYTES];
    if (snprintf(path, sizeof(path), "%s/" WORKSHOP_FILE_REALIZATION, folder) >= (int)sizeof(path) || !common_path_exists(path)) {
        *out = cJSON_CreateNull();
        return *out ? URSATZ_OK : URSATZ_ERR_ALLOC;
    }
    Work *work = NULL;
    char digest[17];
    UrsatzStatus status = read_work(path, path, &work, digest, NULL);
    if (status != URSATZ_OK) {
        *out = cJSON_CreateNull();
        return *out ? URSATZ_OK : URSATZ_ERR_ALLOC; /* a realization that does not read is no layer */
    }
    cJSON *layers = cJSON_CreateObject();
    cJSON *realization = layers ? cJSON_AddObjectToObject(layers, "realization") : NULL;
    cJSON *readings = layers ? cJSON_AddArrayToObject(layers, "readings") : NULL;
    bool ok = realization && readings && cJSON_AddStringToObject(realization, "digest", digest) &&
              cJSON_AddNumberToObject(realization, "movements", (double)work_movement_count(work));
    work_destroy(work);
    char readings_dir[PATH_BYTES];
    CommonNames names = {0};
    if (ok && snprintf(readings_dir, sizeof(readings_dir), "%s/" WORKSHOP_DIR_READINGS, folder) < (int)sizeof(readings_dir) &&
        common_dir_list(readings_dir, &names, NULL) == URSATZ_OK) {
        for (size_t i = 0; ok && i < names.count; i++) {
            char file[PATH_BYTES * 2];
            cJSON *reading = NULL;
            if (!has_suffix(names.items[i], ".json") || names.items[i][0] == '.' ||
                !common_path_join(file, sizeof(file), readings_dir, names.items[i]) ||
                common_json_read_file(file, &reading, NULL) != URSATZ_OK) {
                continue;
            }
            const cJSON *source = cJSON_GetObjectItemCaseSensitive(reading, "source");
            cJSON *entry = cJSON_CreateObject();
            ok = entry && cJSON_AddStringToObject(entry, "source", common_json_string(source, "kind")) &&
                 cJSON_AddStringToObject(entry, "name", common_json_string(source, "name")) && cJSON_AddItemToArray(readings, entry);
            if (!ok) {
                cJSON_Delete(entry);
            }
            cJSON_Delete(reading);
        }
    }
    common_names_free(&names);
    if (!ok) {
        cJSON_Delete(layers);
        return URSATZ_ERR_ALLOC;
    }
    *out = layers;
    return URSATZ_OK;
}
