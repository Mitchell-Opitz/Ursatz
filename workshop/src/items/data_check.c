#include "data_check.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>

#define PATH_BYTES 512
#define TEXT_BYTES 4096

static const char *kChecked = ".pre-overhaul-checked";
static const char *kStoreMarkers[] = {"tones-migrated", "segments-migrated", "themes-cleared"};

/* Appends text to buffer (of TEXT_BYTES) when it fits; a longer text is cut and ends with "...". */
static void append(char *buffer, const char *text) {
    size_t used = strlen(buffer);
    size_t room = TEXT_BYTES - used;
    int wrote = snprintf(buffer + used, room, "%s", text);
    if (wrote < 0 || (size_t)wrote >= room) {
        snprintf(buffer + TEXT_BYTES - 4, 4, "...");
    }
}

/* True when the key, trimmed of white space, is not empty and not exactly "C major" or "C minor" (the old editor's rule). */
static bool key_is_off_c(const char *key) {
    while (*key && isspace((unsigned char)*key)) {
        key++;
    }
    size_t length = strlen(key);
    while (length > 0 && isspace((unsigned char)key[length - 1])) {
        length--;
    }
    if (length == 0) {
        return false;
    }
    return !((length == 7 && strncmp(key, "C major", 7) == 0) || (length == 7 && strncmp(key, "C minor", 7) == 0));
}

/* True when items/<name>/item.json exists; *out_item gets its object (caller frees) when out_item is not NULL. */
static bool read_item(const char *items_dir, const char *name, cJSON **out_item) {
    char path[PATH_BYTES];
    if (name[0] == '.' || snprintf(path, sizeof(path), "%s/%s/item.json", items_dir, name) >= (int)sizeof(path) ||
        !common_path_exists(path)) {
        return false;
    }
    if (out_item) {
        common_json_read_object(path, out_item, NULL);
    }
    return true;
}

/* True when the motif item's current version has a key off C. A motif that names a material is on C (its key text is gone); an
 * old one has the key text, "C major" or "C minor" passes. */
static bool motif_is_off_c(const char *items_dir, const char *name, const cJSON *item) {
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(item, "type");
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(item, "current_version");
    if (!cJSON_IsString(type) || strcmp(type->valuestring, "motif") != 0 || !cJSON_IsNumber(current) ||
        current->valuedouble < 1) {
        return false;
    }
    char path[PATH_BYTES];
    if (snprintf(path, sizeof(path), "%s/%s/v%d.json", items_dir, name, (int)current->valuedouble) >= (int)sizeof(path)) {
        return false;
    }
    cJSON *body = NULL;
    common_json_read_object(path, &body, NULL);
    const cJSON *motif = cJSON_GetObjectItemCaseSensitive(body, "motif");
    const cJSON *key = cJSON_GetObjectItemCaseSensitive(motif, "key");
    bool off = !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(motif, "material")) && cJSON_IsString(key) &&
               key_is_off_c(key->valuestring);
    cJSON_Delete(body);
    return off;
}

bool workshop_data_check(const char *data_dir, char *message, size_t size) {
    if (message && size > 0) {
        message[0] = '\0';
    }
    if (!data_dir || !message || size == 0) {
        return false;
    }
    char path[PATH_BYTES];
    if (snprintf(path, sizeof(path), "%s/%s", data_dir, kChecked) >= (int)sizeof(path)) {
        snprintf(message, size, "the data folder path is too long");
        return false;
    }
    if (common_path_exists(path)) {
        return true;
    }

    char problems[TEXT_BYTES] = "";

    /* (a) the corpus.db import never ran */
    char old_db[PATH_BYTES];
    char import_marker[PATH_BYTES];
    if (snprintf(old_db, sizeof(old_db), "%s/data/corpus.db", data_dir) < (int)sizeof(old_db) &&
        snprintf(import_marker, sizeof(import_marker), "%s/collections/.imported-from-corpus-db", data_dir) <
            (int)sizeof(import_marker) &&
        common_path_exists(old_db) && !common_path_exists(import_marker)) {
        append(problems, "data/corpus.db was never imported (collections/.imported-from-corpus-db is missing)");
    }

    /* (b) and (c) the material store */
    char items_dir[PATH_BYTES];
    CommonNames names = {0};
    if (snprintf(items_dir, sizeof(items_dir), "%s/materials/items", data_dir) < (int)sizeof(items_dir) &&
        common_dir_list(items_dir, &names, NULL) == URSATZ_OK) {
        size_t items = 0;
        char motifs[TEXT_BYTES] = "";
        for (size_t i = 0; i < names.count; i++) {
            cJSON *item = NULL;
            if (!read_item(items_dir, names.items[i], &item)) {
                continue;
            }
            items++;
            if (motif_is_off_c(items_dir, names.items[i], item)) {
                if (motifs[0]) {
                    append(motifs, ", ");
                }
                append(motifs, names.items[i]);
            }
            cJSON_Delete(item);
        }
        common_names_free(&names);

        if (items > 0) {
            char missing[TEXT_BYTES] = "";
            for (size_t i = 0; i < sizeof(kStoreMarkers) / sizeof(kStoreMarkers[0]); i++) {
                char marker[PATH_BYTES];
                if (snprintf(marker, sizeof(marker), "%s/materials/%s", data_dir, kStoreMarkers[i]) < (int)sizeof(marker) &&
                    common_path_exists(marker)) {
                    continue;
                }
                if (missing[0]) {
                    append(missing, ", ");
                }
                append(missing, kStoreMarkers[i]);
            }
            if (missing[0]) {
                if (problems[0]) {
                    append(problems, "; ");
                }
                append(problems, "the material store lacks the migration markers ");
                append(problems, missing);
            }
        }
        if (motifs[0]) {
            if (problems[0]) {
                append(problems, "; ");
            }
            append(problems, "motifs whose current version is not in C (open each there and use Rewrite in C): ");
            append(problems, motifs);
        }
    }

    if (problems[0]) {
        const char *where = strcmp(data_dir, ".") == 0 ? "the working directory" : data_dir;
        snprintf(message, size,
                 "the data folder (%s) has not had the one-time migrations of the GUI before the overhaul: %s. Run the GUI "
                 "built from the git tag pre-overhaul once on this data folder, then start this one.",
                 where, problems);
        return false;
    }

    static const char kMarkerText[] = "checked before the overhaul\n";
    if (!(common_file_write_atomic(path, kMarkerText, sizeof(kMarkerText) - 1, NULL) == URSATZ_OK)) {
        snprintf(message, size, "the data folder (%s) could not get its marker file %s",
                 strcmp(data_dir, ".") == 0 ? "the working directory" : data_dir, kChecked);
        return false;
    }
    return true;
}
