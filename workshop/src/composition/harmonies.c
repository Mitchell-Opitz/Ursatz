#define _XOPEN_SOURCE 700

#include "harmonies.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <common/json.h>
#include <common/status.h>

#include "base/knowledge.h"
#include "base/last_error.h"
#include "compose_common.h"
#include "composer_client.h"
#include "themes.h"

#define PATH_BYTES 800

static const char *const kHarmonyFiles[] = {"plan.json", "harmony.mid", "harmony.musicxml"};

/* A harmony plan is composable when its request names what the progression needs. */
static bool composable(const cJSON *body, UrsatzError *error) {
    const cJSON *request = cJSON_GetObjectItemCaseSensitive(body, "request");
    if (!cJSON_IsObject(request) || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(request, "key")) ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(request, "meter")) || !cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(request, "bars")) ||
        !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(request, "cadence"))) {
        workshop_error_text(error, "the harmony request needs key, meter, bars and cadence");
        return false;
    }
    return true;
}

/* The request of the API: the request as stored with the files asked for and the seed; no suggestions. */
static char *api_request(const cJSON *request, bool use_seed, long long seed) {
    cJSON *copy = cJSON_Duplicate(request, 1);
    char seed_text[32];
    snprintf(seed_text, sizeof(seed_text), "%lld", seed);
    cJSON_DeleteItemFromObjectCaseSensitive(copy, "suggest");
    cJSON_DeleteItemFromObjectCaseSensitive(copy, "midi");
    cJSON_DeleteItemFromObjectCaseSensitive(copy, "musicxml");
    cJSON_DeleteItemFromObjectCaseSensitive(copy, "plans");
    bool ok = copy && (!use_seed || (cJSON_DeleteItemFromObjectCaseSensitive(copy, "seed"), cJSON_AddStringToObject(copy, "seed", seed_text))) &&
              cJSON_AddBoolToObject(copy, "midi", 1) && cJSON_AddBoolToObject(copy, "musicxml", 1) && cJSON_AddBoolToObject(copy, "plans", 1);
    char *text = ok ? cJSON_PrintUnformatted(copy) : NULL;
    cJSON_Delete(copy);
    return text;
}

/* Runs the progression and writes plan.json, harmony.mid and harmony.musicxml into the stage; *out_summary is the Composer's document without its plan. */
static UrsatzStatus run_harmony(const WorkshopComposeRun *run, cJSON **out_summary, UrsatzError *error) {
    char *text = api_request(run->request, run->use_seed, run->seed);
    if (!text) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    UrsatzStatus status = composer_api_progression(workshop_composer_context_global(), text, run->job, &response);
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
        if (!common_path_join(path, sizeof(path), run->stage, kHarmonyFiles[i])) {
            workshop_error_text(error, "a file of the version could not be written");
            result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
            break;
        }
        const void *data = i == 0 ? (const void *)response.plan_json : i == 1 ? (const void *)response.midi : (const void *)response.musicxml;
        size_t size = i == 1 ? response.midi_size : strlen((const char *)data);
        if (common_file_write_atomic(path, data, size, NULL) != URSATZ_OK) {
            workshop_error_text(error, "a file of the version could not be written");
            result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
        }
    }
    if (result == URSATZ_OK) {
        cJSON_DeleteItemFromObjectCaseSensitive(document, "plan");
        if (!workshop_compose_add_files(document, run->named, kHarmonyFiles, 3)) {
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

static cJSON *moved(cJSON *document, const char *name) {
    cJSON *item = cJSON_DetachItemFromObjectCaseSensitive(document, name);
    return item ? item : cJSON_CreateNull();
}

/* The body of the new version: body plus result {seed, harmony, measures, files}. Takes ownership of summary and brief_summary. */
static cJSON *body_with_result(const cJSON *body, cJSON *summary, cJSON *brief_summary, bool has_brief) {
    (void)has_brief;
    cJSON_Delete(brief_summary);
    cJSON *copy = cJSON_Duplicate(body, 1);
    cJSON *result = cJSON_CreateObject();
    cJSON *files = cJSON_CreateArray();
    const cJSON *seed = cJSON_GetObjectItemCaseSensitive(summary, "seed");
    static const char *const names[] = {"request.json", "plan.json", "harmony.mid", "harmony.musicxml"};
    bool ok = copy && result && files && cJSON_AddItemToObject(result, "seed", cJSON_IsNumber(seed) ? cJSON_CreateNumber(seed->valuedouble) : cJSON_CreateNull()) &&
              cJSON_AddItemToObject(result, "harmony", moved(summary, "harmony")) && cJSON_AddItemToObject(result, "measures", moved(summary, "measures"));
    for (size_t i = 0; ok && i < sizeof(names) / sizeof(names[0]); i++) {
        ok = cJSON_AddItemToArray(files, cJSON_CreateString(names[i]));
    }
    ok = ok && cJSON_AddItemToObject(result, "files", files);
    if (ok) {
        files = NULL;
        cJSON_DeleteItemFromObjectCaseSensitive(copy, "result");
        ok = cJSON_AddItemToObject(copy, "result", result);
        result = ok ? NULL : result;
    }
    cJSON_Delete(summary);
    cJSON_Delete(files);
    cJSON_Delete(result);
    if (!ok) {
        cJSON_Delete(copy);
        return NULL;
    }
    return copy;
}

static void harmony_note(const cJSON *summary, char *note, size_t note_size) {
    const cJSON *seed = cJSON_GetObjectItemCaseSensitive(summary, "seed");
    if (cJSON_IsNumber(seed)) {
        snprintf(note, note_size, "composed seed %.0f", seed->valuedouble);
    } else {
        snprintf(note, note_size, "composed");
    }
}

static const WorkshopComposeKind kHarmonyKind = {"harmony", "the item is not a harmony plan", composable, run_harmony, workshop_theme_update_sources,
                                                  workshop_theme_build_brief, harmony_note, body_with_result};

UrsatzStatus workshop_harmony_compose(const char *id, bool has_seed, long long seed, bool update_uses, const UrsatzJob *job, cJSON **out_item,
                                      UrsatzError *error) {
    if (out_item) {
        *out_item = NULL;
    }
    workshop_error_text(error, "");
    cJSON *item = NULL;
    UrsatzStatus result = workshop_compose_item(&kHarmonyKind, id, has_seed, seed, update_uses, NULL, job, &item, error);
    if (result == URSATZ_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    return result;
}
