#define _XOPEN_SOURCE 700

#include "piece_service.h"

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "material_service.h"

#define PATH_BYTES 800

/* One compose at a time: the next version number and its folder are decided before the Composer runs. */
static pthread_mutex_t g_compose_lock = PTHREAD_MUTEX_INITIALIZER;

static void say(char *message, size_t size, const char *text) {
    if (message && size > 0) {
        snprintf(message, size, "%s", text);
    }
}

static const char *text_of(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : "";
}

/* Adds child to parent as name, or frees it. false on allocation failure. */
static bool attach(cJSON *parent, const char *name, cJSON *child) {
    if (child && cJSON_AddItemToObject(parent, name, child)) {
        return true;
    }
    cJSON_Delete(child);
    return false;
}

/* Adds a copy of src's member name to dst, or null when src lacks it. false on allocation failure. */
static bool add_copy_or_null(cJSON *dst, const cJSON *src, const char *name) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(src, name);
    return attach(dst, name, member ? cJSON_Duplicate(member, 1) : cJSON_CreateNull());
}

/* Adds a copy of src's member name to dst when it is there and not null. false on allocation failure. */
static bool add_copy_if(cJSON *dst, const cJSON *src, const char *name) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(src, name);
    return !member || cJSON_IsNull(member) || attach(dst, name, cJSON_Duplicate(member, 1));
}

static bool copy_file(const char *from, const char *to) {
    FILE *in = fopen(from, "rb");
    FILE *out = in ? fopen(to, "wb") : NULL;
    bool ok = in && out;
    char buffer[16384];
    size_t n;
    while (ok && (n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        ok = fwrite(buffer, 1, n, out) == n;
    }
    ok = ok && !ferror(in);
    if (in) {
        fclose(in);
    }
    if (out && fclose(out) != 0) {
        ok = false;
    }
    return ok;
}

/* --- the request --------------------------------------------------------- */

/* The copy of a used motif version, from the body's uses. NULL when the use is not there. */
static const cJSON *used_motif(const cJSON *body, const char *item_id, int version) {
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, cJSON_GetObjectItemCaseSensitive(body, "uses")) {
        const cJSON *number = cJSON_GetObjectItemCaseSensitive(use, "version");
        if (strcmp(text_of(use, "item_id"), item_id) == 0 && cJSON_IsNumber(number) && number->valueint == version) {
            return cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(use, "copy"), "motif");
        }
    }
    return NULL;
}

/* The motive of a reference {item_id, version}, or NULL when its copy has no degrees. */
static cJSON *motive_of(const cJSON *body, const cJSON *reference, char *message, size_t message_size, const char *what) {
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(reference, "version");
    const cJSON *motif = used_motif(body, text_of(reference, "item_id"), cJSON_IsNumber(version) ? version->valueint : 0);
    if (!motif || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(motif, "degrees"))) {
        snprintf(message, message_size, "the motif of %s has no degrees", what);
        return NULL;
    }
    return ursatz_gui_theme_motive_json(NULL, NULL, motif, "melodic");
}

/* A section's fill as the request holds it. The plan of a theme fill is copied into dir. */
static UrsatzGuiThemeResult build_fill(const cJSON *body, const cJSON *derived_list, const char *pool, const cJSON *section,
                                       const char *dir, cJSON **out, char *message, size_t message_size) {
    const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
    const char *id = text_of(section, "id");
    const char *kind = cJSON_IsObject(fill) ? text_of(fill, "kind") : "empty";
    cJSON *object = cJSON_CreateObject();
    bool ok = object && cJSON_AddStringToObject(object, "kind", kind);
    UrsatzGuiThemeResult result = URSATZ_GUI_THEME_OK;
    if (ok && strcmp(kind, "motif") == 0) {
        char what[64];
        snprintf(what, sizeof(what), "section %s", id);
        cJSON *motive = motive_of(body, fill, message, message_size, what);
        cJSON *forms = cJSON_CreateArray();
        if (!motive) {
            cJSON_Delete(forms);
            cJSON_Delete(object);
            return URSATZ_GUI_THEME_ERR_INVALID;
        }
        ok = attach(object, "motive", motive);
        ok = attach(object, "forms", forms) && ok;
        if (ok) {
            result = ursatz_gui_theme_add_pool(derived_list, text_of(fill, "item_id"), pool, forms);
        }
        ok = ok && add_copy_if(object, fill, "into") && add_copy_if(object, fill, "hybrid");
    } else if (ok && strcmp(kind, "theme") == 0) {
        char source[PATH_BYTES];
        char plan[PATH_BYTES + 16];
        char copy[PATH_BYTES + 64];
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(fill, "version");
        if (!ursatz_gui_material_version_dir(text_of(fill, "item_id"), cJSON_IsNumber(version) ? version->valueint : 0, source,
                                             sizeof(source))) {
            say(message, message_size, "a theme fill names no version");
            result = URSATZ_GUI_THEME_ERR_INVALID;
        } else {
            snprintf(plan, sizeof(plan), "%s/plan.json", source);
            snprintf(copy, sizeof(copy), "%s/theme-%s.plan.json", dir, id);
            if (!copy_file(plan, copy)) {
                snprintf(message, message_size, "the plan of the theme in section %s could not be copied", id);
                result = URSATZ_GUI_THEME_ERR_INVALID;
            } else {
                ok = cJSON_AddStringToObject(object, "plan", copy) != NULL;
            }
        }
    } else if (ok && strcmp(kind, "progression") == 0) {
        ok = add_copy_or_null(object, fill, "numerals") && add_copy_if(object, fill, "cells");
    }
    if (!ok || result != URSATZ_GUI_THEME_OK) {
        cJSON_Delete(object);
        return result == URSATZ_GUI_THEME_OK ? URSATZ_GUI_THEME_ERR_ALLOC : result;
    }
    *out = object;
    return URSATZ_GUI_THEME_OK;
}

