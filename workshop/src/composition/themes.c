#define _XOPEN_SOURCE 700

#include "themes.h"
#include "base/last_error.h"
#include "base/knowledge.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <common/files.h>
#include <common/strings.h>
#include <common/json.h>
#include <common/status.h>

#include "compose_common.h"
#include "composer_client.h"
#include "items/material_service.h"
#include "analysis/styles.h"

#define PATH_BYTES 800
#define POOL_MAX 64
#define CHAIN_MAX_STEPS 32
#define CHAIN_TEXT_MAX 400
#define MOTIVE_MAX 16
#define MOTIVE_ID_BYTES 16

/* --- updating sources ---------------------------------------------------- */

/* Moves every updated use of the theme to its source's current version, once for each source. */
UrsatzStatus workshop_theme_update_sources(const char *id, UrsatzError *error) {
    cJSON *status = NULL;
    UrsatzStatus result = workshop_material_uses_status(id, &status);
    if (result == URSATZ_ERR_NOT_FOUND) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    if (result != URSATZ_OK) {
        workshop_error_text(error, "the uses could not be read");
        return workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    UrsatzStatus outcome = URSATZ_OK;
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, status) {
        if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(use, "updated")) ||
            cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(use, "missing"))) {
            continue;
        }
        const char *source_id = common_json_string(use, "item_id");
        bool done = false;
        for (const cJSON *earlier = status; earlier && earlier != use; earlier = earlier->next) {
            done = done || strcmp(common_json_string(earlier, "item_id"), source_id) == 0;
        }
        if (done) {
            continue;
        }
        result = workshop_material_update_use(id, source_id, NULL);
        if (result != URSATZ_OK) {
            workshop_error_text(error, "a source could not be updated");
            outcome = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
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
        if (strcmp(common_json_string(use, "item_id"), item_id) == 0 && cJSON_IsNumber(number) && number->valueint == version) {
            return cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(use, "copy"), "motif");
        }
    }
    return NULL;
}

/* A motive of the request from a motif's members: with item (a pool form), else with id (a slot motive); the accompaniment
   motive has neither. */
cJSON *workshop_theme_motive_json(const char *id, const char *item, const cJSON *motif, const char *role) {
    cJSON *object = cJSON_CreateObject();
    bool ok = object && (item ? cJSON_AddStringToObject(object, "item", item) != NULL
                              : !id || cJSON_AddStringToObject(object, "id", id) != NULL);
    ok = ok && cJSON_AddStringToObject(object, "form", "full") && common_json_add_copy_or_null(object, motif, "degrees") &&
         common_json_add_copy_or_null(object, motif, "pickup") && common_json_add_copy_or_null(object, motif, "harmony") &&
         common_json_add_copy_or_null(object, motif, "features") && cJSON_AddStringToObject(object, "role", role);
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
        if (strcmp(common_json_string(entry, "id"), id) == 0) {
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
        const char *source = common_json_string(derived, "source_item");
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
UrsatzStatus workshop_theme_add_pool(const cJSON *list, const char *slot_item, const char *pool, cJSON *forms) {
    const char *const flags[2] = {"approved", "unflagged"};
    int passes = strcmp(pool, "none") == 0 ? 0 : strcmp(pool, "approved") == 0 ? 1 : 2;
    for (int pass = 0; pass < passes; pass++) {
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, list) {
            if (cJSON_GetArraySize(forms) >= POOL_MAX) {
                return URSATZ_OK;
            }
            const char *id = common_json_string(entry, "id");
            if (strcmp(id, slot_item) == 0 || !leads_to(list, id, slot_item)) {
                continue;
            }
            cJSON *meta = NULL;
            cJSON *body = NULL;
            if (workshop_material_item_get(id, &meta, &body) != URSATZ_OK) {
                cJSON_Delete(meta);
                cJSON_Delete(body);
                continue;
            }
            const cJSON *motif = cJSON_GetObjectItemCaseSensitive(body, "motif");
            const cJSON *derived = cJSON_GetObjectItemCaseSensitive(motif, "derived");
            const cJSON *degrees = cJSON_GetObjectItemCaseSensitive(motif, "degrees");
            UrsatzStatus result = URSATZ_OK;
            if (strcmp(common_json_string(derived, "flag"), flags[pass]) == 0 && strcmp(common_json_string(motif, "form"), "full") == 0 &&
                strcmp(common_json_string(motif, "role"), "melodic") == 0 && cJSON_IsString(degrees)) {
                cJSON *form = workshop_theme_motive_json(NULL, id, motif, "melodic");
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
                    result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
                } else {
                    cJSON_AddItemToArray(forms, form);
                }
            }
            cJSON_Delete(meta);
            cJSON_Delete(body);
            if (result != URSATZ_OK) {
                return result;
            }
        }
    }
    return URSATZ_OK;
}

