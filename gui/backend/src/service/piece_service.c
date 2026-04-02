#define _XOPEN_SOURCE 700

#include "piece_service.h"

#include <dirent.h>
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

/*
 * Runs the piece command into dir (absolute) on dir/request.json. With from (a loop plan), it writes section regenerate
 * of that plan again ("none": only the ending version), in lane when that is not NULL.
 */
static UrsatzGuiThemeResult run_piece(const char *dir, bool check, bool has_seed, long long seed, const char *from,
                                      const char *regenerate, const char *lane, cJSON **out_summary, char *message,
                                      size_t message_size) {
    char paths[FILE_NAME_COUNT][PATH_BYTES + 32];
    for (int i = 0; i < FILE_NAME_COUNT; i++) {
        snprintf(paths[i], sizeof(paths[i]), "%s/%s", dir, kFileNames[i]);
    }
    char seed_text[32];
    snprintf(seed_text, sizeof(seed_text), "%lld", seed);
    const char *args[32];
    int n = 0;
    args[n++] = "piece";
    if (from) {
        args[n++] = "--from";
        args[n++] = from;
        args[n++] = "--request";
        args[n++] = paths[0];
        args[n++] = "--regenerate";
        args[n++] = regenerate;
        if (lane) {
            args[n++] = "--lane";
            args[n++] = lane;
        }
    } else {
        args[n++] = paths[0];
    }
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
                           NULL, NULL, NULL, &summary, message, message_size);
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

/* --- reopening a composed version ---------------------------------------- */

static char *read_text(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    size_t capacity = 8192;
    size_t length = 0;
    char *text = malloc(capacity);
    while (text) {
        size_t n = fread(text + length, 1, capacity - length - 1, file);
        length += n;
        if (n == 0) {
            break;
        }
        if (length + 1 >= capacity) {
            char *grown = realloc(text, capacity * 2);
            if (!grown) {
                free(text);
                text = NULL;
                break;
            }
            text = grown;
            capacity *= 2;
        }
    }
    fclose(file);
    if (text) {
        text[length] = '\0';
    }
    return text;
}

static cJSON *read_json(const char *path) {
    char *text = read_text(path);
    cJSON *json = text ? cJSON_Parse(text) : NULL;
    free(text);
    return json;
}

/* Replaces (or adds) the string member name of object. */
static bool set_string(cJSON *object, const char *name, const char *text) {
    cJSON_DeleteItemFromObjectCaseSensitive(object, name);
    return cJSON_AddStringToObject(object, name, text) != NULL;
}

