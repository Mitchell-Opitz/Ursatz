#define _XOPEN_SOURCE 700

#include "piece_service.h"
#include "util/fs_util.h"
#include "util/json_util.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "compose_common.h"
#include "composer_client.h"
#include "material_service.h"

#define PATH_BYTES 800

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
        if (strcmp(ursatz_gui_json_string(use, "item_id"), item_id) == 0 && cJSON_IsNumber(number) && number->valueint == version) {
            return cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(use, "copy"), "motif");
        }
    }
    return NULL;
}

/* The motive of a reference {item_id, version}, or NULL when its copy has no degrees. */
static cJSON *motive_of(const cJSON *body, const cJSON *reference, char *message, size_t message_size, const char *what) {
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(reference, "version");
    const cJSON *motif = used_motif(body, ursatz_gui_json_string(reference, "item_id"), cJSON_IsNumber(version) ? version->valueint : 0);
    if (!motif || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(motif, "degrees"))) {
        snprintf(message, message_size, "the motif of %s has no degrees", what);
        return NULL;
    }
    return ursatz_gui_theme_motive_json(NULL, NULL, motif, "melodic");
}

/* A section's fill as the request holds it. The plan of a theme fill is copied into dir (NULL: not copied, the request
   names the source plan). */
static UrsatzGuiThemeResult build_fill(const cJSON *body, const cJSON *derived_list, const char *pool, const cJSON *section,
                                       const char *dir, cJSON **out, char *message, size_t message_size) {
    const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
    const char *id = ursatz_gui_json_string(section, "id");
    const char *kind = cJSON_IsObject(fill) ? ursatz_gui_json_string(fill, "kind") : "empty";
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
        ok = ursatz_gui_json_attach(object, "motive", motive);
        ok = ursatz_gui_json_attach(object, "forms", forms) && ok;
        if (ok) {
            result = ursatz_gui_theme_add_pool(derived_list, ursatz_gui_json_string(fill, "item_id"), pool, forms);
        }
        ok = ok && ursatz_gui_json_add_copy_if(object, fill, "into") && ursatz_gui_json_add_copy_if(object, fill, "hybrid");
    } else if (ok && strcmp(kind, "theme") == 0) {
        char source[PATH_BYTES];
        char plan[PATH_BYTES + 16];
        char copy[PATH_BYTES + 64];
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(fill, "version");
        if (!ursatz_gui_material_version_dir(ursatz_gui_json_string(fill, "item_id"), cJSON_IsNumber(version) ? version->valueint : 0, source,
                                             sizeof(source))) {
            ursatz_gui_copy_message(message, message_size, "a theme fill names no version");
            result = URSATZ_GUI_THEME_ERR_INVALID;
        } else {
            snprintf(plan, sizeof(plan), "%s/plan.json", source);
            snprintf(copy, sizeof(copy), "%s/theme-%s.plan.json", dir ? dir : "", id);
            if (!dir) {
                /* A check writes nothing: the request names the version's own plan. */
                snprintf(copy, sizeof(copy), "%s", plan);
                ok = cJSON_AddStringToObject(object, "plan", copy) != NULL;
            } else if (!copy_file(plan, copy)) {
                snprintf(message, message_size, "the plan of the theme in section %s could not be copied", id);
                result = URSATZ_GUI_THEME_ERR_INVALID;
            } else {
                ok = cJSON_AddStringToObject(object, "plan", copy) != NULL;
            }
        }
    } else if (ok && strcmp(kind, "progression") == 0) {
        ok = ursatz_gui_json_add_copy_or_null(object, fill, "numerals") && ursatz_gui_json_add_copy_if(object, fill, "cells");
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
    bool ok = object && ursatz_gui_json_add_copy_or_null(object, section, "id") && ursatz_gui_json_add_copy_or_null(object, section, "part") &&
              ursatz_gui_json_add_copy_if(object, section, "letter") && ursatz_gui_json_add_copy_or_null(object, section, "beats") &&
              ursatz_gui_json_attach(object, "pickup", cJSON_IsString(pickup) ? cJSON_Duplicate(pickup, 1) : cJSON_CreateString("0")) &&
              ursatz_gui_json_add_copy_or_null(object, section, "cadence");
    ok = ursatz_gui_json_attach(object, "fill", fill) && ok;
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
    const cJSON *form = ursatz_gui_composer_form(structure_form(structure));
    const char *sections = ursatz_gui_json_string(form, "sections");
    return form && sections && strcmp(sections, "request") != 0;
}

/* What the request needs from the piece, checked before anything is made. A form with sizes asks only key, mode and
 * meter. */
static bool composable(const cJSON *piece, bool has_brief, char *message, size_t message_size) {
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    bool sized = cJSON_IsObject(structure) && structure_sized(structure);
    if (!cJSON_IsObject(structure) || (!sized && !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(structure, "sections"))) ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(structure, "key")) ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(structure, "mode")) ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(structure, "meter"))) {
        ursatz_gui_copy_message(message, message_size, sized ? "the piece structure needs key, mode and meter"
                                                             : "the piece structure needs key, mode, meter and sections");
        return false;
    }
    if (strcmp(ursatz_gui_json_string(structure, "mode"), "style") == 0 && !has_brief) {
        ursatz_gui_copy_message(message, message_size, "the mode style needs a brief");
        return false;
    }
    const cJSON *section = NULL;
    const cJSON *own_sections = sized ? NULL : cJSON_GetObjectItemCaseSensitive(structure, "sections");
    cJSON_ArrayForEach(section, own_sections) {
        const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        if (cJSON_IsObject(fill) && strcmp(ursatz_gui_json_string(fill, "kind"), "motif") == 0 &&
            strcmp(ursatz_gui_json_string(fill, "into"), "hybrid") == 0 && !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(fill, "hybrid"))) {
            snprintf(message, message_size, "the hybrid motif fill of section %s needs hybrid", ursatz_gui_json_string(section, "id"));
            return false;
        }
    }
    return true;
}

