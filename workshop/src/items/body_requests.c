#include "body_requests.h"

#include <stdio.h>
#include <string.h>

#include <common/json.h>

#include "composition/composer_client.h"

#define MOTIVE_ID_BYTES 16

/* {"$item": {item_id, version[, file]}} for a reference {item_id, version}; NULL on allocation failure. */
static cJSON *item_reference(const cJSON *reference, const char *file) {
    cJSON *wrapper = cJSON_CreateObject();
    cJSON *inner = wrapper ? cJSON_AddObjectToObject(wrapper, "$item") : NULL;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(reference, "version");
    if (!inner || !cJSON_AddStringToObject(inner, "item_id", common_json_string(reference, "item_id")) ||
        !cJSON_AddNumberToObject(inner, "version", cJSON_IsNumber(version) ? version->valuedouble : 0) ||
        (file && !cJSON_AddStringToObject(inner, "file", file))) {
        cJSON_Delete(wrapper);
        return NULL;
    }
    return wrapper;
}

static bool is_reference(const cJSON *reference) {
    return cJSON_IsObject(reference) && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(reference, "item_id")) &&
           cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(reference, "version"));
}

/* ---- themes ---------------------------------------------------------------- */

/* The motives of a theme request, one for each distinct motif version, as {"id": "M<n>", "$item": ...}. */
typedef struct {
    cJSON *motives;
    int count;
} MotiveSet;

/* The id (M1, M2...) of the motive of a reference, adding the motive when it is new; NULL with *failed when out of memory. */
static const char *motive_id(MotiveSet *set, const cJSON *reference, char id[MOTIVE_ID_BYTES], bool *failed) {
    int index = 0;
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, set->motives) {
        const cJSON *known = cJSON_GetObjectItemCaseSensitive(entry, "$item");
        if (strcmp(common_json_string(known, "item_id"), common_json_string(reference, "item_id")) == 0 &&
            cJSON_GetObjectItemCaseSensitive(known, "version")->valuedouble == cJSON_GetObjectItemCaseSensitive(reference, "version")->valuedouble) {
            snprintf(id, MOTIVE_ID_BYTES, "M%d", index + 1);
            return id;
        }
        index++;
    }
    snprintf(id, MOTIVE_ID_BYTES, "M%d", set->count + 1);
    cJSON *wrapper = item_reference(reference, NULL);
    cJSON *motive = cJSON_CreateObject();
    if (!wrapper || !motive || !cJSON_AddStringToObject(motive, "id", id) || !cJSON_AddItemToObject(motive, "$item", cJSON_DetachItemFromObjectCaseSensitive(wrapper, "$item")) ||
        !common_json_append(set->motives, motive)) {
        cJSON_Delete(wrapper);
        *failed = true;
        return NULL;
    }
    cJSON_Delete(wrapper);
    set->count++;
    return id;
}