static bool lane_ok(const char *lane) {
    static const char *const lanes[] = {"harmony", "bass", "melody", "inner", "all"};
    for (size_t i = 0; lane && i < sizeof(lanes) / sizeof(lanes[0]); i++) {
        if (strcmp(lane, lanes[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* A version of a piece to work from: its item.json, its body and its folder (absolute). */
typedef struct {
    cJSON *meta;
    cJSON *body;
    const cJSON *section; /* the section asked for, or NULL */
    char dir[PATH_BYTES];
    int current;
} Source;

static void source_clear(Source *source) {
    cJSON_Delete(source->meta);
    cJSON_Delete(source->body);
    memset(source, 0, sizeof(*source));
}

/*
 * Opens version of piece item id: it must be composed (a result, a plan and a request). section_id (not NULL) must name a
 * section of the loop, and lane (not NULL) a lane that section can be written again in.
 */
static UrsatzGuiThemeResult open_source(const char *id, int version, const char *section_id, const char *lane, Source *source,
                                        char *message, size_t message_size) {
    memset(source, 0, sizeof(*source));
    cJSON *current_body = NULL;
    UrsatzGuiMaterialResult read = ursatz_gui_material_item_get(id, &source->meta, &current_body);
    cJSON_Delete(current_body);
    if (read != URSATZ_GUI_MATERIAL_OK) {
        source_clear(source);
        return read == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND || read == URSATZ_GUI_MATERIAL_ERR_INVALID_ID
                   ? URSATZ_GUI_THEME_ERR_NOT_FOUND
                   : URSATZ_GUI_THEME_ERR_FAILED;
    }
    source->current = cJSON_GetObjectItemCaseSensitive(source->meta, "current_version")->valueint;
    if (strcmp(text_of(source->meta, "type"), "piece") != 0) {
        say(message, message_size, "the item is not a piece");
        source_clear(source);
        return URSATZ_GUI_THEME_ERR_INVALID;
    }
    read = ursatz_gui_material_item_get_version(id, version, &source->body);
    if (read != URSATZ_GUI_MATERIAL_OK) {
        source_clear(source);
        say(message, message_size, "version not found");
        return read == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND ? URSATZ_GUI_THEME_ERR_NOT_FOUND : URSATZ_GUI_THEME_ERR_FAILED;
    }
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(source->body, "piece");
    char relative[PATH_BYTES];
    char path[PATH_BYTES + 32];
    char resolved[PATH_MAX];
    bool composed = cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(piece, "result")) &&
                    ursatz_gui_material_version_dir(id, version, relative, sizeof(relative)) && realpath(relative, resolved) &&
                    snprintf(source->dir, sizeof(source->dir), "%s", resolved) < (int)sizeof(source->dir);
    if (composed) {
        snprintf(path, sizeof(path), "%s/plan.json", source->dir);
        composed = access(path, R_OK) == 0;
        snprintf(path, sizeof(path), "%s/request.json", source->dir);
        composed = composed && access(path, R_OK) == 0;
    }
    if (!composed) {
        say(message, message_size, "this version was not composed; compose the piece first");
        source_clear(source);
        return URSATZ_GUI_THEME_ERR_INVALID;
    }
    if (!section_id) {
        return URSATZ_GUI_THEME_OK;
    }
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(piece, "structure"), "sections")) {
        if (strcmp(text_of(section, "id"), section_id) == 0) {
            source->section = section;
            break;
        }
    }
    const cJSON *fill = cJSON_GetObjectItemCaseSensitive(source->section, "fill");
    if (!source->section) {
        snprintf(message, message_size, "the piece has no section %s", section_id);
    } else if (strcmp(text_of(source->section, "part"), "ending") == 0) {
        snprintf(message, message_size, "section %s is in the ending, which is built from the loop; write a section of the loop again",
                 section_id);
    } else if (lane && !lane_ok(lane)) {
        say(message, message_size, "lane is one of harmony, bass, melody, inner, all");
    } else if (cJSON_IsObject(fill) && strcmp(text_of(fill, "kind"), "theme") == 0 &&
               (!lane || (strcmp(lane, "harmony") != 0 && strcmp(lane, "bass") != 0))) {
        snprintf(message, message_size, "section %s is filled with a theme: only its harmony or its bass can be written again", section_id);
    } else {
        return URSATZ_GUI_THEME_OK;
    }
    source_clear(source);
    return URSATZ_GUI_THEME_ERR_INVALID;
}

/* Clears a leftover folder for version number of item id and makes it; dir is its path, absolute the same resolved. */
static UrsatzGuiThemeResult make_folder(const char *id, int number, char *dir, size_t dir_size, char *absolute,
                                        size_t absolute_size, char *message, size_t message_size) {
    char resolved[PATH_MAX];
    if (!ursatz_gui_material_version_dir(id, number, dir, dir_size)) {
        say(message, message_size, "the version folder does not fit");
        return URSATZ_GUI_THEME_ERR_FAILED;
    }
    ursatz_gui_theme_remove_folder(dir); /* a folder no version names is left over from a failed run */
    if (mkdir(dir, 0755) != 0 || !realpath(dir, resolved) || snprintf(absolute, absolute_size, "%s", resolved) >= (int)absolute_size) {
        say(message, message_size, "the version folder could not be made");
        return URSATZ_GUI_THEME_ERR_FAILED;
    }
    return URSATZ_GUI_THEME_OK;
}

/*
 * Copies the request of the version in from_dir into dir with the files it names: the plan of each theme fill and the
 * brief's style file are copied beside it, and the request names the copies, so the new version keeps its own.
 */
static UrsatzGuiThemeResult carry_request(const char *from_dir, const char *dir, char *message, size_t message_size) {
    char path[PATH_BYTES + 96];
    char copy[PATH_BYTES + 96];
    snprintf(path, sizeof(path), "%s/request.json", from_dir);
    cJSON *request = read_json(path);
    if (!request) {
        say(message, message_size, "the request of the version could not be read");
        return URSATZ_GUI_THEME_ERR_FAILED;
    }
    UrsatzGuiThemeResult result = URSATZ_GUI_THEME_OK;
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(request, "sections")) {
        cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        if (result == URSATZ_GUI_THEME_OK && cJSON_IsObject(fill) && strcmp(text_of(fill, "kind"), "theme") == 0) {
            char name[96];
            snprintf(name, sizeof(name), "theme-%s.plan.json", text_of(section, "id"));
            snprintf(path, sizeof(path), "%s/%s", from_dir, name);
            snprintf(copy, sizeof(copy), "%s/%s", dir, name);
            if (!copy_file(path, copy) || !set_string(fill, "plan", copy)) {
                snprintf(message, message_size, "the plan of the theme in section %s could not be copied", text_of(section, "id"));
                result = URSATZ_GUI_THEME_ERR_FAILED;
            }
        }
    }
    if (result == URSATZ_GUI_THEME_OK && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(request, "style"))) {
        snprintf(path, sizeof(path), "%s/brief.style.json", from_dir);
        snprintf(copy, sizeof(copy), "%s/brief.style.json", dir);
        if (!copy_file(path, copy) || !set_string(request, "style", copy)) {
            say(message, message_size, "the style file of the version could not be copied");
            result = URSATZ_GUI_THEME_ERR_FAILED;
        }
    }
    if (result == URSATZ_GUI_THEME_OK) {
        char *text = cJSON_Print(request);
        snprintf(copy, sizeof(copy), "%s/request.json", dir);
        if (!text || !ursatz_gui_theme_write_text(copy, text)) {
            say(message, message_size, "the request could not be written");
            result = text ? URSATZ_GUI_THEME_ERR_FAILED : URSATZ_GUI_THEME_ERR_ALLOC;
        }
        free(text);
    }
    cJSON_Delete(request);
    return result;
}