/*
 * The piece request of the Composer (docs/piece-request.md) for the current version of a piece. dir is the new version's
 * folder (absolute), where theme plans are copied; NULL writes nothing (validate). style_path is the brief's style file, or NULL without a brief.
 */
static UrsatzGuiThemeResult build_request(const cJSON *body, const char *dir, const char *style_path, cJSON **out,
                                          char *message, size_t message_size) {
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(body, "piece");
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    const char *pool = cJSON_IsString(cJSON_GetObjectItemCaseSensitive(piece, "pool")) ? ursatz_gui_json_string(piece, "pool")
                                                                                       : "approved-and-unflagged";
    bool sized = structure_sized(structure);
    cJSON *request = cJSON_CreateObject();
    cJSON *sections = cJSON_CreateArray();
    cJSON *derived_list = NULL;
    UrsatzGuiThemeResult result = URSATZ_GUI_THEME_OK;
    if (!request || !sections) {
        result = URSATZ_GUI_THEME_ERR_ALLOC;
    }
    if (result == URSATZ_GUI_THEME_OK && ursatz_gui_material_derived_motifs(&derived_list) != URSATZ_GUI_MATERIAL_OK) {
        ursatz_gui_copy_message(message, message_size, "the saved forms could not be read");
        result = URSATZ_GUI_THEME_ERR_FAILED;
    }
    const cJSON *section = NULL;
    const cJSON *own_sections = sized ? NULL : cJSON_GetObjectItemCaseSensitive(structure, "sections");
    cJSON_ArrayForEach(section, own_sections) {
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
              cJSON_AddNumberToObject(request, "schema_version", 2) &&
              cJSON_AddStringToObject(request, "form", structure_form(structure)) && ursatz_gui_json_add_copy_if(request, structure, "key") &&
              ursatz_gui_json_add_copy_if(request, structure, "mode") && ursatz_gui_json_add_copy_if(request, structure, "meter");
    const cJSON *motive_ref = cJSON_GetObjectItemCaseSensitive(piece, "motive");
    if (ok && cJSON_IsObject(motive_ref)) {
        cJSON *motive = motive_of(body, motive_ref, message, message_size, "the piece motive");
        if (!motive) {
            result = URSATZ_GUI_THEME_ERR_INVALID;
            ok = false;
        } else {
            ok = ursatz_gui_json_attach(request, "motive", motive);
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
    } else if (ok) {
        ok = ursatz_gui_json_attach(request, "sections", sections);
        sections = NULL;
    }

    /* The accompaniment: nothing or a pattern; the pattern motive is not available in a piece. */
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(piece, "accompaniment");
    if (ok && (!accompaniment || cJSON_IsNull(accompaniment))) {
        ok = cJSON_AddNullToObject(request, "accompaniment") != NULL;
    } else if (ok) {
        cJSON *pattern = cJSON_CreateObject();
        ok = pattern && cJSON_AddStringToObject(pattern, "pattern", ursatz_gui_json_string(accompaniment, "pattern"));
        if (ok) {
            ok = ursatz_gui_json_attach(request, "accompaniment", pattern);
        } else {
            cJSON_Delete(pattern);
        }
    }
    ok = ok && (sized || cJSON_AddNullToObject(request, "voicing") != NULL);
    if (ok && style_path) {
        cJSON *without = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(piece, "without"), 1);
        ok = cJSON_AddStringToObject(request, "style", style_path) && ursatz_gui_json_add_copy_if(request, piece, "style_weight");
        if (ok && without && cJSON_GetArraySize(without) > 0) {
            ok = ursatz_gui_json_attach(request, "without", without);
        } else {
            cJSON_Delete(without);
        }
    }
    ok = ok && ursatz_gui_json_add_copy_or_null(request, piece, "tempo");

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

/* The files every version has besides those of the versions of its form (<name>.mid and the rest, R-430). */
static const char *const kFileNames[] = {"request.json", "brief.style.json"};
#define FILE_NAME_COUNT 2
#define VERSIONS_MAX 4

static cJSON *read_json(const char *path);
static bool set_string(cJSON *object, const char *name, const char *text);

/* A copy of the request as the command read it: the plan file of each theme fill read into the request (the API takes
   plans, not paths; cli/piece.c read_request_file) and its path kept as the source. */
static UrsatzGuiThemeResult request_with_plans(const cJSON *request, cJSON **out, char *message, size_t message_size) {
    cJSON *copy = cJSON_Duplicate(request, 1);
    if (!copy) {
        return URSATZ_GUI_THEME_ERR_ALLOC;
    }
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(copy, "sections")) {
        cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        const cJSON *plan_path = cJSON_GetObjectItemCaseSensitive(fill, "plan");
        if (!cJSON_IsObject(fill) || strcmp(ursatz_gui_json_string(fill, "kind"), "theme") != 0 || !cJSON_IsString(plan_path)) {
            continue;
        }
        cJSON *plan = read_json(plan_path->valuestring);
        char *source = strdup(plan_path->valuestring);
        if (!plan || !source || !cJSON_ReplaceItemInObject(fill, "plan", plan) || !cJSON_AddStringToObject(fill, "source", source)) {
            snprintf(message, message_size, "section %s: cannot read the theme plan '%s'", ursatz_gui_json_string(section, "id"),
                     plan_path->valuestring);
            free(source);
            cJSON_Delete(copy);
            return plan ? URSATZ_GUI_THEME_ERR_ALLOC : URSATZ_GUI_THEME_ERR_INVALID;
        }
        free(source);
    }
    *out = copy;
    return URSATZ_GUI_THEME_OK;
}

static double document_tempo(const cJSON *document) {
    const cJSON *qpm = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "brief"), "tempo"), "qpm");
    return cJSON_IsNumber(qpm) ? qpm->valuedouble : 0;
}


