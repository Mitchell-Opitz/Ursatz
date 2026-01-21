#define _XOPEN_SOURCE 700

#include "theme_service.h"

#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "composer_service.h"
#include "material_service.h"
#include "style_service.h"

#define PATH_BYTES 800
#define POOL_MAX 64
#define CHAIN_MAX_STEPS 32
#define CHAIN_TEXT_MAX 400
#define SLOT_COUNT 4

/* One compose at a time: the next version number and its folder are decided before the Composer runs. */
static pthread_mutex_t g_compose_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *const kSlotNames[SLOT_COUNT] = {"basic", "contrasting", "continuation", "cadential"};
static const char *const kMotiveIds[SLOT_COUNT] = {"A", "B", "C", "D"};

static void say(char *message, size_t size, const char *text) {
    if (message && size > 0) {
        snprintf(message, size, "%s", text);
    }
}

static const char *text_of(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : "";
}

/* Adds a copy of src's member name to dst, or null when src lacks it. false on allocation failure. */
static bool add_copy_or_null(cJSON *dst, const cJSON *src, const char *name) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(src, name);
    cJSON *copy = member ? cJSON_Duplicate(member, 1) : cJSON_CreateNull();
    return copy && cJSON_AddItemToObject(dst, name, copy);
}

/* Adds a copy of src's member name to dst when it is there and not null. false on allocation failure. */
static bool add_copy_if(cJSON *dst, const cJSON *src, const char *name) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(src, name);
    if (!member || cJSON_IsNull(member)) {
        return true;
    }
    cJSON *copy = cJSON_Duplicate(member, 1);
    return copy && cJSON_AddItemToObject(dst, name, copy);
}

static void remove_folder(const char *path) {
    DIR *dir = opendir(path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                char file[PATH_BYTES + 256];
                snprintf(file, sizeof(file), "%s/%s", path, entry->d_name);
                unlink(file);
            }
        }
        closedir(dir);
    }
    rmdir(path);
}

static bool write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    size_t length = strlen(text);
    bool ok = file && fwrite(text, 1, length, file) == length;
    if (file && fclose(file) != 0) {
        ok = false;
    }
    return ok;
}

static bool file_exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

/* --- updating sources ---------------------------------------------------- */

/* Moves every updated use of the theme to its source's current version, once for each source. */
static UrsatzGuiThemeResult update_sources(const char *id, char *message, size_t message_size) {
    cJSON *status = NULL;
    UrsatzGuiMaterialResult result = ursatz_gui_material_uses_status(id, &status);
    if (result == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND) {
        return URSATZ_GUI_THEME_ERR_NOT_FOUND;
    }
    if (result != URSATZ_GUI_MATERIAL_OK) {
        say(message, message_size, "the uses could not be read");
        return URSATZ_GUI_THEME_ERR_FAILED;
    }
    UrsatzGuiThemeResult outcome = URSATZ_GUI_THEME_OK;
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, status) {
        if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(use, "updated")) ||
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(use, "missing"))) {
            continue;
        }
        const char *source_id = text_of(use, "item_id");
        bool done = false;
        for (const cJSON *earlier = status; earlier && earlier != use; earlier = earlier->next) {
            done = done || strcmp(text_of(earlier, "item_id"), source_id) == 0;
        }
        if (done) {
            continue;
        }
        result = ursatz_gui_material_update_use(id, source_id, NULL);
        if (result != URSATZ_GUI_MATERIAL_OK) {
            say(message, message_size, "a source could not be updated");
            outcome = URSATZ_GUI_THEME_ERR_FAILED;
            break;
        }
    }
    cJSON_Delete(status);
    return outcome;
}

/* --- the request --------------------------------------------------------- */

/* The copy of a used version, from the body's uses. NULL when the use is not there. */
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

