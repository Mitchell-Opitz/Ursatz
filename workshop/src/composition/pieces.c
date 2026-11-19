#define _XOPEN_SOURCE 700

#include "pieces.h"
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
#include "items/store.h"

#define PATH_BYTES 800

/* Copies the file whole, atomically (common_file_write_atomic). */
static bool copy_file(const char *from, const char *to) {
    char *data = NULL;
    size_t size = 0;
    bool ok = common_file_read(from, &data, &size, NULL) == URSATZ_OK &&
              common_file_write_atomic(to, data, size, NULL) == URSATZ_OK;
    free(data);
    return ok;
}

/* --- the request --------------------------------------------------------- */

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

/* The motive of a reference {item_id, version}, or NULL when its copy has no degrees. */
static cJSON *motive_of(const cJSON *body, const cJSON *reference, UrsatzError *error, const char *what) {
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(reference, "version");
    const cJSON *motif = used_motif(body, common_json_string(reference, "item_id"), cJSON_IsNumber(version) ? version->valueint : 0);
    if (!motif || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(motif, "degrees"))) {
        workshop_error_printf(error, "the motif of %s has no degrees", what);
        return NULL;
    }
    return workshop_theme_motive_json(NULL, NULL, motif, "melodic");
}

/* A section's fill as the request holds it. The plan of a theme fill is copied into dir (NULL: not copied, the request
   names the source plan). */
static UrsatzStatus build_fill(const cJSON *body, const cJSON *derived_list, const char *pool, const cJSON *section,
                                       const char *dir, cJSON **out, UrsatzError *error) {
    const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
    /* A section of a game loop has an id, an overlay of a sized form a name (R-610). */
    const char *id = common_json_string(section, "id");
    id = id[0] != '\0' ? id : common_json_string(section, "name");
    const char *kind = cJSON_IsObject(fill) ? common_json_string(fill, "kind") : "empty";
    cJSON *object = cJSON_CreateObject();
    bool ok = object && cJSON_AddStringToObject(object, "kind", kind);
    UrsatzStatus result = URSATZ_OK;
    if (ok && strcmp(kind, "motif") == 0) {
        char what[64];
        snprintf(what, sizeof(what), "section %s", id);
        cJSON *motive = motive_of(body, fill, error, what);
        cJSON *forms = cJSON_CreateArray();
        if (!motive) {
            cJSON_Delete(forms);
            cJSON_Delete(object);
            return workshop_fail(URSATZ_ERR_REQUEST, NULL);
        }
        ok = common_json_add(object, "motive", motive);
        ok = common_json_add(object, "forms", forms) && ok;
        if (ok) {
            result = workshop_theme_add_pool(derived_list, common_json_string(fill, "item_id"), pool, forms);
        }
        ok = ok && common_json_add_copy_if(object, fill, "into") && common_json_add_copy_if(object, fill, "hybrid") &&
             common_json_add_copy_if(object, fill, "construction");
    } else if (ok && strcmp(kind, "theme") == 0) {
        char source[PATH_BYTES];
        char plan[PATH_BYTES + 16];
        char copy[PATH_BYTES + 64];
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(fill, "version");
        if (!workshop_material_version_dir(common_json_string(fill, "item_id"), cJSON_IsNumber(version) ? version->valueint : 0, source,
                                             sizeof(source))) {
            workshop_error_text(error, "a theme fill names no version");
            result = workshop_fail(URSATZ_ERR_REQUEST, NULL);
        } else {
            snprintf(plan, sizeof(plan), "%s/plan.json", source);
            snprintf(copy, sizeof(copy), "%s/theme-%s.plan.json", dir ? dir : "", id);
            if (!dir) {
                /* A check writes nothing: the request names the version's own plan. */
                snprintf(copy, sizeof(copy), "%s", plan);
                ok = cJSON_AddStringToObject(object, "plan", copy) != NULL;
            } else if (!copy_file(plan, copy)) {
                workshop_error_printf(error, "the plan of the theme in section %s could not be copied", id);
                result = workshop_fail(URSATZ_ERR_REQUEST, NULL);
            } else {
                ok = cJSON_AddStringToObject(object, "plan", copy) != NULL;
            }
        }
    } else if (ok && strcmp(kind, "progression") == 0) {
        ok = common_json_add_copy_or_null(object, fill, "numerals") && common_json_add_copy_if(object, fill, "cells");
    }
    if (!ok || result != URSATZ_OK) {
        cJSON_Delete(object);
        return result == URSATZ_OK ? workshop_fail(URSATZ_ERR_ALLOC, NULL) : result;
    }
    *out = object;
    return URSATZ_OK;
}

static cJSON *section_json(const cJSON *section, cJSON *fill) {
    cJSON *object = cJSON_CreateObject();
    const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(section, "pickup");
    bool ok = object && common_json_add_copy_or_null(object, section, "id") && common_json_add_copy_or_null(object, section, "part") &&
              common_json_add_copy_if(object, section, "letter") && common_json_add_copy_or_null(object, section, "beats") &&
              common_json_add(object, "pickup", cJSON_IsString(pickup) ? cJSON_Duplicate(pickup, 1) : cJSON_CreateString("0")) &&
              common_json_add_copy_or_null(object, section, "cadence") && common_json_add_copy_if(object, section, "function") &&
              common_json_add_copy_if(object, section, "region") && common_json_add_copy_if(object, section, "harmony");
    /* A progression fill of a body not migrated yet is the section's harmony (R-610). */
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

/* An overlay of a sized form as the request holds it: its name, its region and harmony, and a fill when it has one (R-610). */
static cJSON *overlay_json(const cJSON *overlay, cJSON *fill) {
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

/* The name of the form of a piece structure: structure.form, or game-loop for a piece saved before the forms. */
static const char *structure_form(const cJSON *structure) {
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(structure, "form");
    return cJSON_IsString(form) ? form->valuestring : "game-loop";
}

/* True when the form of the structure has sizes in place of the request's own sections. */
static bool structure_sized(const cJSON *structure) {
    const cJSON *form = workshop_composer_form(structure_form(structure));
    const char *sections = common_json_string(form, "sections");
    return form && sections && strcmp(sections, "request") != 0;
}

/* The material of a structure as text: its id (or style), or the old mode word of a body not migrated; NULL when it has none. */
static const char *structure_material(const cJSON *structure) {
    const cJSON *material = cJSON_GetObjectItemCaseSensitive(structure, "material");
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(structure, "mode");
    return cJSON_IsString(material) ? material->valuestring : cJSON_IsString(mode) ? mode->valuestring : NULL;
}

/* What the request needs from the piece, checked before anything is made. A form with sizes asks only key, material and
 * meter. */
static bool composable(const cJSON *piece, bool has_brief, UrsatzError *error) {
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    bool sized = cJSON_IsObject(structure) && structure_sized(structure);
    if (!cJSON_IsObject(structure) || (!sized && !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(structure, "sections"))) ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(structure, "key")) ||
        !structure_material(structure) || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(structure, "meter"))) {
        workshop_error_text(error, sized ? "the piece structure needs key, material and meter"
                                                             : "the piece structure needs key, material, meter and sections");
        return false;
    }
    if (strcmp(structure_material(structure), "style") == 0 && !has_brief) {
        workshop_error_text(error, "the material style needs a brief");
        return false;
    }
    const cJSON *section = NULL;
    const cJSON *own_sections = sized ? NULL : cJSON_GetObjectItemCaseSensitive(structure, "sections");
    cJSON_ArrayForEach(section, own_sections) {
        const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        if (cJSON_IsObject(fill) && strcmp(common_json_string(fill, "kind"), "motif") == 0 &&
            strcmp(common_json_string(fill, "into"), "hybrid") == 0 && !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(fill, "hybrid"))) {
            workshop_error_printf(error, "the hybrid motif fill of section %s needs hybrid", common_json_string(section, "id"));
            return false;
        }
    }
    return true;
}