static bool write_bytes(const char *dir, const char *name, const void *data, size_t size, char *message, size_t message_size) {
    char path[PATH_BYTES + 128];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (!ursatz_gui_write_file_atomic(path, data, size)) {
        ursatz_gui_copy_message(message, message_size, "a file of the version could not be written");
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
static UrsatzGuiThemeResult run_piece(const char *stage, const char *named, const cJSON *request, bool check, bool use_seed,
                                      long long seed, const cJSON *from, const char *regenerate, const char *lane,
                                      const ComposerJob *job, cJSON **out_summary, char *message, size_t message_size) {
    cJSON *with_plans = NULL;
    UrsatzGuiThemeResult result = request_with_plans(request, &with_plans, message, message_size);
    if (result != URSATZ_GUI_THEME_OK) {
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
        return URSATZ_GUI_THEME_ERR_ALLOC;
    }
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    ComposerStatus status = composer_api_piece(text, job, &response);
    free(text);
    cJSON *document = NULL;
    ComposerResponse extra;
    memset(&extra, 0, sizeof(extra));
    if (status != COMPOSER_OK) {
        result = ursatz_gui_compose_failure(status, &response, message, message_size);
    } else if (!(document = cJSON_Parse(response.json)) || !response.midi || !response.musicxml) {
        ursatz_gui_copy_message(message, message_size, "composer output unreadable");
        result = URSATZ_GUI_THEME_ERR_FAILED;
    }
    const cJSON *plans = cJSON_GetObjectItemCaseSensitive(document, "plans");
    const cJSON *versions = cJSON_GetObjectItemCaseSensitive(document, "versions");
    const cJSON *xmls = cJSON_GetObjectItemCaseSensitive(document, "musicxml_versions");
    int version_count = cJSON_GetArraySize(versions);
    if (result == URSATZ_GUI_THEME_OK && (!cJSON_IsObject(plans) || !cJSON_IsObject(versions) || !cJSON_IsObject(xmls) ||
                                          version_count < 1 || version_count > VERSIONS_MAX)) {
        ursatz_gui_copy_message(message, message_size, "composer output unreadable");
        result = URSATZ_GUI_THEME_ERR_FAILED;
    }
    /* One plan, MIDI and MusicXML file for each version of the form, in the order the Composer lists them: the first
       is the response itself, a later one is exported from its plan and its MusicXML is in musicxml_versions. */
    char path[PATH_BYTES + 128];
    char names[VERSIONS_MAX][96];
    int written = 0;
    const cJSON *version = NULL;
    cJSON_ArrayForEach(version, versions) {
        if (result != URSATZ_GUI_THEME_OK) {
            break;
        }
        const char *name = version->string;
        const cJSON *plan = cJSON_GetObjectItemCaseSensitive(plans, name);
        const cJSON *xml = written == 0 ? NULL : cJSON_GetObjectItemCaseSensitive(xmls, name);
        if (!ursatz_gui_material_version_name_ok(name) || !plan || (written > 0 && !cJSON_IsString(xml))) {
            ursatz_gui_copy_message(message, message_size, "composer output unreadable");
            result = URSATZ_GUI_THEME_ERR_FAILED;
            break;
        }
        char file[96];
        snprintf(file, sizeof(file), written == 0 ? "plan.json" : "%s-plan.json", name);
        snprintf(path, sizeof(path), "%s/%s", stage, file);
        result = ursatz_gui_compose_write_plan(plan, path, message, message_size);
        snprintf(names[written], sizeof(names[written]), "%s", file);
        if (result == URSATZ_GUI_THEME_OK) {
            composer_response_destroy(&extra);
            memset(&extra, 0, sizeof(extra));
            const uint8_t *midi = response.midi;
            size_t midi_size = response.midi_size;
            if (written > 0) {
                result = ursatz_gui_compose_export(plan, document_tempo(document), false, true, false, NULL, job, &extra, message,
                                                   message_size);
                midi = extra.midi;
                midi_size = extra.midi_size;
            }
            snprintf(file, sizeof(file), "%s.mid", name);
            bool wrote = result == URSATZ_GUI_THEME_OK && write_bytes(stage, file, midi, midi_size, message, message_size);
            const char *text = written == 0 ? response.musicxml : xml->valuestring;
            snprintf(file, sizeof(file), "%s.musicxml", name);
            wrote = wrote && write_bytes(stage, file, text, strlen(text), message, message_size);
            if (result == URSATZ_GUI_THEME_OK && !wrote) {
                result = URSATZ_GUI_THEME_ERR_FAILED;
            }
        }
        written++;
    }
    if (result == URSATZ_GUI_THEME_OK) {
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
                result = URSATZ_GUI_THEME_ERR_ALLOC;
                break;
            }
        }
    }
    composer_response_destroy(&extra);
    composer_response_destroy(&response);
    if (result != URSATZ_GUI_THEME_OK) {
        cJSON_Delete(document);
        return result;
    }
    *out_summary = document;
    return URSATZ_GUI_THEME_OK;
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
              ursatz_gui_json_attach(result, "seed", seed ? cJSON_CreateNumber(seed->valuedouble) : cJSON_CreateNull()) &&
              ursatz_gui_json_attach(result, "loop", loop ? cJSON_Duplicate(loop, 1) : cJSON_CreateNull());
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
        if (ok && cJSON_IsObject(fill) && strcmp(ursatz_gui_json_string(fill, "kind"), "theme") == 0) {
            char name[64];
            snprintf(name, sizeof(name), "theme-%s.plan.json", ursatz_gui_json_string(section, "id"));
            ok = cJSON_AddItemToArray(files, cJSON_CreateString(name));
        }
    }
    if (ok) {
        ok = ursatz_gui_json_attach(result, "summary", summary);
        summary = NULL;
    }
    if (ok) {
        ok = ursatz_gui_json_attach(result, "brief", brief_summary);
        brief_summary = NULL;
    }
    if (ok) {
        ok = ursatz_gui_json_attach(result, "files", files);
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

static UrsatzGuiThemeResult run_piece_item(const UrsatzGuiComposeRun *run, cJSON **out_summary, char *message,
                                           size_t message_size) {
    return run_piece(run->stage, run->named, run->request, cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(run->member, "check")),
                     run->use_seed, run->seed, NULL, NULL, NULL, run->job, out_summary, message, message_size);
}