static cJSON *section_json(const cJSON *section, cJSON *fill) {
    cJSON *object = cJSON_CreateObject();
    const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(section, "pickup");
    bool ok = object && add_copy_or_null(object, section, "id") && add_copy_or_null(object, section, "part") &&
              add_copy_if(object, section, "letter") && add_copy_or_null(object, section, "beats") &&
              attach(object, "pickup", cJSON_IsString(pickup) ? cJSON_Duplicate(pickup, 1) : cJSON_CreateString("0")) &&
              add_copy_or_null(object, section, "cadence");
    ok = attach(object, "fill", fill) && ok;
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* What the request needs from the piece, checked before anything is made. */
static bool composable(const cJSON *piece, bool has_brief, char *message, size_t message_size) {
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    if (!cJSON_IsObject(structure) || !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(structure, "sections")) ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(structure, "key")) ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(structure, "mode")) ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(structure, "meter"))) {
        say(message, message_size, "the piece structure needs key, mode, meter and sections");
        return false;
    }
    if (strcmp(text_of(structure, "mode"), "style") == 0 && !has_brief) {
        say(message, message_size, "the mode style needs a brief");
        return false;
    }
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(structure, "sections")) {
        const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        if (cJSON_IsObject(fill) && strcmp(text_of(fill, "kind"), "motif") == 0 &&
            strcmp(text_of(fill, "into"), "hybrid") == 0 && !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(fill, "hybrid"))) {
            snprintf(message, message_size, "the hybrid motif fill of section %s needs hybrid", text_of(section, "id"));
            return false;
        }
    }
    return true;
}

/*
 * The piece request of the Composer (docs/piece-request.md) for the current version of a piece. dir is the new version's
 * folder (absolute), where theme plans are copied. style_path is the brief's style file, or NULL without a brief.
 */
