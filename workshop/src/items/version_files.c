#include "store_internal.h"
#include "layers.h"
#include "layout.h"

/* --- version files ------------------------------------------------------- */

/* theme-<section id>.plan.json: the copy of a theme plan a piece version keeps. */
static bool is_theme_copy_name(const char *name) {
    static const char suffix[] = ".plan.json";
    size_t length = strlen(name);
    if (strncmp(name, "theme-", 6) != 0 || length <= 6 + sizeof(suffix) - 1 ||
        strcmp(name + length - (sizeof(suffix) - 1), suffix) != 0) {
        return false;
    }
    char id[PIECE_ID_MAX + 2];
    size_t id_length = length - 6 - (sizeof(suffix) - 1);
    if (id_length >= sizeof(id)) {
        return false;
    }
    memcpy(id, name + 6, id_length);
    id[id_length] = '\0';
    return store_piece_section_id_ok(id);
}

bool workshop_material_version_name_ok(const char *name) {
    size_t length = name ? strlen(name) : 0;
    if (length < 1 || length > 31 || name[0] < 'a' || name[0] > 'z') {
        return false;
    }
    for (size_t i = 1; i < length; i++) {
        if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= '0' && name[i] <= '9') || name[i] == '-')) {
            return false;
        }
    }
    return true;
}

/* The type of <name>-plan.json, <name>.mid or <name>.musicxml for a version name of a piece's form. */
static const char *named_version_file_type(const char *file) {
    static const struct {
        const char *suffix;
        const char *type;
    } suffixes[] = {{"-plan.json", "application/json"}, {".mid", "audio/midi"}, {".musicxml", "application/xml"}};
    size_t length = strlen(file);
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        size_t suffix = strlen(suffixes[i].suffix);
        if (length > suffix && length - suffix <= 31 && strcmp(file + length - suffix, suffixes[i].suffix) == 0) {
            char name[32];
            memcpy(name, file, length - suffix);
            name[length - suffix] = '\0';
            if (workshop_material_version_name_ok(name)) {
                return suffixes[i].type;
            }
        }
    }
    return NULL;
}

const char *workshop_material_version_file_type(const char *name) {
    static const struct {
        const char *name;
        const char *type;
    } files[] = {{"request.json", "application/json"},
                 {"brief.style.json", "application/json"},
                 {"plan.json", "application/json"},
                 {"theme.mid", "audio/midi"},
                 {"theme.musicxml", "application/xml"},
                 {"ending-plan.json", "application/json"},
                 {"loop.mid", "audio/midi"},
                 {"loop.musicxml", "application/xml"},
                 {"ending.mid", "audio/midi"},
                 {"ending.musicxml", "application/xml"}};
    for (size_t i = 0; name && i < sizeof(files) / sizeof(files[0]); i++) {
        if (strcmp(name, files[i].name) == 0) {
            return files[i].type;
        }
    }
    if (name && is_theme_copy_name(name)) {
        return "application/json";
    }
    return name ? named_version_file_type(name) : NULL;
}

bool workshop_material_version_dir(const char *id, int number, char *out, size_t out_size) {
    LOCK();
    bool ok = store_ready && number >= 1 && store_version_dir_path(id, number, out, out_size);
    UNLOCK();
    return ok;
}

bool workshop_material_render_tmp_dir(char *out, size_t out_size) {
    LOCK();
    bool ok = store_ready && workshop_layout_render_tmp_dir(out, out_size);
    UNLOCK();
    return ok;
}

UrsatzStatus workshop_material_version_layers(const char *id, int number, cJSON **out) {
    *out = NULL;
    char dir[PATH_BYTES];
    LOCK();
    bool found = store_ready && number >= 1 && store_version_dir_path(id, number, dir, sizeof(dir)) && common_path_is_dir(dir);
    UNLOCK();
    if (!found) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    return workshop_layers_describe(dir, out) == URSATZ_OK ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
}

UrsatzStatus workshop_material_version_file(const char *id, int number, const char *name,
                                                          unsigned char **out_data, size_t *out_size) {
    *out_data = NULL;
    *out_size = 0;
    if (!workshop_material_version_file_type(name)) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    LOCK();
    if (!store_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON *item = NULL;
    UrsatzStatus result = store_load_item_locked(id, &item);
    char path[PATH_BYTES];
    if (result == URSATZ_OK &&
        (number < 1 || number > store_current_version_of(item) ||
         !workshop_layout_version_file(common_json_string(item, "project_id"), id, number, name, path, sizeof(path)))) {
        result = workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    if (result == URSATZ_OK) {
        char *data = NULL;
        size_t length = 0;
        UrsatzStatus read = common_file_read(path, &data, &length, NULL);
        if (read == URSATZ_ERR_ALLOC) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        } else if (read != URSATZ_OK) {
            result = workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
        } else {
            *out_data = (unsigned char *)data;
            *out_size = length;
        }
    }
    UNLOCK();
    cJSON_Delete(item);
    return result;
}
