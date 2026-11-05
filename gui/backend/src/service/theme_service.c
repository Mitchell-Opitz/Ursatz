#define _XOPEN_SOURCE 700

#include "theme_service.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "compose_common.h"
#include "material_service.h"
#include "style_service.h"
#include "util/fs_util.h"
#include "util/json_util.h"

#define PATH_BYTES 800
#define POOL_MAX 64
#define CHAIN_MAX_STEPS 32
#define CHAIN_TEXT_MAX 400
#define SLOT_COUNT 4
#define MOTIVE_MAX 16
#define MOTIVE_ID_BYTES 16

static const char *const kSlotNames[SLOT_COUNT] = {"basic", "contrasting", "continuation", "cadential"};

void ursatz_gui_theme_remove_folder(const char *path) {
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

bool ursatz_gui_theme_write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    size_t length = strlen(text);
    bool ok = file && fwrite(text, 1, length, file) == length;
    if (file && fclose(file) != 0) {
        ok = false;
    }
    return ok;
}

/* --- updating sources ---------------------------------------------------- */

/* Moves every updated use of the theme to its source's current version, once for each source. */
UrsatzGuiThemeResult ursatz_gui_theme_update_sources(const char *id, char *message, size_t message_size) {
    cJSON *status = NULL;
    UrsatzGuiMaterialResult result = ursatz_gui_material_uses_status(id, &status);
    if (result == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND) {
        return URSATZ_GUI_THEME_ERR_NOT_FOUND;
    }
    if (result != URSATZ_GUI_MATERIAL_OK) {
        ursatz_gui_copy_message(message, message_size, "the uses could not be read");
        return URSATZ_GUI_THEME_ERR_FAILED;
    }
    UrsatzGuiThemeResult outcome = URSATZ_GUI_THEME_OK;
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, status) {
        if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(use, "updated")) ||
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(use, "missing"))) {
            continue;
        }
        const char *source_id = ursatz_gui_json_string(use, "item_id");
        bool done = false;
        for (const cJSON *earlier = status; earlier && earlier != use; earlier = earlier->next) {
            done = done || strcmp(ursatz_gui_json_string(earlier, "item_id"), source_id) == 0;
        }
        if (done) {
            continue;
        }
        result = ursatz_gui_material_update_use(id, source_id, NULL);
        if (result != URSATZ_GUI_MATERIAL_OK) {
            ursatz_gui_copy_message(message, message_size, "a source could not be updated");
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
        if (strcmp(ursatz_gui_json_string(use, "item_id"), item_id) == 0 && cJSON_IsNumber(number) && number->valueint == version) {
            return cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(use, "copy"), "motif");
        }
    }
    return NULL;
}

/* A motive of the request from a motif's members: with item (a pool form), else with id (a slot motive); the accompaniment
   motive has neither. */
cJSON *ursatz_gui_theme_motive_json(const char *id, const char *item, const cJSON *motif, const char *role) {
    cJSON *object = cJSON_CreateObject();
    bool ok = object && (item ? cJSON_AddStringToObject(object, "item", item) != NULL
                              : !id || cJSON_AddStringToObject(object, "id", id) != NULL);
    ok = ok && cJSON_AddStringToObject(object, "form", "full") && ursatz_gui_json_add_copy_or_null(object, motif, "degrees") &&
         ursatz_gui_json_add_copy_or_null(object, motif, "pickup") && ursatz_gui_json_add_copy_or_null(object, motif, "harmony") &&
         ursatz_gui_json_add_copy_or_null(object, motif, "features") && cJSON_AddStringToObject(object, "role", role);
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
        if (strcmp(ursatz_gui_json_string(entry, "id"), id) == 0) {
            return entry;
        }
    }
    return NULL;
}

/* True when the derived chain of start_id, followed through source_item for at most 32 steps, reaches target. A
   standalone motif (R-192) is a motif of its own: the chain stops at it, so it and its variants never lead to the
   motif it was promoted from. */
