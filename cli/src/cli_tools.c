#include "cli_tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The process's one workshop: a command line process has no caller to hold it. */
static Workshop *g_workshop = NULL;
static bool g_owned = false; /* the tools workshop is ours to close; a session's is not */
static bool g_opened = false;

static void release(void) {
    if (g_owned) {
        workshop_close(g_workshop);
    }
    g_workshop = NULL;
}

void cli_tools_use(Workshop *workshop) {
    if (g_owned) {
        workshop_close(g_workshop); /* the process holds one workshop at a time */
        g_owned = false;
    }
    g_workshop = workshop;
    g_opened = workshop != NULL;
}

Workshop *cli_tools(void) {
    if (!g_opened) {
        UrsatzError error;
        memset(&error, 0, sizeof error);
        g_opened = true;
        if (workshop_tools_open(&g_workshop, &error) != URSATZ_OK) {
            fprintf(stderr, "Error: cannot read the knowledge: %s\n", error.message);
            g_workshop = NULL;
        } else {
            g_owned = true;
            atexit(release);
        }
    }
    return g_workshop;
}

void cli_response_destroy(CliResponse *response) {
    if (!response) {
        return;
    }
    free(response->json);
    free(response->plan_json);
    free(response->midi);
    free(response->musicxml);
    memset(response, 0, sizeof *response);
}

/* A copy of the string member of the reply's json, NULL when it is null or absent. */
static char *copy_text(const cJSON *json, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(json, name);
    return cJSON_IsString(item) ? strdup(item->valuestring) : NULL;
}

static UrsatzStatus call(const char *entry, const char *kind, const cJSON *request, const WorkshopBytes *bytes, CliResponse *out) {
    memset(out, 0, sizeof *out);
    Workshop *workshop = cli_tools();
    if (!workshop) {
        snprintf(out->error.message, sizeof out->error.message, "a knowledge snapshot is required");
        return URSATZ_ERR_REQUEST;
    }
    cJSON *params = cJSON_CreateObject();
    bool ok = params && cJSON_AddStringToObject(params, "entry", entry) &&
              (!kind || cJSON_AddStringToObject(params, "kind", kind)) &&
              (!request || cJSON_AddItemReferenceToObject(params, "request", (cJSON *)request));
    if (!ok) {
        cJSON_Delete(params);
        snprintf(out->error.message, sizeof out->error.message, "out of memory");
        return URSATZ_ERR_ALLOC;
    }
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus status = workshop_tool(workshop, "composer", params, bytes, &reply, &error);
    cJSON_Delete(params);
    if (status != URSATZ_OK) {
        out->error = error;
    }
    /* A refusal may carry an answer too (what a plan is stuck on). */
    out->json = copy_text(reply.json, "json");
    out->plan_json = copy_text(reply.json, "plan_json");
    out->musicxml = copy_text(reply.json, "musicxml");
    out->midi = reply.bytes;
    out->midi_size = reply.size;
    reply.bytes = NULL;
    reply.size = 0;
    workshop_reply_destroy(&reply);
    return status;
}

UrsatzStatus cli_composer(const char *entry, const cJSON *request, const WorkshopBytes *bytes, CliResponse *out) {
    return call(entry, NULL, request, bytes, out);
}

UrsatzStatus cli_composer_validate(const char *kind, const cJSON *request, CliResponse *out) {
    return call("validate", kind, request, NULL, out);
}

#define PITCH_NAMES_CHUNK 4096 /* what one pitch-names call takes */

bool cli_pitch_names(const int *midi, size_t count, char (*names)[8]) {
    for (size_t i = 0; i < count; i++) {
        snprintf(names[i], 8, "?");
    }
    Workshop *workshop = cli_tools();
    for (size_t start = 0; workshop && start < count; start += PITCH_NAMES_CHUNK) {
        size_t chunk = count - start < PITCH_NAMES_CHUNK ? count - start : PITCH_NAMES_CHUNK;
        cJSON *params = cJSON_CreateObject();
        cJSON *numbers = params ? cJSON_AddArrayToObject(params, "midi") : NULL;
        for (size_t i = 0; numbers && i < chunk; i++) {
            cJSON *number = cJSON_CreateNumber(midi[start + i]);
            if (number) {
                cJSON_AddItemToArray(numbers, number);
            }
        }
        WorkshopReply reply;
        UrsatzError error;
        memset(&error, 0, sizeof error);
        if (!numbers || cJSON_GetArraySize(numbers) != (int)chunk ||
            workshop_tool(workshop, "pitch-names", params, NULL, &reply, &error) != URSATZ_OK) {
            cJSON_Delete(params);
            return false;
        }
        cJSON_Delete(params);
        const cJSON *answered = cJSON_GetObjectItemCaseSensitive(reply.json, "names");
        for (size_t i = 0; i < chunk; i++) {
            const cJSON *name = cJSON_GetArrayItem(answered, (int)i);
            if (cJSON_IsString(name)) {
                snprintf(names[start + i], 8, "%s", name->valuestring);
            }
        }
        workshop_reply_destroy(&reply);
    }
    return workshop != NULL;
}

bool cli_fraction(const cJSON *object, const char *name, long long *num, long long *den) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    const cJSON *top = cJSON_GetObjectItemCaseSensitive(item, "num");
    const cJSON *bottom = cJSON_GetObjectItemCaseSensitive(item, "den");
    *num = cJSON_IsNumber(top) ? (long long)top->valuedouble : 0;
    *den = cJSON_IsNumber(bottom) ? (long long)bottom->valuedouble : 0;
    if (*den == 0) {
        *den = 1;
    }
    return item != NULL;
}