/*
 * The piece request of the Composer (docs/piece-request.md) for the current version of a piece. dir is the new version's
 * folder (absolute), where theme plans are copied; NULL writes nothing (validate). style_path is the brief's style file, or NULL without a brief.
 */
static UrsatzStatus build_request(const cJSON *body, const char *dir, const char *style_path, cJSON **out,
                                          UrsatzError *error) {
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(body, "piece");
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    const char *pool = cJSON_IsString(cJSON_GetObjectItemCaseSensitive(piece, "pool")) ? common_json_string(piece, "pool")
                                                                                       : "approved-and-unflagged";
    bool sized = structure_sized(structure);
    cJSON *request = cJSON_CreateObject();
    cJSON *sections = cJSON_CreateArray();
    cJSON *derived_list = NULL;
    UrsatzStatus result = URSATZ_OK;
    if (!request || !sections) {
        result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    if (result == URSATZ_OK && workshop_material_derived_motifs(&derived_list) != URSATZ_OK) {
        workshop_error_text(error, "the saved forms could not be read");
        result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    const cJSON *section = NULL;
    /* A game loop's sections, or a sized form's overlays: the sections of its size that were given something (R-610). */
    const cJSON *own_sections = cJSON_GetObjectItemCaseSensitive(structure, "sections");
    cJSON_ArrayForEach(section, own_sections) {
        if (result != URSATZ_OK) {
            break;
        }
        cJSON *fill = NULL;
        result = build_fill(body, derived_list, pool, section, dir, &fill, error);
        cJSON *entry = result == URSATZ_OK ? (sized ? overlay_json(section, fill) : section_json(section, fill)) : NULL;
        if (result == URSATZ_OK && (!entry || !cJSON_AddItemToArray(sections, entry))) {
            cJSON_Delete(entry);
            result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
    }

    bool ok = result == URSATZ_OK && cJSON_AddStringToObject(request, "schema", "ursatz-piece-request") &&
              cJSON_AddNumberToObject(request, "schema_version", 2) &&
              cJSON_AddStringToObject(request, "form", structure_form(structure)) && common_json_add_copy_if(request, structure, "key") &&
              common_json_add_copy_if(request, structure, "material") && common_json_add_copy_if(request, structure, "mode") &&
              common_json_add_copy_if(request, structure, "meter");
    const cJSON *motive_ref = cJSON_GetObjectItemCaseSensitive(piece, "motive");
    if (ok && cJSON_IsObject(motive_ref)) {
        cJSON *motive = motive_of(body, motive_ref, error, "the piece motive");
        if (!motive) {
            result = workshop_fail(URSATZ_ERR_REQUEST, NULL);
            ok = false;
        } else {
            ok = common_json_add(request, "motive", motive);
        }
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

    /* The accompaniment: nothing or a pattern; the pattern motive is not available in a piece. */
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(piece, "accompaniment");
    if (ok && (!accompaniment || cJSON_IsNull(accompaniment))) {
        ok = cJSON_AddNullToObject(request, "accompaniment") != NULL;
    } else if (ok) {
        cJSON *pattern = cJSON_CreateObject();
        ok = pattern && cJSON_AddStringToObject(pattern, "pattern", common_json_string(accompaniment, "pattern"));
        if (ok) {
            ok = common_json_add(request, "accompaniment", pattern);
        } else {
            cJSON_Delete(pattern);
        }
    }
    ok = ok && (sized || cJSON_AddNullToObject(request, "voicing") != NULL);
    if (ok && style_path) {
        cJSON *without = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(piece, "without"), 1);
        ok = cJSON_AddStringToObject(request, "style", style_path) && common_json_add_copy_if(request, piece, "style_weight");
        if (ok && without && cJSON_GetArraySize(without) > 0) {
            ok = common_json_add(request, "without", without);
        } else {
            cJSON_Delete(without);
        }
    }
    ok = ok && common_json_add_copy_or_null(request, piece, "tempo");

    cJSON_Delete(derived_list);
    cJSON_Delete(sections);
    if (!ok) {
        cJSON_Delete(request);
        return result == URSATZ_OK ? workshop_fail(URSATZ_ERR_ALLOC, NULL) : result;
    }
    *out = request;
    return URSATZ_OK;
}

/* --- composing ----------------------------------------------------------- */

/* The files every version has besides those of the versions of its form (<name>.mid and the rest, R-430). */
static const char *const kFileNames[] = {"request.json", "brief.style.json"};
#define FILE_NAME_COUNT 2
#define VERSIONS_MAX 4

static cJSON *read_json(const char *path);
static bool set_string(cJSON *object, const char *name, const char *text);

/* A copy of the request as the command read it: the plan file of each theme fill read into the request (the API takes
   plans, not paths; cli/piece.c read_request_file) and its path kept as the source. */
static UrsatzStatus request_with_plans(const cJSON *request, cJSON **out, UrsatzError *error) {
    cJSON *copy = cJSON_Duplicate(request, 1);
    if (!copy) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(copy, "sections")) {
        cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        const cJSON *plan_path = cJSON_GetObjectItemCaseSensitive(fill, "plan");
        if (!cJSON_IsObject(fill) || strcmp(common_json_string(fill, "kind"), "theme") != 0 || !cJSON_IsString(plan_path)) {
            continue;
        }
        cJSON *plan = read_json(plan_path->valuestring);
        char *source = strdup(plan_path->valuestring);
        if (!plan || !source || !cJSON_ReplaceItemInObject(fill, "plan", plan) || !cJSON_AddStringToObject(fill, "source", source)) {
            workshop_error_printf(error, "section %s: cannot read the theme plan '%s'", common_json_string(section, "id"),
                     plan_path->valuestring);
            free(source);
            cJSON_Delete(copy);
            return plan ? workshop_fail(URSATZ_ERR_ALLOC, NULL) : workshop_fail(URSATZ_ERR_REQUEST, NULL);
        }
        free(source);
    }
    *out = copy;
    return URSATZ_OK;
}

static double document_tempo(const cJSON *document) {
    const cJSON *qpm = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "brief"), "tempo"), "qpm");
    return cJSON_IsNumber(qpm) ? qpm->valuedouble : 0;
}