static void piece_note(const cJSON *summary, char *note, size_t note_size) {
    const cJSON *reported = reported_seed(summary);
    if (reported) {
        snprintf(note, note_size, "composed seed %.0f", reported->valuedouble);
    } else {
        snprintf(note, note_size, "composed");
    }
}

static const UrsatzGuiComposeKind kPieceKind = {"piece",       "the item is not a piece", composable,
                                                build_request, run_piece_item,            piece_note,
                                                body_with_result};

UrsatzGuiThemeResult ursatz_gui_piece_validate(const cJSON *body, bool *complete, cJSON **out_request, char *message,
                                               size_t message_size) {
    *complete = false;
    *out_request = NULL;
    ursatz_gui_copy_message(message, message_size, "");
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(body, "piece");
    if (!cJSON_IsObject(piece)) {
        ursatz_gui_copy_message(message, message_size, "the body is not a piece");
        return URSATZ_GUI_THEME_OK;
    }
    const cJSON *brief = cJSON_GetObjectItemCaseSensitive(piece, "brief");
    bool has_brief = brief && !cJSON_IsNull(brief);
    if (!composable(piece, has_brief, message, message_size)) {
        return URSATZ_GUI_THEME_OK;
    }
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    if (has_brief && strcmp(ursatz_gui_json_string(structure, "mode"), "style") == 0) {
        /* The style of the brief is built only when composing. */
        ursatz_gui_copy_message(message, message_size, "");
        return URSATZ_GUI_THEME_OK;
    }
    cJSON *built = NULL;
    UrsatzGuiThemeResult result = build_request(body, NULL, NULL, &built, message, message_size);
    if (result == URSATZ_GUI_THEME_OK) {
        result = request_with_plans(built, out_request, message, message_size);
    }
    cJSON_Delete(built);
    *complete = result == URSATZ_GUI_THEME_OK;
    return result;
}