static bool leads_to(const cJSON *list, const char *start_id, const char *target) {
    const char *current = start_id;
    for (int step = 0; step < CHAIN_MAX_STEPS; step++) {
        const cJSON *entry = find_derived(list, current);
        if (!entry) {
            return false;
        }
        const cJSON *derived = cJSON_GetObjectItemCaseSensitive(entry, "derived");
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(derived, "standalone"))) {
            return false;
        }
        const char *source = ursatz_gui_json_string(derived, "source_item");
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
UrsatzGuiThemeResult ursatz_gui_theme_add_pool(const cJSON *list, const char *slot_item, const char *pool, cJSON *forms) {
    const char *const flags[2] = {"approved", "unflagged"};
    int passes = strcmp(pool, "none") == 0 ? 0 : strcmp(pool, "approved") == 0 ? 1 : 2;
    for (int pass = 0; pass < passes; pass++) {
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, list) {
            if (cJSON_GetArraySize(forms) >= POOL_MAX) {
                return URSATZ_GUI_THEME_OK;
            }
            const char *id = ursatz_gui_json_string(entry, "id");
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
            if (strcmp(ursatz_gui_json_string(derived, "flag"), flags[pass]) == 0 && strcmp(ursatz_gui_json_string(motif, "form"), "full") == 0 &&
                strcmp(ursatz_gui_json_string(motif, "role"), "melodic") == 0 && cJSON_IsString(degrees)) {
                cJSON *form = ursatz_gui_theme_motive_json(NULL, id, motif, "melodic");
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
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(form, "mode")) ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(form, "meter"))) {
        ursatz_gui_copy_message(message, message_size, "the theme form needs into, mode and meter");
        return false;
    }
    if (strcmp(ursatz_gui_json_string(form, "into"), "hybrid") == 0 && !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(form, "hybrid"))) {
        ursatz_gui_copy_message(message, message_size, "a hybrid theme form needs hybrid");
        return false;
    }
    if (strcmp(ursatz_gui_json_string(form, "mode"), "style") == 0 && !has_brief) {
        ursatz_gui_copy_message(message, message_size, "the mode style needs a brief");
        return false;
    }
    bool has_basic = false;
    const cJSON *slot = NULL;
    cJSON_ArrayForEach(slot, cJSON_GetObjectItemCaseSensitive(theme, "slots")) {
        has_basic = has_basic || strcmp(ursatz_gui_json_string(slot, "slot"), "basic") == 0;
    }
    if (!has_basic) {
        ursatz_gui_copy_message(message, message_size, "the theme needs a basic slot");
        return false;
    }
    return true;
}

/* The motives of a request, one for each distinct motif version. */
typedef struct {
    const char *item_id[MOTIVE_MAX];
    int version[MOTIVE_MAX];
    int count;
} MotiveSet;

/* A motive of a theme request: the motif's degrees, pickup and features, its base harmony (its own harmony when it has one,
   else the proposed one of its analysis) and its tone overrides. */
static cJSON *theme_motive_json(const char *id, const cJSON *motif) {
    cJSON *motive = ursatz_gui_theme_motive_json(id, NULL, motif, "melodic");
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

/* The id (M1, M2, ...) of the motive for a slot or an idea's motif version, adding the motive to motives when it is new.
   what names the slot or idea for the message. */
static UrsatzGuiThemeResult motive_for(const cJSON *body, MotiveSet *set, cJSON *motives, const char *what,
                                       const cJSON *reference, char id[MOTIVE_ID_BYTES], char *message,
                                       size_t message_size) {
    const char *item_id = ursatz_gui_json_string(reference, "item_id");
    int version = cJSON_GetObjectItemCaseSensitive(reference, "version")->valueint;
    for (int i = 0; i < set->count; i++) {
        if (set->version[i] == version && strcmp(set->item_id[i], item_id) == 0) {
            snprintf(id, MOTIVE_ID_BYTES, "M%d", i + 1);
            return URSATZ_GUI_THEME_OK;
        }
    }
    const cJSON *motif = used_motif(body, item_id, version);
    if (!motif || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(motif, "degrees"))) {
        snprintf(message, message_size, "the motif of the %s has no degrees", what);
        return URSATZ_GUI_THEME_ERR_INVALID;
    }
    if (set->count >= MOTIVE_MAX) {
        ursatz_gui_copy_message(message, message_size, "a theme can use at most 16 motifs");
        return URSATZ_GUI_THEME_ERR_INVALID;
    }
    snprintf(id, MOTIVE_ID_BYTES, "M%d", set->count + 1);
    cJSON *motive = theme_motive_json(id, motif);
    if (!motive) {
        return URSATZ_GUI_THEME_ERR_ALLOC;
    }
    cJSON_AddItemToArray(motives, motive);
    set->item_id[set->count] = item_id;
    set->version[set->count] = version;
    set->count++;
    return URSATZ_GUI_THEME_OK;
}