static bool write_bytes(const char *dir, const char *name, const void *data, size_t size, UrsatzError *error) {
    char path[PATH_BYTES + 128];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (!(common_file_write_atomic(path, data, size, NULL) == URSATZ_OK)) {
        workshop_error_text(error, "a file of the version could not be written");
        return false;
    }
    return true;
}

/*
 * Runs the piece API on request (the request file's members, as the command read it) and writes the eight files of
 * the version into stage. With from (a loop plan object, not owned) it writes section regenerate of that plan again
 * ("none": only the ending version), in lane when that is not NULL. *out_summary is the document `piece --format json`
 * printed: the plans of the versions are their files, and the files are listed under the folder named.
 */
static UrsatzStatus run_piece(const char *stage, const char *named, const cJSON *request, bool check, bool use_seed,
                                      long long seed, const cJSON *from, const char *regenerate, const char *lane,
                                      const UrsatzJob *job, cJSON **out_summary, UrsatzError *error) {
    cJSON *with_plans = NULL;
    UrsatzStatus result = request_with_plans(request, &with_plans, error);
    if (result != URSATZ_OK) {
        return result;
    }
    cJSON *api = cJSON_CreateObject();
    char seed_text[32];
    snprintf(seed_text, sizeof(seed_text), "%lld", seed);
    bool ok = api && cJSON_AddItemToObject(api, "request", with_plans);
    if (!ok) {
        cJSON_Delete(with_plans);
    }
    if (ok && from) {
        cJSON *edit = cJSON_CreateObject();
        cJSON *lanes = cJSON_CreateArray();
        bool whole = !lane || strcmp(lane, "all") == 0;
        ok = edit && lanes && cJSON_AddItemToObject(api, "from", cJSON_Duplicate(from, 1)) &&
             (whole || cJSON_AddItemToArray(lanes, cJSON_CreateString(lane))) && cJSON_AddItemToObject(edit, "lanes", lanes);
        lanes = ok ? NULL : lanes;
        ok = ok && (strcmp(regenerate, "none") == 0 ? cJSON_AddNullToObject(edit, "section") != NULL
                                                    : cJSON_AddStringToObject(edit, "section", regenerate) != NULL) &&
             cJSON_AddItemToObject(api, "regenerate", edit);
        if (!ok) {
            cJSON_Delete(edit);
        }
        cJSON_Delete(lanes);
    }
    ok = ok && (!use_seed || cJSON_AddStringToObject(api, "seed", seed_text)) && cJSON_AddBoolToObject(api, "check", check) &&
         cJSON_AddBoolToObject(api, "plain_midi", 0) && cJSON_AddBoolToObject(api, "midi", 1) &&
         cJSON_AddBoolToObject(api, "musicxml", 1) && cJSON_AddBoolToObject(api, "musicxml_versions", 1);
    char *text = ok ? cJSON_PrintUnformatted(api) : NULL;
    cJSON_Delete(api);
    if (!text) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    UrsatzStatus status = composer_api_piece(workshop_composer_context_global(), text, job, &response);
    free(text);
    cJSON *document = NULL;
    ComposerResponse extra;
    memset(&extra, 0, sizeof(extra));
    if (status != URSATZ_OK) {
        result = workshop_compose_failure(status, &response, error);
    } else if (!(document = cJSON_Parse(response.json)) || !response.midi || !response.musicxml) {
        workshop_error_text(error, "composer output unreadable");
        result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    const cJSON *plans = cJSON_GetObjectItemCaseSensitive(document, "plans");
    const cJSON *versions = cJSON_GetObjectItemCaseSensitive(document, "versions");
    const cJSON *xmls = cJSON_GetObjectItemCaseSensitive(document, "musicxml_versions");
    int version_count = cJSON_GetArraySize(versions);
    if (result == URSATZ_OK && (!cJSON_IsObject(plans) || !cJSON_IsObject(versions) || !cJSON_IsObject(xmls) ||
                                          version_count < 1 || version_count > VERSIONS_MAX)) {
        workshop_error_text(error, "composer output unreadable");
        result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    /* One plan, MIDI and MusicXML file for each version of the form, in the order the Composer lists them: the first
       is the response itself, a later one is exported from its plan and its MusicXML is in musicxml_versions. */
    char path[PATH_BYTES + 128];
    char names[VERSIONS_MAX][96];
    int written = 0;
    const cJSON *version = NULL;
    cJSON_ArrayForEach(version, versions) {
        if (result != URSATZ_OK) {
            break;
        }
        const char *name = version->string;
        const cJSON *plan = cJSON_GetObjectItemCaseSensitive(plans, name);
        const cJSON *xml = written == 0 ? NULL : cJSON_GetObjectItemCaseSensitive(xmls, name);
        if (!workshop_material_version_name_ok(name) || !plan || (written > 0 && !cJSON_IsString(xml))) {
            workshop_error_text(error, "composer output unreadable");
            result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
            break;
        }
        char file[96];
        snprintf(file, sizeof(file), written == 0 ? "plan.json" : "%s-plan.json", name);
        snprintf(path, sizeof(path), "%s/%s", stage, file);
        result = workshop_compose_write_plan(plan, path, error);
        snprintf(names[written], sizeof(names[written]), "%s", file);
        if (result == URSATZ_OK) {
            composer_response_destroy(&extra);
            memset(&extra, 0, sizeof(extra));
            const uint8_t *midi = response.midi;
            size_t midi_size = response.midi_size;
            if (written > 0) {
                result = workshop_compose_export(plan, document_tempo(document), false, true, false, NULL, job, &extra, error);
                midi = extra.midi;
                midi_size = extra.midi_size;
            }
            snprintf(file, sizeof(file), "%s.mid", name);
            bool wrote = result == URSATZ_OK && write_bytes(stage, file, midi, midi_size, error);
            const char *text = written == 0 ? response.musicxml : xml->valuestring;
            snprintf(file, sizeof(file), "%s.musicxml", name);
            wrote = wrote && write_bytes(stage, file, text, strlen(text), error);
            if (result == URSATZ_OK && !wrote) {
                result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
            }
        }
        written++;
    }
    if (result == URSATZ_OK) {
        /* Each version names its plan file; the summary lists every file of the versions. */
        const cJSON *entry = NULL;
        int index = 0;
        cJSON_ArrayForEach(entry, versions) {
            snprintf(path, sizeof(path), "%s/%s", named, names[index++]);
            set_string((cJSON *)entry, "plan", path);
        }
        cJSON_DeleteItemFromObjectCaseSensitive(document, "plans");
        cJSON_DeleteItemFromObjectCaseSensitive(document, "explanation");
        cJSON_DeleteItemFromObjectCaseSensitive(document, "musicxml_versions");
        cJSON *files = cJSON_AddArrayToObject(document, "files");
        index = 0;
        cJSON_ArrayForEach(entry, versions) {
            char file[PATH_BYTES + 128];
            snprintf(file, sizeof(file), "%s/%s", named, names[index++]);
            bool added = files && cJSON_AddItemToArray(files, cJSON_CreateString(file));
            snprintf(file, sizeof(file), "%s/%s.mid", named, entry->string);
            added = added && cJSON_AddItemToArray(files, cJSON_CreateString(file));
            snprintf(file, sizeof(file), "%s/%s.musicxml", named, entry->string);
            added = added && cJSON_AddItemToArray(files, cJSON_CreateString(file));
            if (!added) {
                result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
                break;
            }
        }
    }
    composer_response_destroy(&extra);
    composer_response_destroy(&response);
    if (result != URSATZ_OK) {
        cJSON_Delete(document);
        return result;
    }
    *out_summary = document;
    return URSATZ_OK;
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
              common_json_add(result, "seed", seed ? cJSON_CreateNumber(seed->valuedouble) : cJSON_CreateNull()) &&
              common_json_add(result, "loop", loop ? cJSON_Duplicate(loop, 1) : cJSON_CreateNull());
    for (int i = 0; ok && i < FILE_NAME_COUNT; i++) {
        if (i != 1 || has_brief) {
            ok = cJSON_AddItemToArray(files, cJSON_CreateString(kFileNames[i]));
        }
    }
    /* The files of the versions, as run_piece listed them under the folder named. */
    const cJSON *path = NULL;
    cJSON_ArrayForEach(path, cJSON_GetObjectItemCaseSensitive(summary, "files")) {
        const char *slash = cJSON_IsString(path) ? strrchr(path->valuestring, '/') : NULL;
        if (ok && cJSON_IsString(path)) {
            ok = cJSON_AddItemToArray(files, cJSON_CreateString(slash ? slash + 1 : path->valuestring));
        }
    }
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(piece, "structure"), "sections")) {
        const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        if (ok && cJSON_IsObject(fill) && strcmp(common_json_string(fill, "kind"), "theme") == 0) {
            char name[64];
            snprintf(name, sizeof(name), "theme-%s.plan.json", common_json_string(section, "id"));
            ok = cJSON_AddItemToArray(files, cJSON_CreateString(name));
        }
    }
    if (ok) {
        ok = common_json_add(result, "summary", summary);
        summary = NULL;
    }
    if (ok) {
        ok = common_json_add(result, "brief", brief_summary);
        brief_summary = NULL;
    }
    if (ok) {
        ok = common_json_add(result, "files", files);
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

static UrsatzStatus run_piece_item(const WorkshopComposeRun *run, cJSON **out_summary, UrsatzError *error) {
    return run_piece(run->stage, run->named, run->request, cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(run->member, "check")),
                     run->use_seed, run->seed, NULL, NULL, NULL, run->job, out_summary, error);
}

static void piece_note(const cJSON *summary, char *note, size_t note_size) {
    const cJSON *reported = reported_seed(summary);
    if (reported) {
        snprintf(note, note_size, "composed seed %.0f", reported->valuedouble);
    } else {
        snprintf(note, note_size, "composed");
    }
}

static const WorkshopComposeKind kPieceKind = {"piece",       "the item is not a piece", composable,
                                                build_request, run_piece_item,            piece_note,
                                                body_with_result};

UrsatzStatus workshop_piece_validate(const cJSON *body, bool *complete, cJSON **out_request, UrsatzError *error) {
    *complete = false;
    *out_request = NULL;
    workshop_error_text(error, "");
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(body, "piece");
    if (!cJSON_IsObject(piece)) {
        workshop_error_text(error, "the body is not a piece");
        return URSATZ_OK;
    }
    const cJSON *brief = cJSON_GetObjectItemCaseSensitive(piece, "brief");
    bool has_brief = brief && !cJSON_IsNull(brief);
    if (!composable(piece, has_brief, error)) {
        return URSATZ_OK;
    }
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    if (has_brief && strcmp(structure_material(structure), "style") == 0) {
        /* The style of the brief is built only when composing. */
        workshop_error_text(error, "");
        return URSATZ_OK;
    }
    cJSON *built = NULL;
    UrsatzStatus result = build_request(body, NULL, NULL, &built, error);
    if (result == URSATZ_OK) {
        result = request_with_plans(built, out_request, error);
    }
    cJSON_Delete(built);
    *complete = result == URSATZ_OK;
    return result;
}

UrsatzStatus workshop_piece_compose(const char *id, bool has_seed, long long seed, bool update_uses,
                                                  const UrsatzJob *job, cJSON **out_item, UrsatzError *error) {
    if (out_item) {
        *out_item = NULL;
    }
    workshop_error_text(error, "");
    cJSON *item = NULL;
    UrsatzStatus result = workshop_compose_item(&kPieceKind, id, has_seed, seed, update_uses, job, &item, error);
    if (result == URSATZ_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    return result;
}

/* --- reopening a composed version ---------------------------------------- */

static cJSON *read_json(const char *path) {
    char *text = NULL;
    common_file_read(path, &text, NULL, NULL);
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

/* The name of the first version of a composed piece's result (loop for a game loop, piece for a form with sizes); loop
 * for a result that lists none, as one saved before the forms. */
static const char *first_version_name(const cJSON *piece) {
    const cJSON *versions = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(piece, "result"), "summary"), "versions");
    return cJSON_IsObject(versions) && versions->child && versions->child->string ? versions->child->string : "loop";
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
 * section of the loop (the game loop) or of the first version of the result's summary (a form with sizes), and lane
 * (not NULL) a lane that section can be written again in.
 */
static UrsatzStatus open_source(const char *id, int version, const char *section_id, const char *lane, Source *source,
                                        UrsatzError *error) {
    memset(source, 0, sizeof(*source));
    cJSON *current_body = NULL;
    UrsatzStatus read = workshop_material_item_get(id, &source->meta, &current_body);
    cJSON_Delete(current_body);
    if (read != URSATZ_OK) {
        source_clear(source);
        return read == URSATZ_ERR_NOT_FOUND || read == URSATZ_ERR_REQUEST
                   ? workshop_fail(URSATZ_ERR_NOT_FOUND, NULL)
                   : workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    source->current = cJSON_GetObjectItemCaseSensitive(source->meta, "current_version")->valueint;
    if (strcmp(common_json_string(source->meta, "type"), "piece") != 0) {
        workshop_error_text(error, "the item is not a piece");
        source_clear(source);
        return workshop_fail(URSATZ_ERR_REQUEST, NULL);
    }
    read = workshop_material_item_get_version(id, version, &source->body);
    if (read != URSATZ_OK) {
        source_clear(source);
        workshop_error_text(error, "version not found");
        return read == URSATZ_ERR_NOT_FOUND ? workshop_fail(URSATZ_ERR_NOT_FOUND, NULL) : workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(source->body, "piece");
    char relative[PATH_BYTES];
    char path[PATH_BYTES + 32];
    char resolved[PATH_MAX];
    bool composed = cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(piece, "result")) &&
                    workshop_material_version_dir(id, version, relative, sizeof(relative)) && realpath(relative, resolved) &&
                    snprintf(source->dir, sizeof(source->dir), "%s", resolved) < (int)sizeof(source->dir);
    if (composed) {
        snprintf(path, sizeof(path), "%s/plan.json", source->dir);
        composed = access(path, R_OK) == 0;
        snprintf(path, sizeof(path), "%s/request.json", source->dir);
        composed = composed && access(path, R_OK) == 0;
    }
    if (!composed) {
        workshop_error_text(error, "this version was not composed; compose the piece first");
        source_clear(source);
        return workshop_fail(URSATZ_ERR_REQUEST, NULL);
    }
    if (!section_id) {
        return URSATZ_OK;
    }
    /* A form with sizes has no sections in its structure: its sections are the rows of its first version. */
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    const cJSON *rows =
        structure_sized(structure)
            ? cJSON_GetObjectItemCaseSensitive(
                  cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(
                                                       cJSON_GetObjectItemCaseSensitive(piece, "result"), "summary"), "versions"),
                                                   first_version_name(piece)),
                  "sections")
            : cJSON_GetObjectItemCaseSensitive(structure, "sections");
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, rows) {
        if (strcmp(common_json_string(section, "id"), section_id) == 0) {
            source->section = section;
            break;
        }
    }
    const cJSON *fill = cJSON_GetObjectItemCaseSensitive(source->section, "fill");
    if (!source->section) {
        workshop_error_printf(error, "the piece has no section %s", section_id);
    } else if (strcmp(common_json_string(source->section, "part"), "ending") == 0) {
        workshop_error_printf(error, "section %s is in the ending, which is built from the loop; write a section of the loop again",
                 section_id);
    } else if (lane && !lane_ok(lane)) {
        workshop_error_text(error, "lane is one of harmony, bass, melody, inner, all");
    } else if (cJSON_IsObject(fill) && strcmp(common_json_string(fill, "kind"), "theme") == 0 &&
               (!lane || (strcmp(lane, "harmony") != 0 && strcmp(lane, "bass") != 0))) {
        workshop_error_printf(error, "section %s is filled with a theme: only its harmony or its bass can be written again", section_id);
    } else {
        return URSATZ_OK;
    }
    source_clear(source);
    return workshop_fail(URSATZ_ERR_REQUEST, NULL);
}