static UrsatzGuiThemeResult build_request(const cJSON *body, const char *dir, const char *style_path, cJSON **out,
                                          char *message, size_t message_size) {
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(body, "piece");
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    const char *pool = cJSON_IsString(cJSON_GetObjectItemCaseSensitive(piece, "pool")) ? text_of(piece, "pool")
                                                                                       : "approved-and-unflagged";
    cJSON *request = cJSON_CreateObject();
    cJSON *sections = cJSON_CreateArray();
    cJSON *derived_list = NULL;
    UrsatzGuiThemeResult result = URSATZ_GUI_THEME_OK;
    if (!request || !sections) {
        result = URSATZ_GUI_THEME_ERR_ALLOC;
    }
    if (result == URSATZ_GUI_THEME_OK && ursatz_gui_material_derived_motifs(&derived_list) != URSATZ_GUI_MATERIAL_OK) {
        say(message, message_size, "the saved forms could not be read");
        result = URSATZ_GUI_THEME_ERR_FAILED;
    }
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(structure, "sections")) {
        if (result != URSATZ_GUI_THEME_OK) {
            break;
        }
        cJSON *fill = NULL;
        result = build_fill(body, derived_list, pool, section, dir, &fill, message, message_size);
        cJSON *entry = result == URSATZ_GUI_THEME_OK ? section_json(section, fill) : NULL;
        if (result == URSATZ_GUI_THEME_OK && (!entry || !cJSON_AddItemToArray(sections, entry))) {
            cJSON_Delete(entry);
            result = URSATZ_GUI_THEME_ERR_ALLOC;
        }
    }

    bool ok = result == URSATZ_GUI_THEME_OK && cJSON_AddStringToObject(request, "schema", "ursatz-piece-request") &&
              cJSON_AddNumberToObject(request, "schema_version", 1) && add_copy_if(request, structure, "key") &&
              add_copy_if(request, structure, "mode") && add_copy_if(request, structure, "meter");
    const cJSON *motive_ref = cJSON_GetObjectItemCaseSensitive(piece, "motive");
    if (ok && cJSON_IsObject(motive_ref)) {
        cJSON *motive = motive_of(body, motive_ref, message, message_size, "the piece motive");
        if (!motive) {
            result = URSATZ_GUI_THEME_ERR_INVALID;
            ok = false;
        } else {
            ok = attach(request, "motive", motive);
        }
    } else if (ok) {
        ok = cJSON_AddNullToObject(request, "motive") != NULL;
    }
    if (ok) {
        ok = attach(request, "sections", sections);
        sections = NULL;
    }

    /* The accompaniment: nothing or a pattern; the pattern motive is not available in a piece. */
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(piece, "accompaniment");
    if (ok && (!accompaniment || cJSON_IsNull(accompaniment))) {
        ok = cJSON_AddNullToObject(request, "accompaniment") != NULL;
    } else if (ok) {
        cJSON *pattern = cJSON_CreateObject();
        ok = pattern && cJSON_AddStringToObject(pattern, "pattern", text_of(accompaniment, "pattern"));
        if (ok) {
            ok = attach(request, "accompaniment", pattern);
        } else {
            cJSON_Delete(pattern);
        }
    }
    ok = ok && cJSON_AddNullToObject(request, "voicing") != NULL;
    if (ok && style_path) {
        cJSON *without = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(piece, "without"), 1);
        ok = cJSON_AddStringToObject(request, "style", style_path) && add_copy_if(request, piece, "style_weight");
        if (ok && without && cJSON_GetArraySize(without) > 0) {
            ok = attach(request, "without", without);
        } else {
            cJSON_Delete(without);
        }
    }
    ok = ok && add_copy_or_null(request, piece, "tempo");

    cJSON_Delete(derived_list);
    cJSON_Delete(sections);
    if (!ok) {
        cJSON_Delete(request);
        return result == URSATZ_GUI_THEME_OK ? URSATZ_GUI_THEME_ERR_ALLOC : result;
    }
    *out = request;
    return URSATZ_GUI_THEME_OK;
}

/* --- composing ----------------------------------------------------------- */

static const char *const kFileNames[] = {"request.json",    "brief.style.json", "plan.json",         "ending-plan.json",
                                         "loop.mid",        "loop.musicxml",    "ending.mid",        "ending.musicxml"};
#define FILE_NAME_COUNT 8

static UrsatzGuiThemeResult run_piece(const char *dir, bool check, bool has_seed, long long seed, cJSON **out_summary,
                                      char *message, size_t message_size) {
    char paths[FILE_NAME_COUNT][PATH_BYTES + 32];
    for (int i = 0; i < FILE_NAME_COUNT; i++) {
        snprintf(paths[i], sizeof(paths[i]), "%s/%s", dir, kFileNames[i]);
    }
    char seed_text[32];
    snprintf(seed_text, sizeof(seed_text), "%lld", seed);
    const char *args[24];
    int n = 0;
    args[n++] = "piece";
    args[n++] = paths[0];
    args[n++] = "--format";
    args[n++] = "json";
    args[n++] = "--out";
    args[n++] = paths[2];
    args[n++] = "--ending-out";
    args[n++] = paths[3];
    args[n++] = "--midi";
    args[n++] = paths[4];
    args[n++] = "--musicxml";
    args[n++] = paths[5];
    args[n++] = "--ending-midi";
    args[n++] = paths[6];
    args[n++] = "--ending-musicxml";
    args[n++] = paths[7];
    if (check) {
        args[n++] = "--check";
    }
    if (has_seed) {
        args[n++] = "--seed";
        args[n++] = seed_text;
    }
    args[n] = NULL;
    const char *outputs[] = {paths[2], paths[3], paths[4], paths[5], paths[6], paths[7], NULL};
    return ursatz_gui_theme_run_composer(args, outputs, out_summary, message, message_size);
}