/*
 * The theme request of the Composer (docs/theme-request.md, version 2) for the current version of a theme, always in C.
 * style_path is the brief's style file, or NULL without a brief.
 */
static UrsatzGuiThemeResult build_request(const cJSON *body, const char *style_path, cJSON **out, char *message,
                                          size_t message_size) {
    const cJSON *theme = cJSON_GetObjectItemCaseSensitive(body, "theme");
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    cJSON *request = cJSON_CreateObject();
    cJSON *motives = cJSON_CreateArray();
    cJSON *slots = cJSON_CreateObject();
    cJSON *ideas = cJSON_CreateObject();
    cJSON *without = NULL;
    UrsatzGuiThemeResult result = URSATZ_GUI_THEME_OK;
    if (!request || !motives || !slots || !ideas) {
        result = URSATZ_GUI_THEME_ERR_ALLOC;
    }
    MotiveSet set = {.count = 0};
    char id[MOTIVE_ID_BYTES];
    const cJSON *slot = NULL;
    cJSON_ArrayForEach(slot, cJSON_GetObjectItemCaseSensitive(theme, "slots")) {
        if (result != URSATZ_GUI_THEME_OK) {
            break;
        }
        char what[32];
        snprintf(what, sizeof(what), "%s slot", ursatz_gui_json_string(slot, "slot"));
        result = motive_for(body, &set, motives, what, slot, id, message, message_size);
        if (result == URSATZ_GUI_THEME_OK && !cJSON_AddStringToObject(slots, ursatz_gui_json_string(slot, "slot"), id)) {
            result = URSATZ_GUI_THEME_ERR_ALLOC;
        }
    }
    const cJSON *idea = NULL;
    cJSON_ArrayForEach(idea, cJSON_GetObjectItemCaseSensitive(theme, "ideas")) {
        if (result != URSATZ_GUI_THEME_OK) {
            break;
        }
        char what[64];
        snprintf(what, sizeof(what), "%s idea", ursatz_gui_json_string(idea, "idea"));
        result = motive_for(body, &set, motives, what, idea, id, message, message_size);
        if (result == URSATZ_GUI_THEME_OK && !cJSON_AddStringToObject(ideas, ursatz_gui_json_string(idea, "idea"), id)) {
            result = URSATZ_GUI_THEME_ERR_ALLOC;
        }
    }

    bool ok = result == URSATZ_GUI_THEME_OK && cJSON_AddStringToObject(request, "schema", "ursatz-theme-request") &&
              cJSON_AddNumberToObject(request, "schema_version", 2) &&
              cJSON_AddStringToObject(request, "key", "C");
    static const char *const form_members[] = {"into", "hybrid", "mode", "meter", "bars", "cadence", "first_cadence"};
    for (size_t i = 0; ok && i < sizeof(form_members) / sizeof(form_members[0]); i++) {
        ok = ursatz_gui_json_add_copy_if(request, form, form_members[i]);
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
    if (ok) {
        ok = cJSON_AddItemToObject(request, "ideas", ideas);
        ideas = ok ? NULL : ideas;
    }
    if (ok) {
        const cJSON *chords = cJSON_GetObjectItemCaseSensitive(theme, "chords");
        cJSON *copy = cJSON_IsArray(chords) ? cJSON_Duplicate(chords, 1) : cJSON_CreateArray();
        ok = copy && cJSON_AddItemToObject(request, "chords", copy);
        if (!ok) {
            cJSON_Delete(copy);
        }
    }

    /* The accompaniment: nothing, a pattern, or a saved motive as the pattern. */
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(theme, "accompaniment");
    if (ok && (!accompaniment || cJSON_IsNull(accompaniment))) {
        ok = cJSON_AddNullToObject(request, "accompaniment") != NULL;
    } else if (ok && strcmp(ursatz_gui_json_string(accompaniment, "pattern"), "motive") != 0) {
        cJSON *pattern = cJSON_CreateObject();
        ok = pattern && cJSON_AddStringToObject(pattern, "pattern", ursatz_gui_json_string(accompaniment, "pattern")) &&
             cJSON_AddItemToObject(request, "accompaniment", pattern);
        if (!ok) {
            cJSON_Delete(pattern);
        }
    } else if (ok) {
        const cJSON *motif = used_motif(body, ursatz_gui_json_string(accompaniment, "item_id"),
                                        cJSON_GetObjectItemCaseSensitive(accompaniment, "version")->valueint);
        if (!motif || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(motif, "degrees"))) {
            ursatz_gui_copy_message(message, message_size, "the accompaniment motif has no degrees");
            result = URSATZ_GUI_THEME_ERR_INVALID;
            ok = false;
        } else {
            cJSON *pattern = cJSON_CreateObject();
            cJSON *motive = ursatz_gui_theme_motive_json(NULL, NULL, motif, "accompaniment");
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
        ok = cJSON_AddStringToObject(request, "style", style_path) && ursatz_gui_json_add_copy_if(request, theme, "style_weight");
        without = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(theme, "without"), 1);
        if (ok && without && cJSON_GetArraySize(without) > 0) {
            ok = cJSON_AddItemToObject(request, "without", without);
            without = ok ? NULL : without;
        }
    }
    ok = ok && ursatz_gui_json_add_copy_or_null(request, theme, "tempo");

    cJSON_Delete(without);
    cJSON_Delete(motives);
    cJSON_Delete(slots);
    cJSON_Delete(ideas);
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
UrsatzGuiThemeResult ursatz_gui_theme_build_brief(const cJSON *brief_json, const char *name, const char *path, cJSON **out_summary,
                                        char *message, size_t message_size) {
    UrsatzGuiStyleBrief brief;
    const char *failure = ursatz_gui_style_brief_parse(brief_json, &brief);
    if (failure) {
        ursatz_gui_copy_message(message, message_size, failure);
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
        ursatz_gui_copy_message(message, message_size, "a collection, subject, project or style of the brief was not found");
        result = URSATZ_GUI_THEME_ERR_UNPROCESSABLE;
    } else if (built == URSATZ_GUI_STYLE_ERR_NO_PIECES) {
        ursatz_gui_copy_message(message, message_size, "none of the brief's pieces could be used");
        result = URSATZ_GUI_THEME_ERR_UNPROCESSABLE;
    } else if (built == URSATZ_GUI_STYLE_ERR_INVALID_SOURCE) {
        ursatz_gui_copy_message(message, message_size, "invalid brief");
        result = URSATZ_GUI_THEME_ERR_INVALID;
    } else if (built != URSATZ_GUI_STYLE_OK) {
        ursatz_gui_copy_message(message, message_size, "the brief could not be built");
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

static const char *const kThemeFiles[] = {"plan.json", "theme.mid", "theme.musicxml"};

/* The theme request of the API: the request as the command read it, with the options the command added as members
   (cli/theme.c build_request). */
static char *api_request(const cJSON *request, bool check, bool use_seed, long long seed) {
    cJSON *copy = cJSON_Duplicate(request, 1);
    char seed_text[32];
    snprintf(seed_text, sizeof(seed_text), "%lld", seed);
    bool ok = copy && (!use_seed || cJSON_AddStringToObject(copy, "seed", seed_text)) &&
              cJSON_AddBoolToObject(copy, "check", check) && cJSON_AddBoolToObject(copy, "midi", 1) &&
              cJSON_AddBoolToObject(copy, "musicxml", 1) && cJSON_AddBoolToObject(copy, "plain_midi", 0) &&
              cJSON_AddBoolToObject(copy, "plans", 0);
    char *text = ok ? cJSON_PrintUnformatted(copy) : NULL;
    cJSON_Delete(copy);
    return text;
}

/* Grows the theme with the Composer API and writes plan.json, theme.mid and theme.musicxml into the stage. *out_summary
   is the document `theme --format json` printed: no plans and explanations, and the files written. */
static UrsatzGuiThemeResult run_theme(const UrsatzGuiComposeRun *run, cJSON **out_summary, char *message,
                                      size_t message_size) {
    char *text = api_request(run->request, cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(run->member, "check")),
                             run->use_seed, run->seed);
    if (!text) {
        return URSATZ_GUI_THEME_ERR_ALLOC;
    }
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    ComposerStatus status = composer_api_theme(text, run->job, &response);
    free(text);
    UrsatzGuiThemeResult result = URSATZ_GUI_THEME_OK;
    cJSON *document = NULL;
    if (status != COMPOSER_OK) {
        result = ursatz_gui_compose_failure(status, &response, message, message_size);
    } else if (!(document = cJSON_Parse(response.json)) || !response.plan_json || !response.midi || !response.musicxml) {
        ursatz_gui_copy_message(message, message_size, "composer output unreadable");
        result = URSATZ_GUI_THEME_ERR_FAILED;
    }
    for (size_t i = 0; result == URSATZ_GUI_THEME_OK && i < 3; i++) {
        char path[PATH_BYTES + 64];
        snprintf(path, sizeof(path), "%s/%s", run->stage, kThemeFiles[i]);
        const void *data = i == 0 ? (const void *)response.plan_json : i == 1 ? (const void *)response.midi : (const void *)response.musicxml;
        size_t size = i == 1 ? response.midi_size : strlen((const char *)data);
        if (!ursatz_gui_write_file_atomic(path, data, size)) {
            ursatz_gui_copy_message(message, message_size, "a file of the version could not be written");
            result = URSATZ_GUI_THEME_ERR_FAILED;
        }
    }
    if (result == URSATZ_GUI_THEME_OK) {
        cJSON *theme = NULL;
        cJSON_ArrayForEach(theme, cJSON_GetObjectItemCaseSensitive(document, "themes")) {
            cJSON_DeleteItemFromObjectCaseSensitive(theme, "plan");
            cJSON_DeleteItemFromObjectCaseSensitive(theme, "explanation");
        }
        if (!ursatz_gui_compose_add_files(document, run->named, kThemeFiles, 3)) {
            result = URSATZ_GUI_THEME_ERR_ALLOC;
        }
    }
    composer_response_destroy(&response);
    if (result != URSATZ_GUI_THEME_OK) {
        cJSON_Delete(document);
        return result;
    }
    *out_summary = document;
    return URSATZ_GUI_THEME_OK;
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

static UrsatzGuiThemeResult build_theme_request(const cJSON *body, const char *dir, const char *style_path,
                                               cJSON **out_request, char *message, size_t message_size) {
    (void)dir;
    return build_request(body, style_path, out_request, message, message_size);
}

static void theme_note(const cJSON *summary, char *note, size_t note_size) {
    (void)summary;
    snprintf(note, note_size, "Composed");
}

static const UrsatzGuiComposeKind kThemeKind = {"theme",            "the item is not a theme", composable,
                                                build_theme_request, run_theme,            theme_note,
                                                body_with_result};

UrsatzGuiThemeResult ursatz_gui_theme_validate(const cJSON *body, bool *complete, cJSON **out_request, char *message,
                                               size_t message_size) {
    *complete = false;
    *out_request = NULL;
    ursatz_gui_copy_message(message, message_size, "");
    const cJSON *theme = cJSON_GetObjectItemCaseSensitive(body, "theme");
    if (!cJSON_IsObject(theme)) {
        ursatz_gui_copy_message(message, message_size, "the body is not a theme");
        return URSATZ_GUI_THEME_OK;
    }
    const cJSON *brief = cJSON_GetObjectItemCaseSensitive(theme, "brief");
    bool has_brief = brief && !cJSON_IsNull(brief);
    if (!composable(theme, has_brief, message, message_size)) {
        return URSATZ_GUI_THEME_OK;
    }
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    if (has_brief && strcmp(ursatz_gui_json_string(form, "mode"), "style") == 0) {
        /* The style of the brief is built only when composing. */
        ursatz_gui_copy_message(message, message_size, "");
        return URSATZ_GUI_THEME_OK;
    }
    UrsatzGuiThemeResult result = build_request(body, NULL, out_request, message, message_size);
    *complete = result == URSATZ_GUI_THEME_OK;
    return result;
}

UrsatzGuiThemeResult ursatz_gui_theme_compose(const char *id, bool has_seed, long long seed, bool update_uses,
                                              const ComposerJob *job, cJSON **out_item, char *message,
                                              size_t message_size) {
    if (out_item) {
        *out_item = NULL;
    }
    ursatz_gui_copy_message(message, message_size, "");
    cJSON *item = NULL;
    UrsatzGuiThemeResult result = ursatz_gui_compose_item(&kThemeKind, id, has_seed, seed, update_uses, job, &item, message, message_size);
    if (result == URSATZ_GUI_THEME_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    return result;
}
