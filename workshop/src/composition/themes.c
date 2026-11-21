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
#include "request_resolve.h"
#include "items/store.h"
#include "analysis/styles.h"

#define PATH_BYTES 800

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

/* The request is the body's own (R-650): the resolver replaces its references (composition/request_resolve.c). A theme is
   composable when its request names what the Composer cannot do without: the way it is made (into), its material and its meter. */
static bool composable(const cJSON *body, UrsatzError *error) {
    const cJSON *request = cJSON_GetObjectItemCaseSensitive(body, "request");
    const cJSON *material = cJSON_GetObjectItemCaseSensitive(request, "material");
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(request, "mode");
    if (!cJSON_IsObject(request) || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(request, "into")) ||
        (!cJSON_IsString(material) && !cJSON_IsString(mode)) || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(request, "meter"))) {
        workshop_error_text(error, "the theme request needs into, material and meter");
        return false;
    }
    return true;
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
              cJSON_AddBoolToObject(copy, "plans", 0) && cJSON_AddBoolToObject(copy, "layers", 1);
    char *text = ok ? cJSON_PrintUnformatted(copy) : NULL;
    cJSON_Delete(copy);
    return text;
}

/* Grows the theme with the Composer API and writes plan.json, theme.mid and theme.musicxml into the stage. *out_summary
   is the document `theme --format json` printed: no plans and explanations, and the files written. */
static UrsatzStatus run_theme(const WorkshopComposeRun *run, cJSON **out_summary, UrsatzError *error) {
    char *text = api_request(run->request, cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(run->body, "check")),
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
        result = workshop_compose_write_layers(run->stage, document, error);
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

/* The body of the new version: body plus result. Takes ownership of summary and brief_summary. */
static cJSON *body_with_result(const cJSON *body, cJSON *summary, cJSON *brief_summary, bool has_brief) {
    cJSON *copy = cJSON_Duplicate(body, 1);
    cJSON *result = cJSON_CreateObject();
    cJSON *files = cJSON_CreateArray();
    const cJSON *seed = reported_seed(summary);
    static const char *const names[] = {"request.json", "brief.style.json", "plan.json", "theme.mid", "theme.musicxml"};
    bool ok = copy && result && files &&
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
        cJSON_DeleteItemFromObjectCaseSensitive(copy, "result");
        ok = cJSON_AddItemToObject(copy, "result", result);
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

static void theme_note(const cJSON *summary, char *note, size_t note_size) {
    (void)summary;
    snprintf(note, note_size, "Composed");
}

static const WorkshopComposeKind kThemeKind = {"theme",       "the item is not a theme", composable, run_theme,
                                                workshop_theme_update_sources, workshop_theme_build_brief, theme_note,
                                                body_with_result};

UrsatzStatus workshop_theme_validate(const cJSON *body, bool *complete, cJSON **out_request, UrsatzError *error) {
    *complete = false;
    *out_request = NULL;
    workshop_error_text(error, "");
    if (!cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(body, "request"))) {
        workshop_error_text(error, "the body is not a theme");
        return URSATZ_OK;
    }
    if (!composable(body, error)) {
        return URSATZ_OK;
    }
    const cJSON *brief = cJSON_GetObjectItemCaseSensitive(body, "brief");
    const cJSON *request = cJSON_GetObjectItemCaseSensitive(body, "request");
    const cJSON *material = cJSON_GetObjectItemCaseSensitive(request, "material");
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(request, "mode");
    if (brief && !cJSON_IsNull(brief) && strcmp(cJSON_IsString(material) ? material->valuestring : cJSON_IsString(mode) ? mode->valuestring : "", "style") == 0) {
        /* The style of the brief is built only when composing. */
        workshop_error_text(error, "");
        return URSATZ_OK;
    }
    UrsatzStatus result = workshop_request_resolve("theme", body, NULL, NULL, out_request, error);
    *complete = result == URSATZ_OK;
    return result;
}

UrsatzStatus workshop_theme_compose(const char *id, bool has_seed, long long seed, bool update_uses, const WorkshopComposeSettings *settings,
                                              const UrsatzJob *job, cJSON **out_item, UrsatzError *error) {
    if (out_item) {
        *out_item = NULL;
    }
    workshop_error_text(error, "");
    cJSON *item = NULL;
    UrsatzStatus result = workshop_compose_item(&kThemeKind, id, has_seed, seed, update_uses, settings, job, &item, error);
    if (result == URSATZ_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    return result;
}
