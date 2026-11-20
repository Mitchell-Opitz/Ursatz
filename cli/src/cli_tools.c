#include "cli_tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The process's one workshop: a command line process has no caller to hold it. */
static Workshop *g_workshop = NULL;
static bool g_opened = false;

static void release(void) {
    workshop_close(g_workshop);
    g_workshop = NULL;
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