/* The theme request of a format 1 theme member, with its references in place. NULL on allocation failure. */
static cJSON *theme_request(const cJSON *theme) {
    static const char *const form_members[] = {"into", "hybrid", "material", "mode", "meter", "bars", "cadence", "first_cadence", "development", "construction"};
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    cJSON *request = cJSON_CreateObject();
    cJSON *slots = cJSON_CreateObject();
    cJSON *ideas = cJSON_CreateObject();
    MotiveSet set = {cJSON_CreateArray(), 0};
    bool ok = request && slots && ideas && set.motives && cJSON_AddStringToObject(request, "schema", "ursatz-theme-request") &&
              cJSON_AddNumberToObject(request, "schema_version", 2) && cJSON_AddStringToObject(request, "key", "C");
    for (size_t i = 0; ok && i < sizeof(form_members) / sizeof(form_members[0]); i++) {
        ok = common_json_add_copy_if(request, form, form_members[i]);
    }
    bool failed = !ok;
    char id[MOTIVE_ID_BYTES];
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(theme, "slots")) {
        if (failed) {
            break;
        }
        if (is_reference(entry) && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(entry, "slot"))) {
            const char *found = motive_id(&set, entry, id, &failed);
            failed = failed || !found || !cJSON_AddStringToObject(slots, common_json_string(entry, "slot"), found);
        }
    }
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(theme, "ideas")) {
        if (failed) {
            break;
        }
        if (is_reference(entry) && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(entry, "idea"))) {
            const char *found = motive_id(&set, entry, id, &failed);
            failed = failed || !found || !cJSON_AddStringToObject(ideas, common_json_string(entry, "idea"), found);
        }
    }
    const cJSON *slot_entry = NULL;
    cJSON_ArrayForEach(slot_entry, workshop_composer_vocabulary_list("theme_slots")) {
        const char *name = common_json_string(slot_entry, "value");
        if (!failed && !cJSON_GetObjectItemCaseSensitive(slots, name)) {
            failed = cJSON_AddNullToObject(slots, name) == NULL;
        }
    }
    ok = !failed && common_json_add(request, "motives", set.motives);
    set.motives = ok ? NULL : set.motives;
    ok = ok && common_json_add(request, "slots", slots);
    slots = ok ? NULL : slots;
    ok = ok && common_json_add(request, "ideas", ideas);
    ideas = ok ? NULL : ideas;
    if (ok) {
        const cJSON *chords = cJSON_GetObjectItemCaseSensitive(theme, "chords");
        cJSON *copy = cJSON_IsArray(chords) ? cJSON_Duplicate(chords, 1) : cJSON_CreateArray();
        ok = common_json_add(request, "chords", copy);
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(theme, "accompaniment");
    if (ok && !cJSON_IsObject(accompaniment)) {
        ok = cJSON_AddNullToObject(request, "accompaniment") != NULL;
    } else if (ok) {
        cJSON *pattern = cJSON_CreateObject();
        ok = pattern && cJSON_AddStringToObject(pattern, "pattern", common_json_string(accompaniment, "pattern"));
        if (ok && strcmp(common_json_string(accompaniment, "pattern"), "motive") == 0 && is_reference(accompaniment)) {
            ok = common_json_add(pattern, "motive", item_reference(accompaniment, NULL));
        }
        ok = ok && common_json_add(request, "accompaniment", pattern);
        if (!ok && pattern && !cJSON_GetObjectItemCaseSensitive(request, "accompaniment")) {
            cJSON_Delete(pattern);
        }
    }
    ok = ok && cJSON_AddNullToObject(request, "voicing") != NULL && common_json_add_copy_if(request, theme, "style_weight");
    if (ok) {
        const cJSON *without = cJSON_GetObjectItemCaseSensitive(theme, "without");
        ok = !cJSON_IsArray(without) || cJSON_GetArraySize(without) == 0 || common_json_add(request, "without", cJSON_Duplicate(without, 1));
    }
    ok = ok && common_json_add_copy_or_null(request, theme, "tempo");
    cJSON_Delete(set.motives);
    cJSON_Delete(slots);
    cJSON_Delete(ideas);
    if (!ok) {
        cJSON_Delete(request);
        return NULL;
    }
    return request;
}

/* ---- pieces ---------------------------------------------------------------- */

static const char *structure_form(const cJSON *structure) {
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(structure, "form");
    return cJSON_IsString(form) ? form->valuestring : "game-loop";
}

static bool structure_sized(const cJSON *structure) {
    const cJSON *form = workshop_composer_form(structure_form(structure));
    const char *sections = common_json_string(form, "sections");
    return form && sections && strcmp(sections, "request") != 0;
}

/* A section's fill as the request holds it: a motif's motive and pool by reference, a theme's plan by reference. */
static cJSON *fill_request(const cJSON *section, const char *pool, bool *ok) {
    const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
    const char *kind = cJSON_IsObject(fill) ? common_json_string(fill, "kind") : "empty";
    cJSON *object = cJSON_CreateObject();
    bool good = object && cJSON_AddStringToObject(object, "kind", kind);
    if (good && strcmp(kind, "motif") == 0) {
        cJSON *forms = cJSON_CreateObject();
        cJSON *inner = forms ? cJSON_AddObjectToObject(forms, "$pool") : NULL;
        good = inner && cJSON_AddStringToObject(inner, "item_id", common_json_string(fill, "item_id")) && cJSON_AddStringToObject(inner, "pool", pool);
        good = good && (is_reference(fill) ? common_json_add(object, "motive", item_reference(fill, NULL)) : cJSON_AddNullToObject(object, "motive") != NULL);
        good = good && common_json_add(object, "forms", forms);
        if (!good) {
            cJSON_Delete(forms);
        }
        good = good && common_json_add_copy_if(object, fill, "into") && common_json_add_copy_if(object, fill, "hybrid") &&
               common_json_add_copy_if(object, fill, "construction");
    } else if (good && strcmp(kind, "theme") == 0) {
        good = common_json_add(object, "plan", item_reference(fill, "plan.json"));
    } else if (good && strcmp(kind, "progression") == 0) {
        good = common_json_add_copy_or_null(object, fill, "numerals") && common_json_add_copy_if(object, fill, "cells");
    }
    if (!good) {
        cJSON_Delete(object);
        *ok = false;
        return NULL;
    }
    return object;
}