/* The material of a theme form as text: its id (or style), or the old mode word of a body not migrated; NULL when it has none. */
static const char *form_material(const cJSON *form) {
    const cJSON *material = cJSON_GetObjectItemCaseSensitive(form, "material");
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(form, "mode");
    return cJSON_IsString(material) ? material->valuestring : cJSON_IsString(mode) ? mode->valuestring : NULL;
}

/* What the request needs from the theme, checked before anything is made. */
static bool composable(const cJSON *theme, bool has_brief, UrsatzError *error) {
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    if (!cJSON_IsObject(form) || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(form, "into")) ||
        !form_material(form) || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(form, "meter"))) {
        workshop_error_text(error, "the theme form needs into, material and meter");
        return false;
    }
    if (strcmp(common_json_string(form, "into"), "hybrid") == 0 && !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(form, "hybrid"))) {
        workshop_error_text(error, "a hybrid theme form needs hybrid");
        return false;
    }
    if (strcmp(form_material(form), "style") == 0 && !has_brief) {
        workshop_error_text(error, "the material style needs a brief");
        return false;
    }
    const cJSON *required = cJSON_GetArrayItem(workshop_composer_vocabulary_list("theme_slots"),
                                               workshop_composer_vocabulary_required_index("theme_slots"));
    const char *required_slot = common_json_string(required, "value");
    bool has_required = false;
    const cJSON *slot = NULL;
    cJSON_ArrayForEach(slot, cJSON_GetObjectItemCaseSensitive(theme, "slots")) {
        has_required = has_required || strcmp(common_json_string(slot, "slot"), required_slot) == 0;
    }
    if (!has_required) {
        char text[96];
        snprintf(text, sizeof(text), "the theme needs a %s slot", required_slot);
        workshop_error_text(error, text);
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

/* The id (M1, M2, ...) of the motive for a slot or an idea's motif version, adding the motive to motives when it is new.
   what names the slot or idea for the message. */
static UrsatzStatus motive_for(const cJSON *body, MotiveSet *set, cJSON *motives, const char *what,
                                       const cJSON *reference, char id[MOTIVE_ID_BYTES], UrsatzError *error) {
    const char *item_id = common_json_string(reference, "item_id");
    int version = cJSON_GetObjectItemCaseSensitive(reference, "version")->valueint;
    for (int i = 0; i < set->count; i++) {
        if (set->version[i] == version && strcmp(set->item_id[i], item_id) == 0) {
            snprintf(id, MOTIVE_ID_BYTES, "M%d", i + 1);
            return URSATZ_OK;
        }
    }
    const cJSON *motif = used_motif(body, item_id, version);
    if (!motif || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(motif, "degrees"))) {
        workshop_error_printf(error, "the motif of the %s has no degrees", what);
        return workshop_fail(URSATZ_ERR_REQUEST, NULL);
    }
    if (set->count >= MOTIVE_MAX) {
        workshop_error_text(error, "a theme can use at most 16 motifs");
        return workshop_fail(URSATZ_ERR_REQUEST, NULL);
    }
    snprintf(id, MOTIVE_ID_BYTES, "M%d", set->count + 1);
    cJSON *motive = theme_motive_json(id, motif);
    if (!motive) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    cJSON_AddItemToArray(motives, motive);
    set->item_id[set->count] = item_id;
    set->version[set->count] = version;
    set->count++;
    return URSATZ_OK;
}