UrsatzGuiThemeResult ursatz_gui_piece_compose(const char *id, bool has_seed, long long seed, bool update_uses,
                                                  const ComposerJob *job, cJSON **out_item, char *message,
                                                  size_t message_size) {
    if (out_item) {
        *out_item = NULL;
    }
    ursatz_gui_copy_message(message, message_size, "");
    cJSON *item = NULL;
    UrsatzGuiThemeResult result = ursatz_gui_compose_item(&kPieceKind, id, has_seed, seed, update_uses, job, &item, message, message_size);
    if (result == URSATZ_GUI_THEME_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    return result;
}

/* --- reopening a composed version ---------------------------------------- */

static cJSON *read_json(const char *path) {
    char *text = ursatz_gui_read_file(path, NULL);
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
    if (strcmp(ursatz_gui_json_string(source->meta, "type"), "piece") != 0) {
        ursatz_gui_copy_message(message, message_size, "the item is not a piece");
        source_clear(source);
        return URSATZ_GUI_THEME_ERR_INVALID;
    }
    read = ursatz_gui_material_item_get_version(id, version, &source->body);
    if (read != URSATZ_GUI_MATERIAL_OK) {
        source_clear(source);
        ursatz_gui_copy_message(message, message_size, "version not found");
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
        ursatz_gui_copy_message(message, message_size, "this version was not composed; compose the piece first");
        source_clear(source);
        return URSATZ_GUI_THEME_ERR_INVALID;
    }
    if (!section_id) {
        return URSATZ_GUI_THEME_OK;
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
        if (strcmp(ursatz_gui_json_string(section, "id"), section_id) == 0) {
            source->section = section;
            break;
        }
    }
    const cJSON *fill = cJSON_GetObjectItemCaseSensitive(source->section, "fill");
    if (!source->section) {
        snprintf(message, message_size, "the piece has no section %s", section_id);
    } else if (strcmp(ursatz_gui_json_string(source->section, "part"), "ending") == 0) {
        snprintf(message, message_size, "section %s is in the ending, which is built from the loop; write a section of the loop again",
                 section_id);
    } else if (lane && !lane_ok(lane)) {
        ursatz_gui_copy_message(message, message_size, "lane is one of harmony, bass, melody, inner, all");
    } else if (cJSON_IsObject(fill) && strcmp(ursatz_gui_json_string(fill, "kind"), "theme") == 0 &&
               (!lane || (strcmp(lane, "harmony") != 0 && strcmp(lane, "bass") != 0))) {
        snprintf(message, message_size, "section %s is filled with a theme: only its harmony or its bass can be written again", section_id);
    } else {
        return URSATZ_GUI_THEME_OK;
    }
    source_clear(source);
    return URSATZ_GUI_THEME_ERR_INVALID;
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
        ursatz_gui_copy_message(message, message_size, "the request of the version could not be read");
        return URSATZ_GUI_THEME_ERR_FAILED;
    }
    UrsatzGuiThemeResult result = URSATZ_GUI_THEME_OK;
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(request, "sections")) {
        cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        if (result == URSATZ_GUI_THEME_OK && cJSON_IsObject(fill) && strcmp(ursatz_gui_json_string(fill, "kind"), "theme") == 0) {
            char name[96];
            snprintf(name, sizeof(name), "theme-%s.plan.json", ursatz_gui_json_string(section, "id"));
            snprintf(path, sizeof(path), "%s/%s", from_dir, name);
            snprintf(copy, sizeof(copy), "%s/%s", dir, name);
            if (!copy_file(path, copy) || !set_string(fill, "plan", copy)) {
                snprintf(message, message_size, "the plan of the theme in section %s could not be copied", ursatz_gui_json_string(section, "id"));
                result = URSATZ_GUI_THEME_ERR_FAILED;
            }
        }
    }
    if (result == URSATZ_GUI_THEME_OK && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(request, "style"))) {
        snprintf(path, sizeof(path), "%s/brief.style.json", from_dir);
        snprintf(copy, sizeof(copy), "%s/brief.style.json", dir);
        if (!copy_file(path, copy) || !set_string(request, "style", copy)) {
            ursatz_gui_copy_message(message, message_size, "the style file of the version could not be copied");
            result = URSATZ_GUI_THEME_ERR_FAILED;
        }
    }
    if (result == URSATZ_GUI_THEME_OK) {
        char *text = cJSON_Print(request);
        snprintf(copy, sizeof(copy), "%s/request.json", dir);
        if (!text || !ursatz_gui_theme_write_text(copy, text)) {
            ursatz_gui_copy_message(message, message_size, "the request could not be written");
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

/* Writes new_body as the next version. */
static UrsatzGuiThemeResult add_version(const char *id, const cJSON *new_body, const char *note, cJSON **out_item,
                                        char *message, size_t message_size) {
    if (ursatz_gui_material_item_add_version(id, new_body, note, out_item) != URSATZ_GUI_MATERIAL_OK) {
        ursatz_gui_copy_message(message, message_size, "the version could not be written");
        return URSATZ_GUI_THEME_ERR_FAILED;
    }
    return URSATZ_GUI_THEME_OK;
}

/* Names the version's own folder in the request file of the stage and in summary instead of the stage's. */
static UrsatzGuiThemeResult rebase_request(const UrsatzGuiComposeStage *stage, cJSON *summary, char *message,
                                           size_t message_size) {
    char path[PATH_BYTES + 96];
    snprintf(path, sizeof(path), "%s/request.json", stage->absolute);
    cJSON *request = read_json(path);
    char *text = NULL;
    if (request) {
        ursatz_gui_compose_rebase(request, stage->absolute, stage->named);
        text = cJSON_Print(request);
    }
    bool ok = text && ursatz_gui_theme_write_text(path, text);
    free(text);
    cJSON_Delete(request);
    ursatz_gui_compose_rebase(summary, stage->absolute, stage->named);
    if (!ok) {
        ursatz_gui_copy_message(message, message_size, "the request could not be written");
        return URSATZ_GUI_THEME_ERR_FAILED;
    }
    return URSATZ_GUI_THEME_OK;
}

/* Reads the request and the loop plan of the source version into *request and *from, or says why not. */
static UrsatzGuiThemeResult read_inputs(const char *request_dir, const char *plan_path, cJSON **request, cJSON **from,
                                        char *message, size_t message_size) {
    char path[PATH_BYTES + 96];
    snprintf(path, sizeof(path), "%s/request.json", request_dir);
    *request = read_json(path);
    *from = read_json(plan_path);
    if (!*request || !*from) {
        ursatz_gui_copy_message(message, message_size, "the request or the plan of the version could not be read");
        cJSON_Delete(*request);
        cJSON_Delete(*from);
        *request = NULL;
        *from = NULL;
        return URSATZ_GUI_THEME_ERR_FAILED;
    }
    return URSATZ_GUI_THEME_OK;
}

static UrsatzGuiThemeResult regenerate_locked(const char *id, int version, const char *section_id, const char *lane,
                                              bool has_seed, long long seed, const ComposerJob *job, cJSON **out_item,
                                              char *message, size_t message_size) {
    Source source;
    UrsatzGuiThemeResult result = open_source(id, version, section_id, lane, &source, message, message_size);
    if (result != URSATZ_GUI_THEME_OK) {
        return result;
    }
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(source.body, "piece");
    const cJSON *brief_json = cJSON_GetObjectItemCaseSensitive(piece, "brief");
    bool has_brief = brief_json && !cJSON_IsNull(brief_json);
    UrsatzGuiComposeStage stage;
    char plan_path[PATH_BYTES + 32];
    cJSON *request = NULL;
    cJSON *from = NULL;
    cJSON *summary = NULL;
    cJSON *new_body = NULL;
    cJSON *item = NULL;
    result = ursatz_gui_compose_stage_make(id, source.current + 1, &stage, message, message_size);
    bool made = result == URSATZ_GUI_THEME_OK;
    if (result == URSATZ_GUI_THEME_OK) {
        result = carry_request(source.dir, stage.absolute, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        snprintf(plan_path, sizeof(plan_path), "%s/plan.json", source.dir);
        result = read_inputs(stage.absolute, plan_path, &request, &from, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        result = run_piece(stage.absolute, stage.named, request, cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(piece, "check")),
                           has_seed, seed, from, section_id, lane, job, &summary, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        result = rebase_request(&stage, summary, message, message_size);
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
            result = add_version(id, new_body, note, &item, message, message_size);
        }
    }
    if (result == URSATZ_GUI_THEME_OK) {
        ursatz_gui_compose_stage_finish(id, item, &stage);
        *out_item = item;
    } else if (made) {
        ursatz_gui_compose_stage_discard(&stage);
    }
    cJSON_Delete(request);
    cJSON_Delete(from);
    cJSON_Delete(summary);
    cJSON_Delete(new_body);
    source_clear(&source);
    return result;
}

UrsatzGuiThemeResult ursatz_gui_piece_regenerate(const char *id, int version, const char *section_id, const char *lane,
                                                 bool has_seed, long long seed, const ComposerJob *job, cJSON **out_item,
                                                 char *message, size_t message_size) {
    if (out_item) {
        *out_item = NULL;
    }
    ursatz_gui_copy_message(message, message_size, "");
    cJSON *item = NULL;
    UrsatzGuiThemeResult result =
        regenerate_locked(id, version, section_id, lane, has_seed, seed, job, &item, message, message_size);
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

/* The row of section_id in the sections of a version of the result (the loop of a game loop, the piece of a form). */
static const cJSON *loop_section(const cJSON *loop, const char *section_id) {
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(loop, "sections")) {
        if (strcmp(ursatz_gui_json_string(section, "id"), section_id) == 0) {
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
        ok = object && ursatz_gui_json_add_copy_or_null(object, entry, "index") && ursatz_gui_json_add_copy_or_null(object, entry, "seed") &&
             ursatz_gui_json_attach(object, "score", copy_or_null(cJSON_GetObjectItemCaseSensitive(loop, "score"))) &&
             ursatz_gui_json_attach(object, "seam", copy_or_null(cJSON_GetObjectItemCaseSensitive(loop, "seam"))) &&
             ursatz_gui_json_attach(object, "heard", copy_or_null(heard)) &&
             cJSON_AddItemToArray(list, object);
        if (!ok) {
            cJSON_Delete(object);
        }
    }
    if (ok) {
        ok = ursatz_gui_json_attach(answer, "variations", list);
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
static UrsatzGuiThemeResult run_variations(const Source *source, const char *name, const cJSON *request, const cJSON *from, const char *section_id,
                                           const char *lane, int count, bool check, bool has_seed, long long seed,
                                           const ComposerJob *job, const char *out_dir, cJSON **out_document, char *message,
                                           size_t message_size) {
    cJSON *with_plans = NULL;
    UrsatzGuiThemeResult result = request_with_plans(request, &with_plans, message, message_size);
    if (result != URSATZ_GUI_THEME_OK) {
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
        return URSATZ_GUI_THEME_ERR_ALLOC;
    }
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    ComposerStatus status = composer_api_piece_variations(text, job, &response);
    free(text);
    cJSON *document = NULL;
    if (status != COMPOSER_OK) {
        result = ursatz_gui_compose_failure(status, &response, message, message_size);
    } else if (!(document = cJSON_Parse(response.json))) {
        ursatz_gui_copy_message(message, message_size, "the variations could not be read");
        result = URSATZ_GUI_THEME_ERR_FAILED;
    }
    double qpm = document ? document_tempo(document) : 0;
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(document, "variations");
    int kept = cJSON_GetArraySize(list);
    for (int i = 0; result == URSATZ_GUI_THEME_OK && i < kept && i < 8; i++) {
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
        result = cJSON_IsObject(plan) && cJSON_IsString(musicxml) ? URSATZ_GUI_THEME_OK : URSATZ_GUI_THEME_ERR_FAILED;
        if (result != URSATZ_GUI_THEME_OK) {
            ursatz_gui_copy_message(message, message_size, "the variations could not be read");
            break;
        }
        result = ursatz_gui_compose_write_plan(plan, paths[0], message, message_size);
        if (result == URSATZ_GUI_THEME_OK) {
            result = ursatz_gui_compose_export(plan, qpm, false, true, false, cJSON_GetObjectItemCaseSensitive(entry, "loop_markers"),
                                               job, &exported, message, message_size);
        }
        if (result == URSATZ_GUI_THEME_OK &&
            (!write_bytes(out_dir, names[1], exported.midi, exported.midi_size, message, message_size) ||
             !write_bytes(out_dir, names[2], musicxml->valuestring, strlen(musicxml->valuestring), message, message_size))) {
            result = URSATZ_GUI_THEME_ERR_FAILED;
        }
        composer_response_destroy(&exported);
        /* The document as printed: the plan of the version is its file, and the files are named. */
        if (result == URSATZ_GUI_THEME_OK) {
            set_string(loop, "plan", paths[0]);
            set_string(entry, "midi", paths[1]);
            set_string(entry, "musicxml", paths[2]);
            cJSON_DeleteItemFromObjectCaseSensitive(entry, "loop_markers");
        }
    }
    if (result == URSATZ_GUI_THEME_OK) {
        cJSON_DeleteItemFromObjectCaseSensitive(document, "brief");
        char *printed = cJSON_PrintUnformatted(document);
        char *file = printed ? malloc(strlen(printed) + 2) : NULL;
        if (file) {
            sprintf(file, "%s\n", printed);
        }
        if (!file || !write_bytes(out_dir, "result.json", file, strlen(file), message, message_size)) {
            result = file ? URSATZ_GUI_THEME_ERR_FAILED : URSATZ_GUI_THEME_ERR_ALLOC;
        }
        free(file);
        free(printed);
    }
    composer_response_destroy(&response);
    if (result != URSATZ_GUI_THEME_OK) {
        cJSON_Delete(document);
        return result;
    }
    *out_document = document;
    return URSATZ_GUI_THEME_OK;
}

static UrsatzGuiThemeResult variations_locked(const char *id, int version, const char *section_id, const char *lane, int count,
                                              bool has_seed, long long seed, const ComposerJob *job, cJSON **out_result,
                                              char *message, size_t message_size) {
    if (!lane || count < 1 || count > 8) {
        ursatz_gui_copy_message(message, message_size, "variations need a lane and a count from 1 to 8");
        return URSATZ_GUI_THEME_ERR_INVALID;
    }
    Source source;
    UrsatzGuiThemeResult result = open_source(id, version, section_id, lane, &source, message, message_size);
    if (result != URSATZ_GUI_THEME_OK) {
        return result;
    }
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(source.body, "piece");
    char out_dir[PATH_BYTES + 32];
    char plan_path[PATH_BYTES + 32];
    cJSON *request = NULL;
    cJSON *from = NULL;
    cJSON *written = NULL;
    snprintf(plan_path, sizeof(plan_path), "%s/plan.json", source.dir);
    result = read_inputs(source.dir, plan_path, &request, &from, message, message_size);
    int set = 0;
    if (result == URSATZ_GUI_THEME_OK) {
        set = make_set_dir(source.dir, id, source.current, out_dir, sizeof(out_dir));
        if (set == 0) {
            ursatz_gui_copy_message(message, message_size, "the folder of the variations could not be made");
            result = URSATZ_GUI_THEME_ERR_FAILED;
        }
    }
    if (result == URSATZ_GUI_THEME_OK) {
        result = run_variations(&source, first_version_name(piece), request, from, section_id, lane, count,
                                cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(piece, "check")), has_seed, seed, job, out_dir,
                                &written, message, message_size);
        if (result != URSATZ_GUI_THEME_OK) {
            ursatz_gui_theme_remove_folder(out_dir);
        }
    }
    if (result == URSATZ_GUI_THEME_OK) {
        *out_result = variations_answer(written, first_version_name(piece), section_id, set);
        if (!*out_result) {
            ursatz_gui_copy_message(message, message_size, "the variations could not be read");
            ursatz_gui_theme_remove_folder(out_dir);
            result = URSATZ_GUI_THEME_ERR_FAILED;
        }
    }
    cJSON_Delete(written);
    cJSON_Delete(request);
    cJSON_Delete(from);
    source_clear(&source);
    return result;
}

UrsatzGuiThemeResult ursatz_gui_piece_variations(const char *id, int version, const char *section_id, const char *lane,
                                                 int count, bool has_seed, long long seed, const ComposerJob *job,
                                                 cJSON **out_result, char *message, size_t message_size) {
    if (out_result) {
        *out_result = NULL;
    }
    ursatz_gui_copy_message(message, message_size, "");
    cJSON *answer = NULL;
    UrsatzGuiThemeResult result =
        variations_locked(id, version, section_id, lane, count, has_seed, seed, job, &answer, message, message_size);
    if (result == URSATZ_GUI_THEME_OK && out_result) {
        *out_result = answer;
    } else {
        cJSON_Delete(answer);
    }
    return result;
}

static UrsatzGuiThemeResult choose_locked(const char *id, int version, int set, int index, const ComposerJob *job,
                                          cJSON **out_item, char *message, size_t message_size) {
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
    UrsatzGuiComposeStage stage;
    cJSON *request = NULL;
    cJSON *from_plan = NULL;
    cJSON *summary = NULL;
    cJSON *new_body = NULL;
    cJSON *item = NULL;
    result = ursatz_gui_compose_stage_make(id, source.current + 1, &stage, message, message_size);
    bool made = result == URSATZ_GUI_THEME_OK;
    if (result == URSATZ_GUI_THEME_OK) {
        result = carry_request(source.dir, stage.absolute, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        result = read_inputs(stage.absolute, from, &request, &from_plan, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        result = run_piece(stage.absolute, stage.named, request, false, false, 0, from_plan, "none", NULL, job, &summary, message,
                           message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        result = rebase_request(&stage, summary, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
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
            ok = ok && ursatz_gui_json_attach(summary, "seed", cJSON_CreateNumber(seed->valuedouble));
        }
        cJSON *brief_summary = ok ? kept_brief_summary(&source) : NULL;
        char note[96];
        snprintf(note, sizeof(note), "chose variation %d seed %.0f", index, ok ? seed->valuedouble : 0.0);
        new_body = brief_summary ? body_with_result(source.body, summary, brief_summary, has_brief) : NULL;
        summary = NULL;
        if (!new_body) {
            ursatz_gui_copy_message(message, message_size, "the version could not be made from the variation");
            result = URSATZ_GUI_THEME_ERR_ALLOC;
        } else {
            result = add_version(id, new_body, note, &item, message, message_size);
        }
    }
    if (result == URSATZ_GUI_THEME_OK) {
        ursatz_gui_compose_stage_finish(id, item, &stage);
        *out_item = item;
    } else if (made) {
        ursatz_gui_compose_stage_discard(&stage);
    }
    cJSON_Delete(written);
    cJSON_Delete(request);
    cJSON_Delete(from_plan);
    cJSON_Delete(summary);
    cJSON_Delete(new_body);
    source_clear(&source);
    return result;
}

UrsatzGuiThemeResult ursatz_gui_piece_choose(const char *id, int version, int set, int index, const ComposerJob *job,
                                             cJSON **out_item, char *message, size_t message_size) {
    if (out_item) {
        *out_item = NULL;
    }
    ursatz_gui_copy_message(message, message_size, "");
    cJSON *item = NULL;
    UrsatzGuiThemeResult result = choose_locked(id, version, set, index, job, &item, message, message_size);
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

/* The checked path of a variations-set file, or NOT_FOUND. type is the file's content type when it is wanted. */
static UrsatzGuiThemeResult variation_path_checked(const char *id, int version, int set, const char *name, char *path,
                                                   size_t path_size, const char **out_type) {
    const char *type = name ? variation_file_type(name) : NULL;
    cJSON *meta = NULL;
    cJSON *body = NULL;
    char dir[PATH_BYTES];
    if (!type || set < 1 || !ursatz_gui_material_version_dir(id, version, dir, sizeof(dir)) ||
        ursatz_gui_material_item_get(id, &meta, &body) != URSATZ_GUI_MATERIAL_OK) {
        return URSATZ_GUI_THEME_ERR_NOT_FOUND;
    }
    bool found = version <= cJSON_GetObjectItemCaseSensitive(meta, "current_version")->valueint &&
                 strcmp(ursatz_gui_json_string(meta, "type"), "piece") == 0;
    cJSON_Delete(meta);
    cJSON_Delete(body);
    if (!found || snprintf(path, path_size, "%s/variations-%d/%s", dir, set, name) >= (int)path_size) {
        return URSATZ_GUI_THEME_ERR_NOT_FOUND;
    }
    *out_type = type;
    return URSATZ_GUI_THEME_OK;
}

UrsatzGuiThemeResult ursatz_gui_piece_variation_midi_path(const char *id, int version, int set, const char *name,
                                                          char *out, size_t out_size) {
    const char *type = NULL;
    if (variation_path_checked(id, version, set, name, out, out_size, &type) != URSATZ_GUI_THEME_OK ||
        strcmp(type, "audio/midi") != 0) {
        return URSATZ_GUI_THEME_ERR_NOT_FOUND;
    }
    FILE *file = fopen(out, "rb");
    if (!file) {
        return URSATZ_GUI_THEME_ERR_NOT_FOUND;
    }
    fclose(file);
    return URSATZ_GUI_THEME_OK;
}

UrsatzGuiThemeResult ursatz_gui_piece_variation_file(const char *id, int version, int set, const char *name,
                                                     unsigned char **out_data, size_t *out_size, const char **out_type) {
    *out_data = NULL;
    *out_size = 0;
    const char *type = NULL;
    char path[PATH_BYTES + 96];
    if (variation_path_checked(id, version, set, name, path, sizeof(path), &type) != URSATZ_GUI_THEME_OK) {
        return URSATZ_GUI_THEME_ERR_NOT_FOUND;
    }
    FILE *file = fopen(path, "rb");
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