/* A copy of the brief summary the source version kept, else the empty one. NULL on allocation failure. */
static cJSON *kept_brief_summary(const Source *source) {
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(source->body, "piece");
    const cJSON *brief = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(piece, "result"), "brief");
    if (cJSON_IsObject(brief)) {
        return cJSON_Duplicate(brief, 1);
    }
    cJSON *empty = cJSON_CreateObject();
    if (!empty || !cJSON_AddNumberToObject(empty, "piece_count", 0) || !cJSON_AddArrayToObject(empty, "skipped")) {
        cJSON_Delete(empty);
        return NULL;
    }
    return empty;
}

/* Writes new_body as the next version, whose folder is dir. Another writer may have added a version meanwhile. */
static UrsatzGuiThemeResult add_version(const char *id, int number, const char *dir, const cJSON *new_body, const char *note,
                                        cJSON **out_item, char *message, size_t message_size) {
    cJSON *item = NULL;
    if (ursatz_gui_material_item_add_version(id, new_body, note, &item) != URSATZ_GUI_MATERIAL_OK) {
        say(message, message_size, "the version could not be written");
        return URSATZ_GUI_THEME_ERR_FAILED;
    }
    int actual = cJSON_GetObjectItemCaseSensitive(item, "current_version")->valueint;
    char target[PATH_BYTES];
    if (actual != number && ursatz_gui_material_version_dir(id, actual, target, sizeof(target))) {
        rename(dir, target);
    }
    *out_item = item;
    return URSATZ_GUI_THEME_OK;
}