/* The seed a run reports. */
static const cJSON *reported_seed(const cJSON *summary) {
    const cJSON *seed = cJSON_GetObjectItemCaseSensitive(summary, "seed");
    return cJSON_IsNumber(seed) ? seed : NULL;
}

/* The body of the new version: body plus piece.result. Takes ownership of summary and brief_summary. */
static cJSON *body_with_result(const cJSON *body, cJSON *summary, cJSON *brief_summary, bool has_brief) {
    cJSON *copy = cJSON_Duplicate(body, 1);
    cJSON *piece = cJSON_GetObjectItemCaseSensitive(copy, "piece");
    cJSON *result = cJSON_CreateObject();
    cJSON *files = cJSON_CreateArray();
    const cJSON *seed = reported_seed(summary);
    const cJSON *loop = cJSON_GetObjectItemCaseSensitive(summary, "loop");
    bool ok = copy && piece && result && files &&
              attach(result, "seed", seed ? cJSON_CreateNumber(seed->valuedouble) : cJSON_CreateNull()) &&
              attach(result, "loop", loop ? cJSON_Duplicate(loop, 1) : cJSON_CreateNull());
    for (int i = 0; ok && i < FILE_NAME_COUNT; i++) {
        if (i != 1 || has_brief) {
            ok = cJSON_AddItemToArray(files, cJSON_CreateString(kFileNames[i]));
        }
    }
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(piece, "structure"), "sections")) {
        const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        if (ok && cJSON_IsObject(fill) && strcmp(text_of(fill, "kind"), "theme") == 0) {
            char name[64];
            snprintf(name, sizeof(name), "theme-%s.plan.json", text_of(section, "id"));
            ok = cJSON_AddItemToArray(files, cJSON_CreateString(name));
        }
    }
    if (ok) {
        ok = attach(result, "summary", summary);
        summary = NULL;
    }
    if (ok) {
        ok = attach(result, "brief", brief_summary);
        brief_summary = NULL;
    }
    if (ok) {
        ok = attach(result, "files", files);
        files = NULL;
    }
    if (ok) {
        cJSON_DeleteItemFromObjectCaseSensitive(piece, "result");
        ok = cJSON_AddItemToObject(piece, "result", result);
        result = ok ? NULL : result;
    }
    cJSON_Delete(summary);
    cJSON_Delete(brief_summary);
    cJSON_Delete(files);
    cJSON_Delete(result);
    if (!ok) {
        cJSON_Delete(copy);
        return NULL;
    }
    return copy;
}