/* A motive of the request from a motif's members. Adds item when it is not NULL (a pool form). */
static cJSON *motive_json(const char *id, const char *item, const cJSON *motif, const char *role) {
    cJSON *object = cJSON_CreateObject();
    bool ok = object && (item ? cJSON_AddStringToObject(object, "item", item) != NULL
                              : cJSON_AddStringToObject(object, "id", id) != NULL);
    ok = ok && cJSON_AddStringToObject(object, "form", "full") && add_copy_or_null(object, motif, "degrees") &&
         add_copy_or_null(object, motif, "pickup") && add_copy_or_null(object, motif, "harmony") &&
         add_copy_or_null(object, motif, "features") && cJSON_AddStringToObject(object, "role", role);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* "op k=v k=v + op k=v" from the stored operations. */
static void chain_text(const cJSON *operations, char *out, size_t size) {
    size_t used = 0;
    out[0] = '\0';
    const cJSON *step = NULL;
    cJSON_ArrayForEach(step, operations) {
        const cJSON *op = cJSON_GetObjectItemCaseSensitive(step, "op");
        if (!cJSON_IsString(op)) {
            continue;
        }
        used += (size_t)snprintf(out + used, size - used, "%s%s", used ? " + " : "", op->valuestring);
        const cJSON *param = NULL;
        cJSON_ArrayForEach(param, cJSON_GetObjectItemCaseSensitive(step, "params")) {
            if (used >= size - 1 || !param->string) {
                break;
            }
            if (cJSON_IsNumber(param)) {
                used += (size_t)snprintf(out + used, size - used, " %s=%g", param->string, param->valuedouble);
            } else if (cJSON_IsString(param)) {
                used += (size_t)snprintf(out + used, size - used, " %s=%s", param->string, param->valuestring);
            } else if (cJSON_IsBool(param)) {
                used += (size_t)snprintf(out + used, size - used, " %s=%s", param->string,
                                         cJSON_IsTrue(param) ? "true" : "false");
            }
        }
        if (used >= size - 1) {
            used = size - 1;
            break;
        }
    }
}

static const cJSON *find_derived(const cJSON *list, const char *id) {
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, list) {
        if (strcmp(text_of(entry, "id"), id) == 0) {
            return entry;
        }
    }
    return NULL;
}

/* True when the derived chain of start_id, followed through source_item for at most 32 steps, reaches target. */
static bool leads_to(const cJSON *list, const char *start_id, const char *target) {
    const char *current = start_id;
    for (int step = 0; step < CHAIN_MAX_STEPS; step++) {
        const cJSON *entry = find_derived(list, current);
        if (!entry) {
            return false;
        }
        const char *source = text_of(cJSON_GetObjectItemCaseSensitive(entry, "derived"), "source_item");
        if (strcmp(source, target) == 0) {
            return true;
        }
        current = source;
    }
    return false;
}

/*
 * Adds to forms the saved forms of the motif item slot_item: the motifs whose derived chain leads to it, approved
 * first, then unflagged when the pool takes them; never rejected, at most 64. pool is the theme's pool setting.
 */
static UrsatzGuiThemeResult add_pool(const cJSON *list, const char *slot_item, const char *pool, cJSON *forms) {
    const char *const flags[2] = {"approved", "unflagged"};
    int passes = strcmp(pool, "none") == 0 ? 0 : strcmp(pool, "approved") == 0 ? 1 : 2;
    for (int pass = 0; pass < passes; pass++) {
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, list) {
            if (cJSON_GetArraySize(forms) >= POOL_MAX) {
                return URSATZ_GUI_THEME_OK;
            }
            const char *id = text_of(entry, "id");
            if (strcmp(id, slot_item) == 0 || !leads_to(list, id, slot_item)) {
                continue;
            }
            cJSON *meta = NULL;
            cJSON *body = NULL;
            if (ursatz_gui_material_item_get(id, &meta, &body) != URSATZ_GUI_MATERIAL_OK) {
                cJSON_Delete(meta);
                cJSON_Delete(body);
                continue;
            }
            const cJSON *motif = cJSON_GetObjectItemCaseSensitive(body, "motif");
            const cJSON *derived = cJSON_GetObjectItemCaseSensitive(motif, "derived");
            const cJSON *degrees = cJSON_GetObjectItemCaseSensitive(motif, "degrees");
            UrsatzGuiThemeResult result = URSATZ_GUI_THEME_OK;
            if (strcmp(text_of(derived, "flag"), flags[pass]) == 0 && strcmp(text_of(motif, "form"), "full") == 0 &&
                strcmp(text_of(motif, "role"), "melodic") == 0 && cJSON_IsString(degrees)) {
                cJSON *form = motive_json(NULL, id, motif, "melodic");
                char chain[CHAIN_TEXT_MAX];
                chain_text(cJSON_GetObjectItemCaseSensitive(derived, "operations"), chain, sizeof(chain));
                const cJSON *functions = cJSON_GetObjectItemCaseSensitive(derived, "functions");
                cJSON *functions_copy = functions ? cJSON_Duplicate(functions, 1) : cJSON_CreateArray();
                if (!form || !functions_copy || !cJSON_AddStringToObject(form, "chain", chain) ||
                    !cJSON_AddItemToObject(form, "functions", functions_copy) ||
                    !cJSON_AddStringToObject(form, "flag", flags[pass])) {
                    if (form && !cJSON_GetObjectItemCaseSensitive(form, "functions")) {
                        cJSON_Delete(functions_copy);
                    }
                    cJSON_Delete(form);
                    result = URSATZ_GUI_THEME_ERR_ALLOC;
                } else {
                    cJSON_AddItemToArray(forms, form);
                }
            }
            cJSON_Delete(meta);
            cJSON_Delete(body);
            if (result != URSATZ_GUI_THEME_OK) {
                return result;
            }
        }
    }
    return URSATZ_GUI_THEME_OK;
}

