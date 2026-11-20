#define _XOPEN_SOURCE 700

#include "request_resolve.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <common/json.h>

#include "items/store.h"
#include "themes.h"

#define PATH_BYTES 800

/* The copy of a used motif version, from the body's uses. NULL when the use is not there. */
static const cJSON *used_motif(const cJSON *body, const char *item_id, int version) {
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, cJSON_GetObjectItemCaseSensitive(body, "uses")) {
        const cJSON *number = cJSON_GetObjectItemCaseSensitive(use, "version");
        if (strcmp(common_json_string(use, "item_id"), item_id) == 0 && cJSON_IsNumber(number) && number->valueint == version) {
            return cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(use, "copy"), "motif");
        }
    }
    return NULL;
}

/* The reference of a node, {item_id, version[, file]} under "$item", or NULL. */
static const cJSON *reference_of(const cJSON *node, const char *key) {
    const cJSON *reference = cJSON_IsObject(node) ? cJSON_GetObjectItemCaseSensitive(node, key) : NULL;
    return cJSON_IsObject(reference) ? reference : NULL;
}

static int reference_version(const cJSON *reference) {
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(reference, "version");
    return cJSON_IsNumber(version) ? version->valueint : 0;
}

/* The motif a reference names, from the body's uses; NULL with *error set (not found, at path) when the body does not use it or it
   has no degrees. */
static const cJSON *motif_of(const cJSON *body, const cJSON *reference, const char *path, UrsatzError *error) {
    const cJSON *motif = used_motif(body, common_json_string(reference, "item_id"), reference_version(reference));
    if (!motif) {
        ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, path, "the motif version is not one the body uses");
        return NULL;
    }
    if (!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(motif, "degrees"))) {
        ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, path, "the motif has no degrees");
        return NULL;
    }
    return motif;
}

/* A motive of a theme request: the motif's degrees, pickup and features, its base harmony (its own harmony when it has one,
   else the proposed one of its analysis) and its tone overrides. */
static cJSON *theme_motive(const char *id, const cJSON *motif) {
    cJSON *motive = workshop_theme_motive_json(id, NULL, motif, "melodic");
    if (!motive) {
        return NULL;
    }
    const cJSON *own = cJSON_GetObjectItemCaseSensitive(motif, "harmony");
    const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(motif, "analysis");
    const cJSON *base = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(analysis, "analysis"), "base_harmony"), "text");
    const char *text = cJSON_IsString(own) && own->valuestring[0] != '\0' ? own->valuestring
                       : cJSON_IsString(base) && base->valuestring[0] != '\0' ? base->valuestring
                                                                              : NULL;
    const cJSON *tones = cJSON_GetObjectItemCaseSensitive(motif, "tone_overrides");
    bool ok = true;
    if (text) {
        cJSON *replacement = cJSON_CreateString(text);
        ok = replacement != NULL;
        if (ok) {
            cJSON_ReplaceItemInObjectCaseSensitive(motive, "harmony", replacement);
        }
    }
    if (ok && cJSON_IsObject(tones) && tones->child) {
        cJSON *copy = cJSON_Duplicate(tones, 1);
        ok = copy && cJSON_AddItemToObject(motive, "tone_overrides", copy);
        if (!ok) {
            cJSON_Delete(copy);
        }
    }
    if (!ok) {
        cJSON_Delete(motive);
        return NULL;
    }
    return motive;
}

/* Replaces the member name of object by value (the member keeps its place). false on allocation failure. */
static bool replace_member(cJSON *object, const char *name, cJSON *value) {
    if (!value) {
        return false;
    }
    if (!cJSON_ReplaceItemInObjectCaseSensitive(object, name, value)) {
        cJSON_Delete(value);
        return false;
    }
    return true;
}

static UrsatzStatus out_of_memory(UrsatzError *error) {
    return ursatz_error_set(error, URSATZ_ERR_ALLOC, NULL, "out of memory");
}

/* The plan of a theme fill, from its reference: copied into dir as theme-<section>.plan.json, or named at its source. */
static UrsatzStatus resolve_plan(cJSON *fill, const char *section, const char *dir, const char *path, UrsatzError *error) {
    const cJSON *reference = reference_of(cJSON_GetObjectItemCaseSensitive(fill, "plan"), "$item");
    if (!reference) {
        return URSATZ_OK;
    }
    char source[PATH_BYTES];
    char plan[PATH_BYTES + 32];
    const char *file = cJSON_IsString(cJSON_GetObjectItemCaseSensitive(reference, "file")) ? common_json_string(reference, "file") : "plan.json";
    if (!workshop_material_version_dir(common_json_string(reference, "item_id"), reference_version(reference), source, sizeof(source))) {
        return ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, path, "a theme fill names no version");
    }
    snprintf(plan, sizeof(plan), "%s/%s", source, file);
    char target[PATH_BYTES + 64];
    if (!dir) {
        snprintf(target, sizeof(target), "%s", plan);
    } else {
        snprintf(target, sizeof(target), "%s/theme-%s.plan.json", dir, section);
        char *data = NULL;
        size_t size = 0;
        bool copied = common_file_read(plan, &data, &size, NULL) == URSATZ_OK &&
                      common_file_write_atomic(target, data, size, NULL) == URSATZ_OK;
        free(data);
        if (!copied) {
            return ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, path, "the plan of the theme in section %s could not be copied", section);
        }
    }
    return replace_member(fill, "plan", cJSON_CreateString(target)) ? URSATZ_OK : out_of_memory(error);
}