/*
 * The theme request of the Composer (docs/theme-request.md, version 2) for the current version of a theme, always in C.
 * style_path is the brief's style file, or NULL without a brief.
 */
static UrsatzStatus build_request(const cJSON *body, const char *style_path, cJSON **out, UrsatzError *error) {
    const cJSON *theme = cJSON_GetObjectItemCaseSensitive(body, "theme");
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    cJSON *request = cJSON_CreateObject();
    cJSON *motives = cJSON_CreateArray();
    cJSON *slots = cJSON_CreateObject();
    cJSON *ideas = cJSON_CreateObject();
    cJSON *without = NULL;
    UrsatzStatus result = URSATZ_OK;
    if (!request || !motives || !slots || !ideas) {
        result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    MotiveSet set = {.count = 0};
    char id[MOTIVE_ID_BYTES];
    const cJSON *slot = NULL;
    cJSON_ArrayForEach(slot, cJSON_GetObjectItemCaseSensitive(theme, "slots")) {
        if (result != URSATZ_OK) {
            break;
        }
        char what[32];
        snprintf(what, sizeof(what), "%s slot", common_json_string(slot, "slot"));
        result = motive_for(body, &set, motives, what, slot, id, error);
        if (result == URSATZ_OK && !cJSON_AddStringToObject(slots, common_json_string(slot, "slot"), id)) {
            result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
    }
    const cJSON *idea = NULL;
    cJSON_ArrayForEach(idea, cJSON_GetObjectItemCaseSensitive(theme, "ideas")) {
        if (result != URSATZ_OK) {
            break;
        }
        char what[64];
        snprintf(what, sizeof(what), "%s idea", common_json_string(idea, "idea"));
        result = motive_for(body, &set, motives, what, idea, id, error);
        if (result == URSATZ_OK && !cJSON_AddStringToObject(ideas, common_json_string(idea, "idea"), id)) {
            result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
    }

    bool ok = result == URSATZ_OK && cJSON_AddStringToObject(request, "schema", "ursatz-theme-request") &&
              cJSON_AddNumberToObject(request, "schema_version", 2) &&
              cJSON_AddStringToObject(request, "key", "C");
    static const char *const form_members[] = {"into", "hybrid", "material", "mode", "meter", "bars", "cadence", "first_cadence", "development", "construction"};
    for (size_t i = 0; ok && i < sizeof(form_members) / sizeof(form_members[0]); i++) {
        ok = common_json_add_copy_if(request, form, form_members[i]);
    }
    const cJSON *slot_entry = NULL;
    cJSON_ArrayForEach(slot_entry, workshop_composer_vocabulary_list("theme_slots")) {
        const char *slot_name = common_json_string(slot_entry, "value");
        if (ok && !cJSON_GetObjectItemCaseSensitive(slots, slot_name)) {
            ok = cJSON_AddNullToObject(slots, slot_name) != NULL;
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
    } else if (ok && strcmp(common_json_string(accompaniment, "pattern"), "motive") != 0) {
        cJSON *pattern = cJSON_CreateObject();
        ok = pattern && cJSON_AddStringToObject(pattern, "pattern", common_json_string(accompaniment, "pattern")) &&
             cJSON_AddItemToObject(request, "accompaniment", pattern);
        if (!ok) {
            cJSON_Delete(pattern);
        }
    } else if (ok) {
        const cJSON *motif = used_motif(body, common_json_string(accompaniment, "item_id"),
                                        cJSON_GetObjectItemCaseSensitive(accompaniment, "version")->valueint);
        if (!motif || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(motif, "degrees"))) {
            workshop_error_text(error, "the accompaniment motif has no degrees");
            result = workshop_fail(URSATZ_ERR_REQUEST, NULL);
            ok = false;
        } else {
            cJSON *pattern = cJSON_CreateObject();
            cJSON *motive = workshop_theme_motive_json(NULL, NULL, motif, "accompaniment");
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
        ok = cJSON_AddStringToObject(request, "style", style_path) && common_json_add_copy_if(request, theme, "style_weight");
        without = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(theme, "without"), 1);
        if (ok && without && cJSON_GetArraySize(without) > 0) {
            ok = cJSON_AddItemToObject(request, "without", without);
            without = ok ? NULL : without;
        }
    }
    ok = ok && common_json_add_copy_or_null(request, theme, "tempo");

    cJSON_Delete(without);
    cJSON_Delete(motives);
    cJSON_Delete(slots);
    cJSON_Delete(ideas);
    if (!ok) {
        cJSON_Delete(request);
        return result == URSATZ_OK ? workshop_fail(URSATZ_ERR_ALLOC, NULL) : result;
    }
    *out = request;
    return URSATZ_OK;
}

/* --- the brief ----------------------------------------------------------- */

/*
 * Builds the theme's brief into path. *out_summary is {piece_count, skipped: [{id, reason}]}, caller-owned.
 */
UrsatzStatus workshop_theme_build_brief(const cJSON *brief_json, const char *name, const char *path, cJSON **out_summary,
                                        UrsatzError *error) {
    WorkshopStyleBrief brief;
    const char *failure = workshop_style_brief_parse(brief_json, &brief);
    if (failure) {
        workshop_error_text(error, failure);
        return strcmp(failure, "out of memory") == 0 ? workshop_fail(URSATZ_ERR_ALLOC, NULL) : workshop_fail(URSATZ_ERR_REQUEST, NULL);
    }
    WorkshopStyleSource source;
    memset(&source, 0, sizeof(source));
    source.kind = WORKSHOP_STYLE_SOURCE_BRIEF;
    source.brief = &brief;
    char **pieces = NULL;
    size_t piece_count = 0;
    WorkshopStyleSkipped *skipped = NULL;
    size_t skipped_count = 0;
    UrsatzStatus built =
        workshop_style_build_file(&source, name, path, &pieces, &piece_count, &skipped, &skipped_count);
    workshop_style_brief_free(&brief);
    UrsatzStatus result = URSATZ_OK;
    if (built == URSATZ_ERR_NOT_FOUND) {
        workshop_error_text(error, "a collection, subject, project or style of the brief was not found");
        result = workshop_fail(URSATZ_ERR_UNSATISFIABLE, NULL);
    } else if (built == URSATZ_ERR_UNSATISFIABLE) {
        workshop_error_text(error, "none of the brief's pieces could be used");
        result = workshop_fail(URSATZ_ERR_UNSATISFIABLE, NULL);
    } else if (built == URSATZ_ERR_REQUEST) {
        workshop_error_text(error, "invalid brief");
        result = workshop_fail(URSATZ_ERR_REQUEST, NULL);
    } else if (built != URSATZ_OK) {
        workshop_error_text(error, "the brief could not be built");
        result = built == URSATZ_ERR_ALLOC ? workshop_fail(URSATZ_ERR_ALLOC, NULL) : workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    cJSON *summary = result == URSATZ_OK ? cJSON_CreateObject() : NULL;
    cJSON *list = summary ? cJSON_AddArrayToObject(summary, "skipped") : NULL;
    if (result == URSATZ_OK &&
        (!list || !cJSON_AddNumberToObject(summary, "piece_count", (double)piece_count))) {
        result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    for (size_t i = 0; result == URSATZ_OK && i < skipped_count; i++) {
        cJSON *entry = cJSON_CreateObject();
        if (!entry || !cJSON_AddStringToObject(entry, "id", skipped[i].id) ||
            !cJSON_AddStringToObject(entry, "reason", skipped[i].reason)) {
            cJSON_Delete(entry);
            result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
        } else {
            cJSON_AddItemToArray(list, entry);
        }
    }
    for (size_t i = 0; built == URSATZ_OK && i < piece_count; i++) {
        free(pieces[i]);
    }
    free(pieces);
    workshop_style_skipped_free(skipped, skipped_count);
    if (result != URSATZ_OK) {
        cJSON_Delete(summary);
        return result;
    }
    *out_summary = summary;
    return URSATZ_OK;
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
static UrsatzStatus run_theme(const WorkshopComposeRun *run, cJSON **out_summary, UrsatzError *error) {
    char *text = api_request(run->request, cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(run->member, "check")),
                             run->use_seed, run->seed);
    if (!text) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    UrsatzStatus status = composer_api_theme(workshop_composer_context_global(), text, run->job, &response);
    free(text);
    UrsatzStatus result = URSATZ_OK;
    cJSON *document = NULL;
    if (status != URSATZ_OK) {
        result = workshop_compose_failure(status, &response, error);
    } else if (!(document = cJSON_Parse(response.json)) || !response.plan_json || !response.midi || !response.musicxml) {
        workshop_error_text(error, "composer output unreadable");
        result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    for (size_t i = 0; result == URSATZ_OK && i < 3; i++) {
        char path[PATH_BYTES + 64];
        snprintf(path, sizeof(path), "%s/%s", run->stage, kThemeFiles[i]);
        const void *data = i == 0 ? (const void *)response.plan_json : i == 1 ? (const void *)response.midi : (const void *)response.musicxml;
        size_t size = i == 1 ? response.midi_size : strlen((const char *)data);
        if (!(common_file_write_atomic(path, data, size, NULL) == URSATZ_OK)) {
            workshop_error_text(error, "a file of the version could not be written");
            result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
        }
    }
    if (result == URSATZ_OK) {
        cJSON *theme = NULL;
        cJSON_ArrayForEach(theme, cJSON_GetObjectItemCaseSensitive(document, "themes")) {
            cJSON_DeleteItemFromObjectCaseSensitive(theme, "plan");
            cJSON_DeleteItemFromObjectCaseSensitive(theme, "explanation");
        }
        if (!workshop_compose_add_files(document, run->named, kThemeFiles, 3)) {
            result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
    }
    composer_response_destroy(&response);
    if (result != URSATZ_OK) {
        cJSON_Delete(document);
        return result;
    }
    *out_summary = document;
    return URSATZ_OK;
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

static UrsatzStatus build_theme_request(const cJSON *body, const char *dir, const char *style_path,
                                               cJSON **out_request, UrsatzError *error) {
    (void)dir;
    return build_request(body, style_path, out_request, error);
}

static void theme_note(const cJSON *summary, char *note, size_t note_size) {
    (void)summary;
    snprintf(note, note_size, "Composed");
}

static const WorkshopComposeKind kThemeKind = {"theme",            "the item is not a theme", composable,
                                                build_theme_request, run_theme,            theme_note,
                                                body_with_result};

UrsatzStatus workshop_theme_validate(const cJSON *body, bool *complete, cJSON **out_request, UrsatzError *error) {
    *complete = false;
    *out_request = NULL;
    workshop_error_text(error, "");
    const cJSON *theme = cJSON_GetObjectItemCaseSensitive(body, "theme");
    if (!cJSON_IsObject(theme)) {
        workshop_error_text(error, "the body is not a theme");
        return URSATZ_OK;
    }
    const cJSON *brief = cJSON_GetObjectItemCaseSensitive(theme, "brief");
    bool has_brief = brief && !cJSON_IsNull(brief);
    if (!composable(theme, has_brief, error)) {
        return URSATZ_OK;
    }
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    if (has_brief && strcmp(form_material(form), "style") == 0) {
        /* The style of the brief is built only when composing. */
        workshop_error_text(error, "");
        return URSATZ_OK;
    }
    UrsatzStatus result = build_request(body, NULL, out_request, error);
    *complete = result == URSATZ_OK;
    return result;
}

UrsatzStatus workshop_theme_compose(const char *id, bool has_seed, long long seed, bool update_uses,
                                              const UrsatzJob *job, cJSON **out_item, UrsatzError *error) {
    if (out_item) {
        *out_item = NULL;
    }
    workshop_error_text(error, "");
    cJSON *item = NULL;
    UrsatzStatus result = workshop_compose_item(&kThemeKind, id, has_seed, seed, update_uses, job, &item, error);
    if (result == URSATZ_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    return result;
}