/* What the request needs from the theme, checked before anything is made. */
static bool composable(const cJSON *theme, bool has_brief, char *message, size_t message_size) {
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    if (!cJSON_IsObject(form) || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(form, "into")) ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(form, "key")) ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(form, "mode")) ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(form, "meter"))) {
        say(message, message_size, "the theme form needs into, key, mode and meter");
        return false;
    }
    if (strcmp(text_of(form, "into"), "hybrid") == 0 && !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(form, "hybrid"))) {
        say(message, message_size, "a hybrid theme form needs hybrid");
        return false;
    }
    if (strcmp(text_of(form, "mode"), "style") == 0 && !has_brief) {
        say(message, message_size, "the mode style needs a brief");
        return false;
    }
    bool has_basic = false;
    const cJSON *slot = NULL;
    cJSON_ArrayForEach(slot, cJSON_GetObjectItemCaseSensitive(theme, "slots")) {
        has_basic = has_basic || strcmp(text_of(slot, "slot"), "basic") == 0;
    }
    if (!has_basic) {
        say(message, message_size, "the theme needs a basic slot");
        return false;
    }
    return true;
}

/*
 * The theme request of the Composer (docs/theme-request.md) for the current version of a theme. style_path is the
 * brief's style file, or NULL without a brief.
 */