/*
 * Copies the request of the version in from_dir into dir with the files it names: the plan of each theme fill and the
 * brief's style file are copied beside it, and the request names the copies, so the new version keeps its own.
 */
static UrsatzStatus carry_request(const char *from_dir, const char *dir, UrsatzError *error) {
    char path[PATH_BYTES + 96];
    char copy[PATH_BYTES + 96];
    snprintf(path, sizeof(path), "%s/request.json", from_dir);
    cJSON *request = read_json(path);
    if (!request) {
        workshop_error_text(error, "the request of the version could not be read");
        return workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    UrsatzStatus result = URSATZ_OK;
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(request, "sections")) {
        cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        if (result == URSATZ_OK && cJSON_IsObject(fill) && strcmp(common_json_string(fill, "kind"), "theme") == 0) {
            char name[96];
            snprintf(name, sizeof(name), "theme-%s.plan.json", common_json_string(section, "id"));
            snprintf(path, sizeof(path), "%s/%s", from_dir, name);
            snprintf(copy, sizeof(copy), "%s/%s", dir, name);
            if (!copy_file(path, copy) || !set_string(fill, "plan", copy)) {
                workshop_error_printf(error, "the plan of the theme in section %s could not be copied", common_json_string(section, "id"));
                result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
            }
        }
    }
    if (result == URSATZ_OK && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(request, "style"))) {
        snprintf(path, sizeof(path), "%s/brief.style.json", from_dir);
        snprintf(copy, sizeof(copy), "%s/brief.style.json", dir);
        if (!copy_file(path, copy) || !set_string(request, "style", copy)) {
            workshop_error_text(error, "the style file of the version could not be copied");
            result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
        }
    }
    if (result == URSATZ_OK) {
        char *text = cJSON_Print(request);
        snprintf(copy, sizeof(copy), "%s/request.json", dir);
        if (!text || !(common_file_write_atomic(copy, text, strlen(text), NULL) == URSATZ_OK)) {
            workshop_error_text(error, "the request could not be written");
            result = text ? workshop_fail(URSATZ_ERR_INTERNAL, NULL) : workshop_fail(URSATZ_ERR_ALLOC, NULL);
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

/* Writes new_body as the next version. */
static UrsatzStatus add_version(const char *id, const cJSON *new_body, const char *note, cJSON **out_item,
                                        UrsatzError *error) {
    if (workshop_material_item_add_version(id, new_body, note, out_item) != URSATZ_OK) {
        workshop_error_text(error, "the version could not be written");
        return workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    return URSATZ_OK;
}

/* Names the version's own folder in the request file of the stage and in summary instead of the stage's. */
static UrsatzStatus rebase_request(const WorkshopComposeStage *stage, cJSON *summary, UrsatzError *error) {
    char path[PATH_BYTES + 96];
    snprintf(path, sizeof(path), "%s/request.json", stage->absolute);
    cJSON *request = read_json(path);
    char *text = NULL;
    if (request) {
        workshop_compose_rebase(request, stage->absolute, stage->named);
        text = cJSON_Print(request);
    }
    bool ok = text && (common_file_write_atomic(path, text, strlen(text), NULL) == URSATZ_OK);
    free(text);
    cJSON_Delete(request);
    workshop_compose_rebase(summary, stage->absolute, stage->named);
    if (!ok) {
        workshop_error_text(error, "the request could not be written");
        return workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    return URSATZ_OK;
}

/* Reads the request and the loop plan of the source version into *request and *from, or says why not. */
static UrsatzStatus read_inputs(const char *request_dir, const char *plan_path, cJSON **request, cJSON **from,
                                        UrsatzError *error) {
    char path[PATH_BYTES + 96];
    snprintf(path, sizeof(path), "%s/request.json", request_dir);
    *request = read_json(path);
    *from = read_json(plan_path);
    if (!*request || !*from) {
        workshop_error_text(error, "the request or the plan of the version could not be read");
        cJSON_Delete(*request);
        cJSON_Delete(*from);
        *request = NULL;
        *from = NULL;
        return workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    return URSATZ_OK;
}

static UrsatzStatus regenerate_locked(const char *id, int version, const char *section_id, const char *lane,
                                              bool has_seed, long long seed, const UrsatzJob *job, cJSON **out_item,
                                              UrsatzError *error) {
    Source source;
    UrsatzStatus result = open_source(id, version, section_id, lane, &source, error);
    if (result != URSATZ_OK) {
        return result;
    }
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(source.body, "piece");
    const cJSON *brief_json = cJSON_GetObjectItemCaseSensitive(piece, "brief");
    bool has_brief = brief_json && !cJSON_IsNull(brief_json);
    WorkshopComposeStage stage;
    char plan_path[PATH_BYTES + 32];
    cJSON *request = NULL;
    cJSON *from = NULL;
    cJSON *summary = NULL;
    cJSON *new_body = NULL;
    cJSON *item = NULL;
    result = workshop_compose_stage_make(id, source.current + 1, &stage, error);
    bool made = result == URSATZ_OK;
    if (result == URSATZ_OK) {
        result = carry_request(source.dir, stage.absolute, error);
    }
    if (result == URSATZ_OK) {
        snprintf(plan_path, sizeof(plan_path), "%s/plan.json", source.dir);
        result = read_inputs(stage.absolute, plan_path, &request, &from, error);
    }
    if (result == URSATZ_OK) {
        result = run_piece(stage.absolute, stage.named, request, cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(piece, "check")),
                           has_seed, seed, from, section_id, lane, job, &summary, error);
    }
    if (result == URSATZ_OK) {
        result = rebase_request(&stage, summary, error);
    }
    if (result == URSATZ_OK) {
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
            result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
        } else {
            result = add_version(id, new_body, note, &item, error);
        }
    }
    if (result == URSATZ_OK) {
        workshop_compose_stage_finish(id, item, &stage);
        *out_item = item;
    } else if (made) {
        workshop_compose_stage_discard(&stage);
    }
    cJSON_Delete(request);
    cJSON_Delete(from);
    cJSON_Delete(summary);
    cJSON_Delete(new_body);
    source_clear(&source);
    return result;
}

UrsatzStatus workshop_piece_regenerate(const char *id, int version, const char *section_id, const char *lane,
                                                 bool has_seed, long long seed, const UrsatzJob *job, cJSON **out_item,
                                                 UrsatzError *error) {
    if (out_item) {
        *out_item = NULL;
    }
    workshop_error_text(error, "");
    cJSON *item = NULL;
    UrsatzStatus result =
        regenerate_locked(id, version, section_id, lane, has_seed, seed, job, &item, error);
    if (result == URSATZ_OK && out_item) {
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
        if (!workshop_material_version_dir(id, number, dir, sizeof(dir))) {
            continue;
        }
        CommonNames names = {0};
        if (common_dir_list(dir, &names, NULL) != URSATZ_OK) {
            continue;
        }
        for (size_t i = 0; i < names.count; i++) {
            const char *name = names.items[i];
            char *end = NULL;
            if (strncmp(name, "variations-", 11) == 0) {
                long set = strtol(name + 11, &end, 10);
                if (end != name + 11 && *end == '\0' && set > highest && set < 1000000) {
                    highest = (int)set;
                }
            }
        }
        common_names_free(&names);
    }
    return highest + 1;
}

/* The row of section_id in the sections of a version of the result (the loop of a game loop, the piece of a form). */
static const cJSON *loop_section(const cJSON *loop, const char *section_id) {
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(loop, "sections")) {
        if (strcmp(common_json_string(section, "id"), section_id) == 0) {
            return section;
        }
    }
    return NULL;
}

/* A copy of member, or a null when there is none. */
static cJSON *copy_or_null(const cJSON *member) {
    return member ? cJSON_Duplicate(member, 1) : cJSON_CreateNull();
}

/* {set, variations: [{index, seed, score, seam, heard}]} from the Composer's result.json, each variation's version read
 * under name (the first version of the piece's form). NULL on allocation failure. */
static cJSON *variations_answer(const cJSON *result, const char *name, const char *section_id, int set) {
    cJSON *answer = cJSON_CreateObject();
    cJSON *list = cJSON_CreateArray();
    bool ok = answer && list && cJSON_AddNumberToObject(answer, "set", set);
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(result, "variations")) {
        const cJSON *loop = cJSON_GetObjectItemCaseSensitive(entry, name);
        const cJSON *heard = cJSON_GetObjectItemCaseSensitive(loop_section(loop, section_id), "heard");
        cJSON *object = ok ? cJSON_CreateObject() : NULL;
        ok = object && common_json_add_copy_or_null(object, entry, "index") && common_json_add_copy_or_null(object, entry, "seed") &&
             common_json_add(object, "score", copy_or_null(cJSON_GetObjectItemCaseSensitive(loop, "score"))) &&
             common_json_add(object, "seam", copy_or_null(cJSON_GetObjectItemCaseSensitive(loop, "seam"))) &&
             common_json_add(object, "heard", copy_or_null(heard)) &&
             cJSON_AddItemToArray(list, object);
        if (!ok) {
            cJSON_Delete(object);
        }
    }
    if (ok) {
        ok = common_json_add(answer, "variations", list);
        list = NULL;
    }
    cJSON_Delete(list);
    if (!ok) {
        cJSON_Delete(answer);
        return NULL;
    }
    return answer;
}

/* Makes the folder of the next variations set of the item: the first number from next_set whose folder this call makes. */
static int make_set_dir(const char *source_dir, const char *id, int current, char *out_dir, size_t out_size) {
    for (int set = next_set(id, current), tries = 0; tries < 1000; set++, tries++) {
        snprintf(out_dir, out_size, "%s/variations-%d", source_dir, set);
        if (mkdir(out_dir, 0755) == 0) {
            return set;
        }
    }
    return 0;
}

/*
 * Makes the variations with the Composer API and writes variation-<i>.json, .mid and .musicxml of the loop version and
 * result.json into out_dir (cli/piece.c write_variations). *out_document is the result.json document.
 */
static UrsatzStatus run_variations(const Source *source, const char *name, const cJSON *request, const cJSON *from, const char *section_id,
                                           const char *lane, int count, bool check, bool has_seed, long long seed,
                                           const UrsatzJob *job, const char *out_dir, cJSON **out_document, UrsatzError *error) {
    cJSON *with_plans = NULL;
    UrsatzStatus result = request_with_plans(request, &with_plans, error);
    if (result != URSATZ_OK) {
        return result;
    }
    (void)source;
    cJSON *api = cJSON_CreateObject();
    cJSON *edit = cJSON_CreateObject();
    cJSON *lanes = cJSON_CreateArray();
    char count_text[16];
    char seed_text[32];
    snprintf(count_text, sizeof(count_text), "%d", count);
    snprintf(seed_text, sizeof(seed_text), "%lld", seed);
    bool whole = !lane || strcmp(lane, "all") == 0;
    bool ok = api && edit && lanes && cJSON_AddItemToObject(api, "request", with_plans);
    if (!ok) {
        cJSON_Delete(with_plans);
    }
    ok = ok && cJSON_AddItemToObject(api, "from", cJSON_Duplicate(from, 1)) &&
         (whole || cJSON_AddItemToArray(lanes, cJSON_CreateString(lane))) && cJSON_AddItemToObject(edit, "lanes", lanes);
    lanes = ok ? NULL : lanes;
    ok = ok && cJSON_AddStringToObject(edit, "section", section_id) && cJSON_AddStringToObject(edit, "count", count_text) &&
         cJSON_AddItemToObject(api, "variations", edit);
    edit = ok ? NULL : edit;
    ok = ok && (!has_seed || cJSON_AddStringToObject(api, "seed", seed_text)) && cJSON_AddBoolToObject(api, "check", check) &&
         cJSON_AddBoolToObject(api, "plain_midi", 0) && cJSON_AddBoolToObject(api, "musicxml", 1);
    char *text = ok ? cJSON_PrintUnformatted(api) : NULL;
    cJSON_Delete(api);
    cJSON_Delete(edit);
    cJSON_Delete(lanes);
    if (!text) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    UrsatzStatus status = composer_api_piece_variations(workshop_composer_context_global(), text, job, &response);
    free(text);
    cJSON *document = NULL;
    if (status != URSATZ_OK) {
        result = workshop_compose_failure(status, &response, error);
    } else if (!(document = cJSON_Parse(response.json))) {
        workshop_error_text(error, "the variations could not be read");
        result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    double qpm = document ? document_tempo(document) : 0;
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(document, "variations");
    int kept = cJSON_GetArraySize(list);
    for (int i = 0; result == URSATZ_OK && i < kept && i < 8; i++) {
        cJSON *entry = cJSON_GetArrayItem(list, i);
        cJSON *loop = cJSON_GetObjectItemCaseSensitive(entry, name);
        const cJSON *plan = cJSON_GetObjectItemCaseSensitive(loop, "plan");
        const cJSON *musicxml = cJSON_GetObjectItemCaseSensitive(entry, "musicxml");
        char names[3][64];
        char paths[3][PATH_BYTES + 128];
        static const char *const extensions[] = {"json", "mid", "musicxml"};
        for (int k = 0; k < 3; k++) {
            snprintf(names[k], sizeof(names[k]), "variation-%d.%s", i + 1, extensions[k]);
            snprintf(paths[k], sizeof(paths[k]), "%s/%s", out_dir, names[k]);
        }
        ComposerResponse exported;
        memset(&exported, 0, sizeof(exported));
        result = cJSON_IsObject(plan) && cJSON_IsString(musicxml) ? URSATZ_OK : workshop_fail(URSATZ_ERR_INTERNAL, NULL);
        if (result != URSATZ_OK) {
            workshop_error_text(error, "the variations could not be read");
            break;
        }
        result = workshop_compose_write_plan(plan, paths[0], error);
        if (result == URSATZ_OK) {
            result = workshop_compose_export(plan, qpm, false, true, false, cJSON_GetObjectItemCaseSensitive(entry, "loop_markers"),
                                               job, &exported, error);
        }
        if (result == URSATZ_OK &&
            (!write_bytes(out_dir, names[1], exported.midi, exported.midi_size, error) ||
             !write_bytes(out_dir, names[2], musicxml->valuestring, strlen(musicxml->valuestring), error))) {
            result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
        }
        composer_response_destroy(&exported);
        /* The document as printed: the plan of the version is its file, and the files are named. */
        if (result == URSATZ_OK) {
            set_string(loop, "plan", paths[0]);
            set_string(entry, "midi", paths[1]);
            set_string(entry, "musicxml", paths[2]);
            cJSON_DeleteItemFromObjectCaseSensitive(entry, "loop_markers");
        }
    }
    if (result == URSATZ_OK) {
        cJSON_DeleteItemFromObjectCaseSensitive(document, "brief");
        char *printed = cJSON_PrintUnformatted(document);
        char *file = printed ? malloc(strlen(printed) + 2) : NULL;
        if (file) {
            sprintf(file, "%s\n", printed);
        }
        if (!file || !write_bytes(out_dir, "result.json", file, strlen(file), error)) {
            result = file ? workshop_fail(URSATZ_ERR_INTERNAL, NULL) : workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
        free(file);
        free(printed);
    }
    composer_response_destroy(&response);
    if (result != URSATZ_OK) {
        cJSON_Delete(document);
        return result;
    }
    *out_document = document;
    return URSATZ_OK;
}

static UrsatzStatus variations_locked(const char *id, int version, const char *section_id, const char *lane, int count,
                                              bool has_seed, long long seed, const UrsatzJob *job, cJSON **out_result,
                                              UrsatzError *error) {
    if (!lane || count < 1 || count > 8) {
        workshop_error_text(error, "variations need a lane and a count from 1 to 8");
        return workshop_fail(URSATZ_ERR_REQUEST, NULL);
    }
    Source source;
    UrsatzStatus result = open_source(id, version, section_id, lane, &source, error);
    if (result != URSATZ_OK) {
        return result;
    }
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(source.body, "piece");
    char out_dir[PATH_BYTES + 32];
    char plan_path[PATH_BYTES + 32];
    cJSON *request = NULL;
    cJSON *from = NULL;
    cJSON *written = NULL;
    snprintf(plan_path, sizeof(plan_path), "%s/plan.json", source.dir);
    result = read_inputs(source.dir, plan_path, &request, &from, error);
    int set = 0;
    if (result == URSATZ_OK) {
        set = make_set_dir(source.dir, id, source.current, out_dir, sizeof(out_dir));
        if (set == 0) {
            workshop_error_text(error, "the folder of the variations could not be made");
            result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
        }
    }
    if (result == URSATZ_OK) {
        result = run_variations(&source, first_version_name(piece), request, from, section_id, lane, count,
                                cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(piece, "check")), has_seed, seed, job, out_dir,
                                &written, error);
        if (result != URSATZ_OK) {
            common_dir_remove(out_dir, NULL);
        }
    }
    if (result == URSATZ_OK) {
        *out_result = variations_answer(written, first_version_name(piece), section_id, set);
        if (!*out_result) {
            workshop_error_text(error, "the variations could not be read");
            common_dir_remove(out_dir, NULL);
            result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
        }
    }
    cJSON_Delete(written);
    cJSON_Delete(request);
    cJSON_Delete(from);
    source_clear(&source);
    return result;
}

UrsatzStatus workshop_piece_variations(const char *id, int version, const char *section_id, const char *lane,
                                                 int count, bool has_seed, long long seed, const UrsatzJob *job,
                                                 cJSON **out_result, UrsatzError *error) {
    if (out_result) {
        *out_result = NULL;
    }
    workshop_error_text(error, "");
    cJSON *answer = NULL;
    UrsatzStatus result =
        variations_locked(id, version, section_id, lane, count, has_seed, seed, job, &answer, error);
    if (result == URSATZ_OK && out_result) {
        *out_result = answer;
    } else {
        cJSON_Delete(answer);
    }
    return result;
}

static UrsatzStatus choose_locked(const char *id, int version, int set, int index, const UrsatzJob *job,
                                          cJSON **out_item, UrsatzError *error) {
    Source source;
    UrsatzStatus result = open_source(id, version, NULL, NULL, &source, error);
    if (result != URSATZ_OK) {
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
        workshop_error_printf(error, "variation %d is not in set %d of version %d", index, set, version);
        cJSON_Delete(written);
        source_clear(&source);
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    WorkshopComposeStage stage;
    cJSON *request = NULL;
    cJSON *from_plan = NULL;
    cJSON *summary = NULL;
    cJSON *new_body = NULL;
    cJSON *item = NULL;
    result = workshop_compose_stage_make(id, source.current + 1, &stage, error);
    bool made = result == URSATZ_OK;
    if (result == URSATZ_OK) {
        result = carry_request(source.dir, stage.absolute, error);
    }
    if (result == URSATZ_OK) {
        result = read_inputs(stage.absolute, from, &request, &from_plan, error);
    }
    if (result == URSATZ_OK) {
        result = run_piece(stage.absolute, stage.named, request, false, false, 0, from_plan, "none", NULL, job, &summary, error);
    }
    if (result == URSATZ_OK) {
        result = rebase_request(&stage, summary, error);
    }
    if (result == URSATZ_OK) {
        /* The Composer's run only built the versions after the first; the first (the loop of a game loop, the piece of a
           form with sizes) is the variation's, with its seed. */
        const char *name = first_version_name(piece);
        const cJSON *seed = cJSON_GetObjectItemCaseSensitive(chosen, "seed");
        cJSON *versions = cJSON_GetObjectItemCaseSensitive(summary, "versions");
        const cJSON *loop = cJSON_GetObjectItemCaseSensitive(chosen, name);
        bool ok = cJSON_IsNumber(seed) && cJSON_IsObject(versions) && cJSON_IsObject(loop);
        if (ok) {
            cJSON *replacement = cJSON_Duplicate(loop, 1);
            /* In place: the first member of the versions names the first version (first_version_name). */
            ok = replacement && cJSON_ReplaceItemInObjectCaseSensitive(versions, name, replacement);
            if (!ok) {
                cJSON_Delete(replacement);
            }
            cJSON_DeleteItemFromObjectCaseSensitive(summary, "seed");
            ok = ok && common_json_add(summary, "seed", cJSON_CreateNumber(seed->valuedouble));
        }
        cJSON *brief_summary = ok ? kept_brief_summary(&source) : NULL;
        char note[96];
        snprintf(note, sizeof(note), "chose variation %d seed %.0f", index, ok ? seed->valuedouble : 0.0);
        new_body = brief_summary ? body_with_result(source.body, summary, brief_summary, has_brief) : NULL;
        summary = NULL;
        if (!new_body) {
            workshop_error_text(error, "the version could not be made from the variation");
            result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
        } else {
            result = add_version(id, new_body, note, &item, error);
        }
    }
    if (result == URSATZ_OK) {
        workshop_compose_stage_finish(id, item, &stage);
        *out_item = item;
    } else if (made) {
        workshop_compose_stage_discard(&stage);
    }
    cJSON_Delete(written);
    cJSON_Delete(request);
    cJSON_Delete(from_plan);
    cJSON_Delete(summary);
    cJSON_Delete(new_body);
    source_clear(&source);
    return result;
}

UrsatzStatus workshop_piece_choose(const char *id, int version, int set, int index, const UrsatzJob *job,
                                             cJSON **out_item, UrsatzError *error) {
    if (out_item) {
        *out_item = NULL;
    }
    workshop_error_text(error, "");
    cJSON *item = NULL;
    UrsatzStatus result = choose_locked(id, version, set, index, job, &item, error);
    if (result == URSATZ_OK && out_item) {
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

/* The checked path of a variations-set file, or NOT_FOUND. type is the file's content type when it is wanted. */
static UrsatzStatus variation_path_checked(const char *id, int version, int set, const char *name, char *path,
                                                   size_t path_size, const char **out_type) {
    const char *type = name ? variation_file_type(name) : NULL;
    cJSON *meta = NULL;
    cJSON *body = NULL;
    char dir[PATH_BYTES];
    if (!type || set < 1 || !workshop_material_version_dir(id, version, dir, sizeof(dir)) ||
        workshop_material_item_get(id, &meta, &body) != URSATZ_OK) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    bool found = version <= cJSON_GetObjectItemCaseSensitive(meta, "current_version")->valueint &&
                 strcmp(common_json_string(meta, "type"), "piece") == 0;
    cJSON_Delete(meta);
    cJSON_Delete(body);
    if (!found || snprintf(path, path_size, "%s/variations-%d/%s", dir, set, name) >= (int)path_size) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    *out_type = type;
    return URSATZ_OK;
}

UrsatzStatus workshop_piece_variation_midi_path(const char *id, int version, int set, const char *name,
                                                          char *out, size_t out_size) {
    const char *type = NULL;
    if (variation_path_checked(id, version, set, name, out, out_size, &type) != URSATZ_OK ||
        strcmp(type, "audio/midi") != 0) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    FILE *file = fopen(out, "rb");
    if (!file) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    fclose(file);
    return URSATZ_OK;
}

UrsatzStatus workshop_piece_variation_file(const char *id, int version, int set, const char *name,
                                                     unsigned char **out_data, size_t *out_size, const char **out_type) {
    *out_data = NULL;
    *out_size = 0;
    const char *type = NULL;
    char path[PATH_BYTES + 96];
    if (variation_path_checked(id, version, set, name, path, sizeof(path), &type) != URSATZ_OK) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    char *data = NULL;
    size_t length = 0;
    UrsatzStatus read = common_file_read(path, &data, &length, NULL);
    if (read == URSATZ_ERR_ALLOC) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    if (read != URSATZ_OK) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    *out_data = (unsigned char *)data;
    *out_size = length;
    *out_type = type;
    return URSATZ_OK;
}