/* A fill of a piece request: its motive, its pool of saved forms and the plan of a theme. */
static UrsatzStatus resolve_fill(const cJSON *body, cJSON *fill, const char *section, const char *dir, const char *path,
                                 UrsatzError *error) {
    if (!cJSON_IsObject(fill)) {
        return URSATZ_OK;
    }
    char member[PATH_BYTES];
    const cJSON *reference = reference_of(cJSON_GetObjectItemCaseSensitive(fill, "motive"), "$item");
    if (reference) {
        snprintf(member, sizeof(member), "%s/motive", path);
        const cJSON *motif = motif_of(body, reference, member, error);
        if (!motif) {
            return URSATZ_ERR_NOT_FOUND;
        }
        if (!replace_member(fill, "motive", workshop_theme_motive_json(NULL, NULL, motif, "melodic"))) {
            return out_of_memory(error);
        }
    }
    const cJSON *pool = reference_of(cJSON_GetObjectItemCaseSensitive(fill, "forms"), "$pool");
    if (pool) {
        cJSON *derived_list = NULL;
        cJSON *forms = cJSON_CreateArray();
        if (!forms || workshop_material_derived_motifs(&derived_list) != URSATZ_OK) {
            cJSON_Delete(forms);
            return ursatz_error_set(error, forms ? URSATZ_ERR_INTERNAL : URSATZ_ERR_ALLOC, NULL, "the saved forms could not be read");
        }
        UrsatzStatus added = workshop_theme_add_pool(derived_list, common_json_string(pool, "item_id"),
                                                     cJSON_IsString(cJSON_GetObjectItemCaseSensitive(pool, "pool")) ? common_json_string(pool, "pool") : "approved-and-unflagged", forms);
        cJSON_Delete(derived_list);
        if (added != URSATZ_OK || !replace_member(fill, "forms", forms)) {
            return added != URSATZ_OK ? ursatz_error_set(error, added, NULL, "the saved forms could not be read") : out_of_memory(error);
        }
    }
    snprintf(member, sizeof(member), "%s/plan", path);
    return resolve_plan(fill, section, dir, member, error);
}

static UrsatzStatus resolve_piece(const cJSON *body, cJSON *request, const char *dir, UrsatzError *error) {
    const cJSON *reference = reference_of(cJSON_GetObjectItemCaseSensitive(request, "motive"), "$item");
    if (reference) {
        const cJSON *motif = motif_of(body, reference, "/motive", error);
        if (!motif) {
            return URSATZ_ERR_NOT_FOUND;
        }
        if (!replace_member(request, "motive", workshop_theme_motive_json(NULL, NULL, motif, "melodic"))) {
            return out_of_memory(error);
        }
    }
    int index = 0;
    cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(request, "sections")) {
        const char *id = common_json_string(section, "id");
        id = id[0] != '\0' ? id : common_json_string(section, "name");
        char path[128];
        snprintf(path, sizeof(path), "/sections/%d/fill", index);
        UrsatzStatus status = resolve_fill(body, cJSON_GetObjectItemCaseSensitive(section, "fill"), id, dir, path, error);
        if (status != URSATZ_OK) {
            return status;
        }
        index++;
    }
    return URSATZ_OK;
}

static UrsatzStatus resolve_theme(const cJSON *body, cJSON *request, UrsatzError *error) {
    int index = 0;
    cJSON *motive = NULL;
    cJSON_ArrayForEach(motive, cJSON_GetObjectItemCaseSensitive(request, "motives")) {
        const cJSON *reference = reference_of(motive, "$item");
        if (reference) {
            char path[64];
            snprintf(path, sizeof(path), "/motives/%d", index);
            const cJSON *motif = motif_of(body, reference, path, error);
            if (!motif) {
                return URSATZ_ERR_NOT_FOUND;
            }
            cJSON *resolved = theme_motive(common_json_string(motive, "id"), motif);
            if (!resolved) {
                return out_of_memory(error);
            }
            cJSON_ReplaceItemInArray(cJSON_GetObjectItemCaseSensitive(request, "motives"), index, resolved);
            motive = resolved;
        }
        index++;
    }
    cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(request, "accompaniment");
    const cJSON *reference = reference_of(cJSON_GetObjectItemCaseSensitive(accompaniment, "motive"), "$item");
    if (reference) {
        const cJSON *motif = motif_of(body, reference, "/accompaniment/motive", error);
        if (!motif) {
            return URSATZ_ERR_NOT_FOUND;
        }
        if (!replace_member(accompaniment, "motive", workshop_theme_motive_json(NULL, NULL, motif, "accompaniment"))) {
            return out_of_memory(error);
        }
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_request_resolve(const char *kind, const cJSON *body, const char *dir, const char *style_path,
                                      cJSON **resolved, UrsatzError *error) {
    const cJSON *stored = cJSON_GetObjectItemCaseSensitive(body, "request");
    if (!cJSON_IsObject(stored)) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/request", "the body has no request");
    }
    cJSON *request = cJSON_Duplicate(stored, 1);
    if (!request) {
        return out_of_memory(error);
    }
    UrsatzStatus status = strcmp(kind, "theme") == 0 ? resolve_theme(body, request, error) : resolve_piece(body, request, dir, error);
    if (status == URSATZ_OK) {
        /* A style, its weight and its exclusions go together: without a brief the request has none. */
        if (style_path) {
            cJSON_DeleteItemFromObjectCaseSensitive(request, "style");
            if (!cJSON_AddStringToObject(request, "style", style_path)) {
                status = out_of_memory(error);
            }
        } else {
            cJSON_DeleteItemFromObjectCaseSensitive(request, "style");
            cJSON_DeleteItemFromObjectCaseSensitive(request, "style_weight");
            cJSON_DeleteItemFromObjectCaseSensitive(request, "without");
        }
    }
    if (status != URSATZ_OK) {
        cJSON_Delete(request);
        return status;
    }
    *resolved = request;
    return URSATZ_OK;
}