static UrsatzGuiThemeResult build_request(const cJSON *body, const char *style_path, cJSON **out, char *message,
                                          size_t message_size) {
    const cJSON *theme = cJSON_GetObjectItemCaseSensitive(body, "theme");
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    cJSON *request = cJSON_CreateObject();
    cJSON *motives = cJSON_CreateArray();
    cJSON *slots = cJSON_CreateObject();
    cJSON *without = NULL;
    cJSON *derived_list = NULL;
    UrsatzGuiThemeResult result = URSATZ_GUI_THEME_OK;
    if (!request || !motives || !slots) {
        result = URSATZ_GUI_THEME_ERR_ALLOC;
    }
    if (result == URSATZ_GUI_THEME_OK && ursatz_gui_material_derived_motifs(&derived_list) != URSATZ_GUI_MATERIAL_OK) {
        say(message, message_size, "the saved forms could not be read");
        result = URSATZ_GUI_THEME_ERR_FAILED;
    }
    int motive_count = 0;
    const cJSON *slot = NULL;
    cJSON_ArrayForEach(slot, cJSON_GetObjectItemCaseSensitive(theme, "slots")) {
        if (result != URSATZ_GUI_THEME_OK) {
            break;
        }
        const char *item_id = text_of(slot, "item_id");
        const cJSON *motif = used_motif(body, item_id, cJSON_GetObjectItemCaseSensitive(slot, "version")->valueint);
        if (!motif || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(motif, "degrees"))) {
            snprintf(message, message_size, "the motif of the %s slot has no degrees", text_of(slot, "slot"));
            result = URSATZ_GUI_THEME_ERR_INVALID;
            break;
        }
        const char *motive_id = kMotiveIds[motive_count++];
        cJSON *motive = motive_json(motive_id, NULL, motif, "melodic");
        cJSON *forms = cJSON_CreateArray();
        if (!motive || !forms || !cJSON_AddItemToObject(motive, "forms", forms)) {
            cJSON_Delete(forms);
            cJSON_Delete(motive);
            result = URSATZ_GUI_THEME_ERR_ALLOC;
            break;
        }
        const char *pool = cJSON_IsString(cJSON_GetObjectItemCaseSensitive(theme, "pool"))
                               ? text_of(theme, "pool")
                               : "approved-and-unflagged";
        result = add_pool(derived_list, item_id, pool, forms);
        if (result != URSATZ_GUI_THEME_OK) {
            cJSON_Delete(motive);
            break;
        }
        cJSON_AddItemToArray(motives, motive);
        if (!cJSON_AddStringToObject(slots, text_of(slot, "slot"), motive_id)) {
            result = URSATZ_GUI_THEME_ERR_ALLOC;
        }
    }

    bool ok = result == URSATZ_GUI_THEME_OK && cJSON_AddStringToObject(request, "schema", "ursatz-theme-request") &&
              cJSON_AddNumberToObject(request, "schema_version", 1);
    static const char *const form_members[] = {"into", "hybrid", "key", "mode", "meter", "bars", "cadence", "first_cadence"};
    for (size_t i = 0; ok && i < sizeof(form_members) / sizeof(form_members[0]); i++) {
        ok = add_copy_if(request, form, form_members[i]);
    }
    for (int s = 0; ok && s < SLOT_COUNT; s++) {
        if (!cJSON_GetObjectItemCaseSensitive(slots, kSlotNames[s])) {
            ok = cJSON_AddNullToObject(slots, kSlotNames[s]) != NULL;
        }
    }
    if (ok) {
        ok = cJSON_AddItemToObject(request, "motives", motives);
        motives = ok ? NULL : motives;
    }
    if (ok) {
        ok = cJSON_AddItemToObject(request, "slots", slots);
        slots = ok ? NULL : slots;
    }

    /* The accompaniment: nothing, a pattern, or a saved motive as the pattern. */
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(theme, "accompaniment");
    if (ok && (!accompaniment || cJSON_IsNull(accompaniment))) {
        ok = cJSON_AddNullToObject(request, "accompaniment") != NULL;
    } else if (ok && strcmp(text_of(accompaniment, "pattern"), "motive") != 0) {
        cJSON *pattern = cJSON_CreateObject();
        ok = pattern && cJSON_AddStringToObject(pattern, "pattern", text_of(accompaniment, "pattern")) &&
             cJSON_AddItemToObject(request, "accompaniment", pattern);
        if (!ok) {
            cJSON_Delete(pattern);
        }
    } else if (ok) {
        const cJSON *motif = used_motif(body, text_of(accompaniment, "item_id"),
                                        cJSON_GetObjectItemCaseSensitive(accompaniment, "version")->valueint);
        if (!motif || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(motif, "degrees"))) {
            say(message, message_size, "the accompaniment motif has no degrees");
            result = URSATZ_GUI_THEME_ERR_INVALID;
            ok = false;
        } else {
            cJSON *pattern = cJSON_CreateObject();
            cJSON *motive = motive_json(NULL, NULL, motif, "accompaniment");
            if (motive) {
                cJSON_DeleteItemFromObjectCaseSensitive(motive, "item");
            }
            ok = pattern && motive && cJSON_AddStringToObject(pattern, "pattern", "motive") &&
                 cJSON_AddItemToObject(pattern, "motive", motive) && cJSON_AddItemToObject(request, "accompaniment", pattern);
            if (!ok) {
                cJSON_Delete(motive && !cJSON_GetObjectItemCaseSensitive(pattern, "motive") ? motive : NULL);
                cJSON_Delete(pattern);
            }
        }
    }
    ok = ok && cJSON_AddNullToObject(request, "voicing") != NULL;
    if (ok && style_path) {
        ok = cJSON_AddStringToObject(request, "style", style_path) && add_copy_if(request, theme, "style_weight");
        without = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(theme, "without"), 1);
        if (ok && without && cJSON_GetArraySize(without) > 0) {
            ok = cJSON_AddItemToObject(request, "without", without);
            without = ok ? NULL : without;
        }
    }
    ok = ok && add_copy_or_null(request, theme, "tempo");

    cJSON_Delete(without);
    cJSON_Delete(derived_list);
    cJSON_Delete(motives);
    cJSON_Delete(slots);
    if (!ok) {
        cJSON_Delete(request);
        return result == URSATZ_GUI_THEME_OK ? URSATZ_GUI_THEME_ERR_ALLOC : result;
    }
    *out = request;
    return URSATZ_GUI_THEME_OK;
}