static UrsatzGuiThemeResult compose_locked(const char *id, bool has_seed, long long seed, bool update_uses,
                                           cJSON **out_item, char *message, size_t message_size) {
    UrsatzGuiThemeResult result = update_uses ? ursatz_gui_theme_update_sources(id, message, message_size)
                                              : URSATZ_GUI_THEME_OK;
    if (result != URSATZ_GUI_THEME_OK) {
        return result;
    }
    cJSON *meta = NULL;
    cJSON *body = NULL;
    UrsatzGuiMaterialResult read = ursatz_gui_material_item_get(id, &meta, &body);
    if (read != URSATZ_GUI_MATERIAL_OK) {
        return read == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND || read == URSATZ_GUI_MATERIAL_ERR_INVALID_ID
                   ? URSATZ_GUI_THEME_ERR_NOT_FOUND
                   : URSATZ_GUI_THEME_ERR_FAILED;
    }
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(body, "piece");
    const cJSON *brief_json = cJSON_GetObjectItemCaseSensitive(piece, "brief");
    bool has_brief = brief_json && !cJSON_IsNull(brief_json);
    cJSON *request = NULL;
    cJSON *summary = NULL;
    cJSON *brief_summary = NULL;
    cJSON *new_body = NULL;
    cJSON *item = NULL;
    char dir[PATH_BYTES];
    char absolute[PATH_BYTES];
    bool made = false;
    int number = cJSON_GetObjectItemCaseSensitive(meta, "current_version")->valueint + 1;

    if (strcmp(text_of(meta, "type"), "piece") != 0 || !cJSON_IsObject(piece)) {
        say(message, message_size, "the item is not a piece");
        result = URSATZ_GUI_THEME_ERR_INVALID;
    } else if (!composable(piece, has_brief, message, message_size)) {
        result = URSATZ_GUI_THEME_ERR_INVALID;
    } else if (!ursatz_gui_material_version_dir(id, number, dir, sizeof(dir))) {
        say(message, message_size, "the version folder does not fit");
        result = URSATZ_GUI_THEME_ERR_FAILED;
    } else {
        ursatz_gui_theme_remove_folder(dir); /* a folder no version names is left over from a failed run */
        char resolved[PATH_MAX];
        if (mkdir(dir, 0755) != 0 || !realpath(dir, resolved) ||
            snprintf(absolute, sizeof(absolute), "%s", resolved) >= (int)sizeof(absolute)) {
            say(message, message_size, "the version folder could not be made");
            result = URSATZ_GUI_THEME_ERR_FAILED;
        }
        made = result == URSATZ_GUI_THEME_OK;
    }

    char style_path[PATH_BYTES + 32];
    snprintf(style_path, sizeof(style_path), "%s/brief.style.json", made ? absolute : "");
    if (result == URSATZ_GUI_THEME_OK && has_brief) {
        char name[256];
        snprintf(name, sizeof(name), "%s brief", text_of(meta, "name"));
        result = ursatz_gui_theme_build_brief(brief_json, name, style_path, &brief_summary, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        result = build_request(body, absolute, has_brief ? style_path : NULL, &request, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        char request_path[PATH_BYTES + 32];
        char *text = cJSON_Print(request);
        snprintf(request_path, sizeof(request_path), "%s/request.json", absolute);
        if (!text || !ursatz_gui_theme_write_text(request_path, text)) {
            say(message, message_size, "the request could not be written");
            result = text ? URSATZ_GUI_THEME_ERR_FAILED : URSATZ_GUI_THEME_ERR_ALLOC;
        }
        free(text);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        const cJSON *body_seed = cJSON_GetObjectItemCaseSensitive(piece, "seed");
        bool use_seed = has_seed || cJSON_IsNumber(body_seed);
        long long chosen = has_seed ? seed : use_seed ? (long long)body_seed->valuedouble : 0;
        result = run_piece(absolute, cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(piece, "check")), use_seed, chosen,
                           &summary, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        if (!has_brief) {
            brief_summary = cJSON_CreateObject();
            if (!brief_summary || !cJSON_AddNumberToObject(brief_summary, "piece_count", 0) ||
                !cJSON_AddArrayToObject(brief_summary, "skipped")) {
                result = URSATZ_GUI_THEME_ERR_ALLOC;
            }
        }
        const cJSON *reported = result == URSATZ_GUI_THEME_OK ? reported_seed(summary) : NULL;
        char note[64];
        if (reported) {
            snprintf(note, sizeof(note), "composed seed %.0f", reported->valuedouble);
        } else {
            snprintf(note, sizeof(note), "composed");
        }
        if (result == URSATZ_GUI_THEME_OK) {
            new_body = body_with_result(body, summary, brief_summary, has_brief);
            summary = NULL;
            brief_summary = NULL;
            if (!new_body) {
                result = URSATZ_GUI_THEME_ERR_ALLOC;
            } else if (ursatz_gui_material_item_add_version(id, new_body, note, &item) != URSATZ_GUI_MATERIAL_OK) {
                say(message, message_size, "the version could not be written");
                result = URSATZ_GUI_THEME_ERR_FAILED;
            }
        }
    }
    if (result == URSATZ_GUI_THEME_OK) {
        /* Another writer may have added a version while the Composer ran; the folder follows its version. */
        int actual = cJSON_GetObjectItemCaseSensitive(item, "current_version")->valueint;
        char target[PATH_BYTES];
        if (actual != number && ursatz_gui_material_version_dir(id, actual, target, sizeof(target))) {
            rename(dir, target);
        }
        *out_item = item;
    } else if (made) {
        ursatz_gui_theme_remove_folder(dir);
    }
    cJSON_Delete(request);
    cJSON_Delete(summary);
    cJSON_Delete(brief_summary);
    cJSON_Delete(new_body);
    cJSON_Delete(meta);
    cJSON_Delete(body);
    return result;
}

UrsatzGuiThemeResult ursatz_gui_piece_compose(const char *id, bool has_seed, long long seed, bool update_uses,
                                              cJSON **out_item, char *message, size_t message_size) {
    if (out_item) {
        *out_item = NULL;
    }
    say(message, message_size, "");
    cJSON *item = NULL;
    pthread_mutex_lock(&g_compose_lock);
    UrsatzGuiThemeResult result = compose_locked(id, has_seed, seed, update_uses, &item, message, message_size);
    pthread_mutex_unlock(&g_compose_lock);
    if (result == URSATZ_GUI_THEME_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    return result;
}