static UrsatzGuiThemeResult regenerate_locked(const char *id, int version, const char *section_id, const char *lane,
                                              bool has_seed, long long seed, cJSON **out_item, char *message,
                                              size_t message_size) {
    Source source;
    UrsatzGuiThemeResult result = open_source(id, version, section_id, lane, &source, message, message_size);
    if (result != URSATZ_GUI_THEME_OK) {
        return result;
    }
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(source.body, "piece");
    const cJSON *brief_json = cJSON_GetObjectItemCaseSensitive(piece, "brief");
    bool has_brief = brief_json && !cJSON_IsNull(brief_json);
    int number = source.current + 1;
    char dir[PATH_BYTES];
    char absolute[PATH_BYTES];
    char from[PATH_BYTES + 32];
    cJSON *summary = NULL;
    cJSON *new_body = NULL;
    cJSON *item = NULL;
    result = make_folder(id, number, dir, sizeof(dir), absolute, sizeof(absolute), message, message_size);
    bool made = result == URSATZ_GUI_THEME_OK;
    if (result == URSATZ_GUI_THEME_OK) {
        result = carry_request(source.dir, absolute, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        snprintf(from, sizeof(from), "%s/plan.json", source.dir);
        result = run_piece(absolute, cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(piece, "check")), has_seed, seed, from,
                           section_id, lane, &summary, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        cJSON *brief_summary = kept_brief_summary(&source);
        const cJSON *reported = reported_seed(summary);
        char note[96];
        if (reported) {
            snprintf(note, sizeof(note), "regenerated %s %s seed %.0f", section_id, lane ? lane : "all", reported->valuedouble);
        } else {
            snprintf(note, sizeof(note), "regenerated %s %s", section_id, lane ? lane : "all");
        }
        new_body = brief_summary ? body_with_result(source.body, summary, brief_summary, has_brief) : NULL;
        summary = NULL;
        if (!new_body) {
            result = URSATZ_GUI_THEME_ERR_ALLOC;
        } else {
            result = add_version(id, number, dir, new_body, note, &item, message, message_size);
        }
    }
    if (result == URSATZ_GUI_THEME_OK) {
        *out_item = item;
    } else if (made) {
        ursatz_gui_theme_remove_folder(dir);
    }
    cJSON_Delete(summary);
    cJSON_Delete(new_body);
    source_clear(&source);
    return result;
}

UrsatzGuiThemeResult ursatz_gui_piece_regenerate(const char *id, int version, const char *section_id, const char *lane,
                                                 bool has_seed, long long seed, cJSON **out_item, char *message,
                                                 size_t message_size) {
    if (out_item) {
        *out_item = NULL;
    }
    say(message, message_size, "");
    cJSON *item = NULL;
    pthread_mutex_lock(&g_compose_lock);
    UrsatzGuiThemeResult result = regenerate_locked(id, version, section_id, lane, has_seed, seed, &item, message, message_size);
    pthread_mutex_unlock(&g_compose_lock);
    if (result == URSATZ_GUI_THEME_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    return result;
}

/* --- variations ---------------------------------------------------------- */

/* The number of the next variations set of the item: one past the highest of any version's folder. */
static int next_set(const char *id, int current) {
    int highest = 0;
    for (int number = 1; number <= current; number++) {
        char dir[PATH_BYTES];
        if (!ursatz_gui_material_version_dir(id, number, dir, sizeof(dir))) {
            continue;
        }
        DIR *folder = opendir(dir);
        struct dirent *entry;
        while (folder && (entry = readdir(folder)) != NULL) {
            char *end = NULL;
            if (strncmp(entry->d_name, "variations-", 11) == 0) {
                long set = strtol(entry->d_name + 11, &end, 10);
                if (end != entry->d_name + 11 && *end == '\0' && set > highest && set < 1000000) {
                    highest = (int)set;
                }
            }
        }
        if (folder) {
            closedir(folder);
        }
    }
    return highest + 1;
}

static const cJSON *loop_section(const cJSON *loop, const char *section_id) {
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(loop, "sections")) {
        if (strcmp(text_of(section, "id"), section_id) == 0) {
            return section;
        }
    }
    return NULL;
}

/* A copy of member, or a null when there is none. */
static cJSON *copy_or_null(const cJSON *member) {
    return member ? cJSON_Duplicate(member, 1) : cJSON_CreateNull();
}

/* {set, variations: [{index, seed, score, seam, heard}]} from the Composer's result.json. NULL on allocation failure. */
static cJSON *variations_answer(const cJSON *result, const char *section_id, int set) {
    cJSON *answer = cJSON_CreateObject();
    cJSON *list = cJSON_CreateArray();
    bool ok = answer && list && cJSON_AddNumberToObject(answer, "set", set);
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(result, "variations")) {
        const cJSON *loop = cJSON_GetObjectItemCaseSensitive(entry, "loop");
        const cJSON *heard = cJSON_GetObjectItemCaseSensitive(loop_section(loop, section_id), "heard");
        cJSON *object = ok ? cJSON_CreateObject() : NULL;
        ok = object && add_copy_or_null(object, entry, "index") && add_copy_or_null(object, entry, "seed") &&
             attach(object, "score", copy_or_null(cJSON_GetObjectItemCaseSensitive(loop, "score"))) &&
             attach(object, "seam", copy_or_null(cJSON_GetObjectItemCaseSensitive(loop, "seam"))) &&
             attach(object, "heard", copy_or_null(heard)) &&
             cJSON_AddItemToArray(list, object);
        if (!ok) {
            cJSON_Delete(object);
        }
    }
    if (ok) {
        ok = attach(answer, "variations", list);
        list = NULL;
    }
    cJSON_Delete(list);
    if (!ok) {
        cJSON_Delete(answer);
        return NULL;
    }
    return answer;
}