/* --- the brief ----------------------------------------------------------- */

/*
 * Builds the theme's brief into path. *out_summary is {piece_count, skipped: [{id, reason}]}, caller-owned.
 */
static UrsatzGuiThemeResult build_brief(const cJSON *brief_json, const char *name, const char *path, cJSON **out_summary,
                                        char *message, size_t message_size) {
    UrsatzGuiStyleBrief brief;
    const char *failure = ursatz_gui_style_brief_parse(brief_json, &brief);
    if (failure) {
        say(message, message_size, failure);
        return strcmp(failure, "out of memory") == 0 ? URSATZ_GUI_THEME_ERR_ALLOC : URSATZ_GUI_THEME_ERR_INVALID;
    }
    UrsatzGuiStyleSource source;
    memset(&source, 0, sizeof(source));
    source.kind = URSATZ_GUI_STYLE_SOURCE_BRIEF;
    source.brief = &brief;
    char **pieces = NULL;
    size_t piece_count = 0;
    UrsatzGuiStyleSkipped *skipped = NULL;
    size_t skipped_count = 0;
    UrsatzGuiStyleResult built =
        ursatz_gui_style_build_file(&source, name, path, &pieces, &piece_count, &skipped, &skipped_count);
    ursatz_gui_style_brief_free(&brief);
    UrsatzGuiThemeResult result = URSATZ_GUI_THEME_OK;
    if (built == URSATZ_GUI_STYLE_ERR_NOT_FOUND) {
        say(message, message_size, "a collection, subject, project or style of the brief was not found");
        result = URSATZ_GUI_THEME_ERR_UNPROCESSABLE;
    } else if (built == URSATZ_GUI_STYLE_ERR_NO_PIECES) {
        say(message, message_size, "none of the brief's pieces could be used");
        result = URSATZ_GUI_THEME_ERR_UNPROCESSABLE;
    } else if (built == URSATZ_GUI_STYLE_ERR_INVALID_SOURCE) {
        say(message, message_size, "invalid brief");
        result = URSATZ_GUI_THEME_ERR_INVALID;
    } else if (built != URSATZ_GUI_STYLE_OK) {
        say(message, message_size, "the brief could not be built");
        result = built == URSATZ_GUI_STYLE_ERR_ALLOC ? URSATZ_GUI_THEME_ERR_ALLOC : URSATZ_GUI_THEME_ERR_FAILED;
    }
    cJSON *summary = result == URSATZ_GUI_THEME_OK ? cJSON_CreateObject() : NULL;
    cJSON *list = summary ? cJSON_AddArrayToObject(summary, "skipped") : NULL;
    if (result == URSATZ_GUI_THEME_OK &&
        (!list || !cJSON_AddNumberToObject(summary, "piece_count", (double)piece_count))) {
        result = URSATZ_GUI_THEME_ERR_ALLOC;
    }
    for (size_t i = 0; result == URSATZ_GUI_THEME_OK && i < skipped_count; i++) {
        cJSON *entry = cJSON_CreateObject();
        if (!entry || !cJSON_AddStringToObject(entry, "id", skipped[i].id) ||
            !cJSON_AddStringToObject(entry, "reason", skipped[i].reason)) {
            cJSON_Delete(entry);
            result = URSATZ_GUI_THEME_ERR_ALLOC;
        } else {
            cJSON_AddItemToArray(list, entry);
        }
    }
    for (size_t i = 0; built == URSATZ_GUI_STYLE_OK && i < piece_count; i++) {
        free(pieces[i]);
    }
    free(pieces);
    ursatz_gui_style_skipped_free(skipped, skipped_count);
    if (result != URSATZ_GUI_THEME_OK) {
        cJSON_Delete(summary);
        return result;
    }
    *out_summary = summary;
    return URSATZ_GUI_THEME_OK;
}