/* A section of a game loop as the request holds it (a progression fill of a body not migrated yet is the section's harmony). */
static cJSON *section_request(const cJSON *section, cJSON *fill) {
    cJSON *object = cJSON_CreateObject();
    const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(section, "pickup");
    bool ok = object && common_json_add_copy_or_null(object, section, "id") && common_json_add_copy_or_null(object, section, "part") &&
              common_json_add_copy_if(object, section, "letter") && common_json_add_copy_or_null(object, section, "beats") &&
              common_json_add(object, "pickup", cJSON_IsString(pickup) ? cJSON_Duplicate(pickup, 1) : cJSON_CreateString("0")) &&
              common_json_add_copy_or_null(object, section, "cadence") && common_json_add_copy_if(object, section, "function") &&
              common_json_add_copy_if(object, section, "region") && common_json_add_copy_if(object, section, "harmony");
    if (ok && fill && strcmp(common_json_string(fill, "kind"), "progression") == 0) {
        const cJSON *numerals = cJSON_GetObjectItemCaseSensitive(fill, "numerals");
        const cJSON *cells = cJSON_GetObjectItemCaseSensitive(fill, "cells");
        cJSON *harmony = cJSON_IsString(numerals) && cJSON_GetObjectItemCaseSensitive(object, "harmony") == NULL ? cJSON_CreateObject() : NULL;
        if (harmony && cJSON_AddStringToObject(harmony, "progression", numerals->valuestring) &&
            (!cJSON_IsNumber(cells) || cJSON_AddNumberToObject(harmony, "cells", cells->valuedouble)) &&
            cJSON_AddItemToObject(object, "harmony", harmony)) {
            harmony = NULL;
        }
        cJSON_Delete(harmony);
        cJSON *empty = cJSON_CreateObject();
        if (empty && cJSON_AddStringToObject(empty, "kind", "empty")) {
            cJSON_Delete(fill);
            fill = empty;
        } else {
            cJSON_Delete(empty);
        }
    }
    ok = common_json_add(object, "fill", fill) && ok;
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* An overlay of a sized form: its name, region and harmony, and a fill when it has one. */
static cJSON *overlay_request(const cJSON *overlay, cJSON *fill) {
    cJSON *object = cJSON_CreateObject();
    bool ok = object && common_json_add_copy_or_null(object, overlay, "name") && common_json_add_copy_if(object, overlay, "region") &&
              common_json_add_copy_if(object, overlay, "harmony");
    if (ok && fill && strcmp(common_json_string(fill, "kind"), "empty") != 0) {
        ok = common_json_add(object, "fill", fill);
    } else {
        cJSON_Delete(fill);
    }
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *piece_request(const cJSON *piece) {
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    const char *pool = cJSON_IsString(cJSON_GetObjectItemCaseSensitive(piece, "pool")) ? common_json_string(piece, "pool") : "approved-and-unflagged";
    bool sized = structure_sized(structure);
    cJSON *request = cJSON_CreateObject();
    cJSON *sections = cJSON_CreateArray();
    bool ok = request && sections;
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(structure, "sections")) {
        if (!ok) {
            break;
        }
        cJSON *fill = fill_request(section, pool, &ok);
        cJSON *entry = ok ? (sized ? overlay_request(section, fill) : section_request(section, fill)) : NULL;
        ok = ok && entry && common_json_append(sections, entry);
    }
    ok = ok && cJSON_AddStringToObject(request, "schema", "ursatz-piece-request") && cJSON_AddNumberToObject(request, "schema_version", 2) &&
         cJSON_AddStringToObject(request, "form", structure_form(structure)) && common_json_add_copy_if(request, structure, "key") &&
         common_json_add_copy_if(request, structure, "material") && common_json_add_copy_if(request, structure, "mode") &&
         common_json_add_copy_if(request, structure, "meter");
    const cJSON *motive = cJSON_GetObjectItemCaseSensitive(piece, "motive");
    if (ok && is_reference(motive)) {
        ok = common_json_add(request, "motive", item_reference(motive, NULL));
    } else if (ok) {
        ok = cJSON_AddNullToObject(request, "motive") != NULL;
    }
    if (ok && sized) {
        const cJSON *bars = cJSON_GetObjectItemCaseSensitive(structure, "bars");
        const cJSON *count = cJSON_GetObjectItemCaseSensitive(structure, "count");
        ok = cJSON_AddNumberToObject(request, "bars", cJSON_IsNumber(bars) ? bars->valuedouble : 0) &&
             cJSON_AddNumberToObject(request, "count", cJSON_IsNumber(count) ? count->valuedouble : 0) &&
             cJSON_AddBoolToObject(request, "modulate", cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(structure, "modulate")));
        if (ok && cJSON_GetArraySize(sections) > 0) {
            ok = common_json_add(request, "sections", sections);
            sections = NULL;
        }
    } else if (ok) {
        ok = common_json_add(request, "sections", sections);
        sections = NULL;
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(piece, "accompaniment");
    if (ok && !cJSON_IsObject(accompaniment)) {
        ok = cJSON_AddNullToObject(request, "accompaniment") != NULL;
    } else if (ok) {
        cJSON *pattern = cJSON_CreateObject();
        ok = pattern && cJSON_AddStringToObject(pattern, "pattern", common_json_string(accompaniment, "pattern")) && common_json_add(request, "accompaniment", pattern);
        if (!ok) {
            cJSON_Delete(pattern);
        }
    }
    ok = ok && (sized || cJSON_AddNullToObject(request, "voicing") != NULL) && common_json_add_copy_if(request, piece, "style_weight");
    if (ok) {
        const cJSON *without = cJSON_GetObjectItemCaseSensitive(piece, "without");
        ok = !cJSON_IsArray(without) || cJSON_GetArraySize(without) == 0 || common_json_add(request, "without", cJSON_Duplicate(without, 1));
    }
    ok = ok && common_json_add_copy_or_null(request, piece, "tempo");
    cJSON_Delete(sections);
    if (!ok) {
        cJSON_Delete(request);
        return NULL;
    }
    return request;
}

/* ---- the body ---------------------------------------------------------------- */

bool workshop_body_requests_migrate(cJSON *body) {
    const char *member = cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(body, "theme")) ? "theme"
                         : cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(body, "piece")) ? "piece"
                                                                                            : NULL;
    if (!member) {
        return true;
    }
    const cJSON *old = cJSON_GetObjectItemCaseSensitive(body, member);
    cJSON *request = strcmp(member, "theme") == 0 ? theme_request(old) : piece_request(old);
    if (!request) {
        return false;
    }
    /* what the old member held besides the request: the brief and the last compose's result, the options of the run */
    cJSON *brief = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(old, "brief"), 1);
    cJSON *result = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(old, "result"), 1);
    const cJSON *check = cJSON_GetObjectItemCaseSensitive(old, "check");
    const cJSON *seed = cJSON_GetObjectItemCaseSensitive(old, "seed");
    bool ok = cJSON_AddItemToObject(body, "request", request);
    request = ok ? NULL : request;
    ok = ok && (!brief || cJSON_AddItemToObject(body, "brief", brief));
    brief = ok ? NULL : brief;
    ok = ok && (!result || cJSON_AddItemToObject(body, "result", result));
    result = ok ? NULL : result;
    ok = ok && (!cJSON_IsBool(check) || cJSON_AddBoolToObject(body, "check", cJSON_IsTrue(check))) &&
         (!cJSON_IsNumber(seed) || cJSON_AddNumberToObject(body, "seed", seed->valuedouble));
    if (!ok) {
        cJSON_DeleteItemFromObjectCaseSensitive(body, "request");
        cJSON_DeleteItemFromObjectCaseSensitive(body, "brief");
        cJSON_DeleteItemFromObjectCaseSensitive(body, "result");
        cJSON_DeleteItemFromObjectCaseSensitive(body, "check");
        cJSON_DeleteItemFromObjectCaseSensitive(body, "seed");
        cJSON_Delete(request);
        cJSON_Delete(brief);
        cJSON_Delete(result);
        return false;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(body, member);
    return true;
}