static UrsatzGuiThemeResult variations_locked(const char *id, int version, const char *section_id, const char *lane, int count,
                                              bool has_seed, long long seed, cJSON **out_result, char *message,
                                              size_t message_size) {
    if (!lane || count < 1 || count > 8) {
        say(message, message_size, "variations need a lane and a count from 1 to 8");
        return URSATZ_GUI_THEME_ERR_INVALID;
    }
    Source source;
    UrsatzGuiThemeResult result = open_source(id, version, section_id, lane, &source, message, message_size);
    if (result != URSATZ_GUI_THEME_OK) {
        return result;
    }
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(source.body, "piece");
    int set = next_set(id, source.current);
    char out_dir[PATH_BYTES + 32];
    char from[PATH_BYTES + 32];
    char request[PATH_BYTES + 32];
    char result_path[PATH_BYTES + 64];
    char count_text[16];
    char seed_text[32];
    snprintf(out_dir, sizeof(out_dir), "%s/variations-%d", source.dir, set);
    snprintf(from, sizeof(from), "%s/plan.json", source.dir);
    snprintf(request, sizeof(request), "%s/request.json", source.dir);
    snprintf(result_path, sizeof(result_path), "%s/result.json", out_dir);
    snprintf(count_text, sizeof(count_text), "%d", count);
    snprintf(seed_text, sizeof(seed_text), "%lld", seed);
    const char *args[24];
    int n = 0;
    args[n++] = "piece";
    args[n++] = "--from";
    args[n++] = from;
    args[n++] = "--request";
    args[n++] = request;
    args[n++] = "--variations";
    args[n++] = section_id;
    args[n++] = "--lane";
    args[n++] = lane;
    args[n++] = "--count";
    args[n++] = count_text;
    args[n++] = "--out-dir";
    args[n++] = out_dir;
    args[n++] = "--format";
    args[n++] = "json";
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(piece, "check"))) {
        args[n++] = "--check";
    }
    if (has_seed) {
        args[n++] = "--seed";
        args[n++] = seed_text;
    }
    args[n] = NULL;
    const char *outputs[] = {result_path, NULL};
    cJSON *summary = NULL;
    result = ursatz_gui_theme_run_composer(args, outputs, &summary, message, message_size);
    cJSON *written = result == URSATZ_GUI_THEME_OK ? read_json(result_path) : NULL;
    if (result == URSATZ_GUI_THEME_OK) {
        *out_result = written ? variations_answer(written, section_id, set) : NULL;
        if (!*out_result) {
            say(message, message_size, "the variations could not be read");
            result = URSATZ_GUI_THEME_ERR_FAILED;
        }
    }
    if (result != URSATZ_GUI_THEME_OK) {
        ursatz_gui_theme_remove_folder(out_dir);
    }
    cJSON_Delete(written);
    cJSON_Delete(summary);
    source_clear(&source);
    return result;
}