/* --- composing ----------------------------------------------------------- */

static UrsatzGuiThemeResult run_theme(const char *dir, bool check, bool has_seed, long long seed, cJSON **out_summary,
                                      char *message, size_t message_size) {
    char request[PATH_BYTES + 32];
    char plan[PATH_BYTES + 32];
    char midi[PATH_BYTES + 32];
    char musicxml[PATH_BYTES + 32];
    char seed_text[32];
    snprintf(request, sizeof(request), "%s/request.json", dir);
    snprintf(plan, sizeof(plan), "%s/plan.json", dir);
    snprintf(midi, sizeof(midi), "%s/theme.mid", dir);
    snprintf(musicxml, sizeof(musicxml), "%s/theme.musicxml", dir);
    snprintf(seed_text, sizeof(seed_text), "%lld", seed);
    const char *args[20];
    int n = 0;
    args[n++] = "theme";
    args[n++] = request;
    args[n++] = "--format";
    args[n++] = "json";
    args[n++] = "--out";
    args[n++] = plan;
    args[n++] = "--midi";
    args[n++] = midi;
    args[n++] = "--musicxml";
    args[n++] = musicxml;
    if (check) {
        args[n++] = "--check";
    }
    if (has_seed) {
        args[n++] = "--seed";
        args[n++] = seed_text;
    }
    args[n] = NULL;

    UrsatzGuiComposerOutput output = {0, NULL, NULL};
    UrsatzGuiComposerResult ran = ursatz_gui_composer_run(args, &output);
    if (ran == URSATZ_GUI_COMPOSER_ERR_NO_BINARY) {
        say(message, message_size, "composer not found");
        return URSATZ_GUI_THEME_ERR_NO_BINARY;
    }
    if (ran == URSATZ_GUI_COMPOSER_ERR_TIMEOUT) {
        say(message, message_size, "composer timed out");
        return URSATZ_GUI_THEME_ERR_TIMEOUT;
    }
    if (ran != URSATZ_GUI_COMPOSER_OK) {
        say(message, message_size, "composer could not be started");
        return URSATZ_GUI_THEME_ERR_FAILED;
    }
    UrsatzGuiThemeResult result = URSATZ_GUI_THEME_OK;
    if (output.exit_status != 0) {
        const char *reason = output.stderr_text && output.stderr_text[0] ? output.stderr_text : "composer failed";
        snprintf(message, message_size, "%s", reason);
        size_t end = strlen(message);
        while (end > 0 && (message[end - 1] == '\n' || message[end - 1] == '\r')) {
            message[--end] = '\0';
        }
        result = output.exit_status == 1   ? URSATZ_GUI_THEME_ERR_INVALID
                 : output.exit_status == 2 ? URSATZ_GUI_THEME_ERR_UNPROCESSABLE
                                           : URSATZ_GUI_THEME_ERR_FAILED;
    } else {
        *out_summary = cJSON_Parse(output.stdout_text);
        if (!*out_summary || !file_exists(plan) || !file_exists(midi) || !file_exists(musicxml)) {
            cJSON_Delete(*out_summary);
            *out_summary = NULL;
            say(message, message_size, "composer output unreadable");
            result = URSATZ_GUI_THEME_ERR_FAILED;
        }
    }
    ursatz_gui_composer_output_destroy(&output);
    return result;
}

/* The seed a run reports: "seed", or the first theme's. */
static const cJSON *reported_seed(const cJSON *summary) {
    const cJSON *seed = cJSON_GetObjectItemCaseSensitive(summary, "seed");
    if (!cJSON_IsNumber(seed)) {
        const cJSON *themes = cJSON_GetObjectItemCaseSensitive(summary, "themes");
        const cJSON *first = cJSON_IsArray(themes) ? cJSON_GetArrayItem(themes, 0) : NULL;
        seed = first ? cJSON_GetObjectItemCaseSensitive(first, "seed") : NULL;
    }
    return cJSON_IsNumber(seed) ? seed : NULL;
}

/* The body of the new version: body plus theme.result. Takes ownership of summary and brief_summary. */
static cJSON *body_with_result(const cJSON *body, cJSON *summary, cJSON *brief_summary, bool has_brief) {
    cJSON *copy = cJSON_Duplicate(body, 1);
    cJSON *theme = cJSON_GetObjectItemCaseSensitive(copy, "theme");
    cJSON *result = cJSON_CreateObject();
    cJSON *files = cJSON_CreateArray();
    const cJSON *seed = reported_seed(summary);
    static const char *const names[] = {"request.json", "brief.style.json", "plan.json", "theme.mid", "theme.musicxml"};
    bool ok = copy && theme && result && files &&
              cJSON_AddItemToObject(result, "seed", seed ? cJSON_CreateNumber(seed->valuedouble) : cJSON_CreateNull());
    for (size_t i = 0; ok && i < sizeof(names) / sizeof(names[0]); i++) {
        if (i != 1 || has_brief) {
            ok = cJSON_AddItemToArray(files, cJSON_CreateString(names[i]));
        }
    }
    ok = ok && cJSON_AddItemToObject(result, "summary", summary);
    if (ok) {
        summary = NULL;
        ok = cJSON_AddItemToObject(result, "brief", brief_summary);
        brief_summary = ok ? NULL : brief_summary;
    }
    ok = ok && cJSON_AddItemToObject(result, "files", files);
    if (ok) {
        files = NULL;
        cJSON_DeleteItemFromObjectCaseSensitive(theme, "result");
        ok = cJSON_AddItemToObject(theme, "result", result);
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
    UrsatzGuiThemeResult result = update_uses ? update_sources(id, message, message_size) : URSATZ_GUI_THEME_OK;
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
    const cJSON *theme = cJSON_GetObjectItemCaseSensitive(body, "theme");
    const cJSON *brief_json = cJSON_GetObjectItemCaseSensitive(theme, "brief");
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

    if (strcmp(text_of(meta, "type"), "theme") != 0 || !cJSON_IsObject(theme)) {
        say(message, message_size, "the item is not a theme");
        result = URSATZ_GUI_THEME_ERR_INVALID;
    } else if (!composable(theme, has_brief, message, message_size)) {
        result = URSATZ_GUI_THEME_ERR_INVALID;
    } else if (!ursatz_gui_material_version_dir(id, number, dir, sizeof(dir))) {
        say(message, message_size, "the version folder does not fit");
        result = URSATZ_GUI_THEME_ERR_FAILED;
    } else {
        remove_folder(dir); /* a folder no version names is left over from a failed run */
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
        result = build_brief(brief_json, name, style_path, &brief_summary, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        result = build_request(body, has_brief ? style_path : NULL, &request, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        char request_path[PATH_BYTES + 32];
        char *text = cJSON_Print(request);
        snprintf(request_path, sizeof(request_path), "%s/request.json", absolute);
        if (!text || !write_text(request_path, text)) {
            say(message, message_size, "the request could not be written");
            result = text ? URSATZ_GUI_THEME_ERR_FAILED : URSATZ_GUI_THEME_ERR_ALLOC;
        }
        free(text);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        const cJSON *body_seed = cJSON_GetObjectItemCaseSensitive(theme, "seed");
        bool use_seed = has_seed || cJSON_IsNumber(body_seed);
        long long chosen = has_seed ? seed : use_seed ? (long long)body_seed->valuedouble : 0;
        result = run_theme(absolute, cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(theme, "check")), use_seed, chosen,
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
        remove_folder(dir);
    }
    cJSON_Delete(request);
    cJSON_Delete(summary);
    cJSON_Delete(brief_summary);
    cJSON_Delete(new_body);
    cJSON_Delete(meta);
    cJSON_Delete(body);
    return result;
}

UrsatzGuiThemeResult ursatz_gui_theme_compose(const char *id, bool has_seed, long long seed, bool update_uses,
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