UrsatzGuiThemeResult ursatz_gui_piece_variations(const char *id, int version, const char *section_id, const char *lane,
                                                 int count, bool has_seed, long long seed, cJSON **out_result,
                                                 char *message, size_t message_size) {
    if (out_result) {
        *out_result = NULL;
    }
    say(message, message_size, "");
    cJSON *answer = NULL;
    pthread_mutex_lock(&g_compose_lock);
    UrsatzGuiThemeResult result =
        variations_locked(id, version, section_id, lane, count, has_seed, seed, &answer, message, message_size);
    pthread_mutex_unlock(&g_compose_lock);
    if (result == URSATZ_GUI_THEME_OK && out_result) {
        *out_result = answer;
    } else {
        cJSON_Delete(answer);
    }
    return result;
}

static UrsatzGuiThemeResult choose_locked(const char *id, int version, int set, int index, cJSON **out_item, char *message,
                                          size_t message_size) {
    Source source;
    UrsatzGuiThemeResult result = open_source(id, version, NULL, NULL, &source, message, message_size);
    if (result != URSATZ_GUI_THEME_OK) {
        return result;
    }
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(source.body, "piece");
    const cJSON *brief_json = cJSON_GetObjectItemCaseSensitive(piece, "brief");
    bool has_brief = brief_json && !cJSON_IsNull(brief_json);
    char path[PATH_BYTES + 96];
    char from[PATH_BYTES + 96];
    snprintf(path, sizeof(path), "%s/variations-%d/result.json", source.dir, set);
    snprintf(from, sizeof(from), "%s/variations-%d/variation-%d.json", source.dir, set, index);
    cJSON *written = read_json(path);
    const cJSON *chosen = NULL;
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(written, "variations")) {
        const cJSON *number = cJSON_GetObjectItemCaseSensitive(entry, "index");
        if (cJSON_IsNumber(number) && number->valueint == index) {
            chosen = entry;
        }
    }
    if (!chosen || access(from, R_OK) != 0) {
        snprintf(message, message_size, "variation %d is not in set %d of version %d", index, set, version);
        cJSON_Delete(written);
        source_clear(&source);
        return URSATZ_GUI_THEME_ERR_NOT_FOUND;
    }
    int number = source.current + 1;
    char dir[PATH_BYTES];
    char absolute[PATH_BYTES];
    cJSON *summary = NULL;
    cJSON *new_body = NULL;
    cJSON *item = NULL;
    result = make_folder(id, number, dir, sizeof(dir), absolute, sizeof(absolute), message, message_size);
    bool made = result == URSATZ_GUI_THEME_OK;
    if (result == URSATZ_GUI_THEME_OK) {
        result = carry_request(source.dir, absolute, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        result = run_piece(absolute, false, false, 0, from, "none", NULL, &summary, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        /* The Composer's run only built the ending version; the loop version is the variation's, with its seed. */
        const cJSON *seed = cJSON_GetObjectItemCaseSensitive(chosen, "seed");
        cJSON *versions = cJSON_GetObjectItemCaseSensitive(summary, "versions");
        const cJSON *loop = cJSON_GetObjectItemCaseSensitive(chosen, "loop");
        bool ok = cJSON_IsNumber(seed) && cJSON_IsObject(versions) && cJSON_IsObject(loop);
        if (ok) {
            cJSON_DeleteItemFromObjectCaseSensitive(versions, "loop");
            cJSON_DeleteItemFromObjectCaseSensitive(summary, "seed");
            ok = attach(versions, "loop", cJSON_Duplicate(loop, 1)) && attach(summary, "seed", cJSON_CreateNumber(seed->valuedouble));
        }
        cJSON *brief_summary = ok ? kept_brief_summary(&source) : NULL;
        char note[96];
        snprintf(note, sizeof(note), "chose variation %d seed %.0f", index, ok ? seed->valuedouble : 0.0);
        new_body = brief_summary ? body_with_result(source.body, summary, brief_summary, has_brief) : NULL;
        summary = NULL;
        if (!new_body) {
            say(message, message_size, "the version could not be made from the variation");
            result = URSATZ_GUI_THEME_ERR_ALLOC;
        } else {
            result = add_version(id, number, dir, new_body, note, &item, message, message_size);
        }
    }
    if (result == URSATZ_GUI_THEME_OK) {
        *out_item = item;
    } else if (made) {
        ursatz_gui_theme_remove_folder(dir);
    }
    cJSON_Delete(written);
    cJSON_Delete(summary);
    cJSON_Delete(new_body);
    source_clear(&source);
    return result;
}

UrsatzGuiThemeResult ursatz_gui_piece_choose(const char *id, int version, int set, int index, cJSON **out_item,
                                             char *message, size_t message_size) {
    if (out_item) {
        *out_item = NULL;
    }
    say(message, message_size, "");
    cJSON *item = NULL;
    pthread_mutex_lock(&g_compose_lock);
    UrsatzGuiThemeResult result = choose_locked(id, version, set, index, &item, message, message_size);
    pthread_mutex_unlock(&g_compose_lock);
    if (result == URSATZ_GUI_THEME_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    return result;
}

/* The content type of a set file name: result.json, or variation-<i>.json, .mid, .musicxml (i a number of one or two digits). */
static const char *variation_file_type(const char *name) {
    if (strcmp(name, "result.json") == 0) {
        return "application/json";
    }
    if (strncmp(name, "variation-", 10) != 0) {
        return NULL;
    }
    size_t digits = strspn(name + 10, "0123456789");
    const char *extension = name + 10 + digits;
    if (digits < 1 || digits > 2) {
        return NULL;
    }
    if (strcmp(extension, ".json") == 0) {
        return "application/json";
    }
    if (strcmp(extension, ".mid") == 0) {
        return "audio/midi";
    }
    return strcmp(extension, ".musicxml") == 0 ? "application/xml" : NULL;
}

UrsatzGuiThemeResult ursatz_gui_piece_variation_file(const char *id, int version, int set, const char *name,
                                                     unsigned char **out_data, size_t *out_size, const char **out_type) {
    *out_data = NULL;
    *out_size = 0;
    const char *type = name ? variation_file_type(name) : NULL;
    cJSON *meta = NULL;
    cJSON *body = NULL;
    char dir[PATH_BYTES];
    char path[PATH_BYTES + 96];
    if (!type || set < 1 || !ursatz_gui_material_version_dir(id, version, dir, sizeof(dir)) ||
        ursatz_gui_material_item_get(id, &meta, &body) != URSATZ_GUI_MATERIAL_OK) {
        return URSATZ_GUI_THEME_ERR_NOT_FOUND;
    }
    bool found = version <= cJSON_GetObjectItemCaseSensitive(meta, "current_version")->valueint &&
                 strcmp(text_of(meta, "type"), "piece") == 0;
    cJSON_Delete(meta);
    cJSON_Delete(body);
    snprintf(path, sizeof(path), "%s/variations-%d/%s", dir, set, name);
    FILE *file = found ? fopen(path, "rb") : NULL;
    if (!file) {
        return URSATZ_GUI_THEME_ERR_NOT_FOUND;
    }
    size_t capacity = 65536;
    size_t length = 0;
    unsigned char *data = malloc(capacity);
    while (data) {
        size_t n = fread(data + length, 1, capacity - length, file);
        length += n;
        if (n == 0) {
            break;
        }
        if (length == capacity) {
            unsigned char *grown = realloc(data, capacity * 2);
            if (!grown) {
                free(data);
                data = NULL;
                break;
            }
            data = grown;
            capacity *= 2;
        }
    }
    fclose(file);
    if (!data) {
        return URSATZ_GUI_THEME_ERR_ALLOC;
    }
    *out_data = data;
    *out_size = length;
    *out_type = type;
    return URSATZ_GUI_THEME_OK;
}
