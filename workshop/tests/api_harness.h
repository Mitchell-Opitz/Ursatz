#ifndef WORKSHOP_TESTS_API_HARNESS_H
#define WORKSHOP_TESTS_API_HARNESS_H

/*
 * What the tests of the query and command API share: a workshop opened and started on the test's own working directory (cleared first), a
 * call of a command as a person would give it, a wait for the job a command answers with, and a read of the journal.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cJSON.h>

#include <ursatz_workshop/workshop_api.h>

#include "items/journal.h"
#include "items/layout.h"
#include "ursatz_check.h"

static Workshop *g_workshop = NULL;

static const cJSON *member(const cJSON *object, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

static const char *str(const cJSON *object, const char *name) {
    const cJSON *item = member(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static double num(const cJSON *object, const char *name) {
    const cJSON *item = member(object, name);
    return cJSON_IsNumber(item) ? item->valuedouble : -1;
}

/* Opens and starts a workshop on a cleared working directory (the test's own folder under the build tree). */
static bool harness_open(int port) {
    char cwd[512];
    if (!getcwd(cwd, sizeof(cwd)) || strstr(cwd, "/workshop/tests/") == NULL) {
        fprintf(stderr, "the harness clears its working directory and runs only in the build tree's workshop/tests folder\n");
        return false;
    }
    if (system("rm -rf ./* ./.[!.]*") != 0) {
        return false;
    }
    WorkshopConfig flags = {.data_dir = ".", .config_path = NULL, .port = port, .http_threads = 4, .job_workers = 2};
    WorkshopConfig resolved;
    char storage[4096];
    UrsatzError error = {{0}, {0}};
    if (workshop_settings_resolve(&flags, &resolved, storage, sizeof(storage), &error) != URSATZ_OK ||
        workshop_open(&resolved, &g_workshop, &error) != URSATZ_OK || workshop_start(g_workshop, &error) != URSATZ_OK) {
        fprintf(stderr, "cannot open the workshop: %s\n", error.message);
        return false;
    }
    return true;
}

static void harness_close(void) {
    workshop_close(g_workshop);
    g_workshop = NULL;
}

/* One command as the API takes it. params is JSON text (NULL for none); base is the version read (negative for none). */
static UrsatzStatus command_as(const char *actor, const char *name, const char *target, const char *params, long base, const WorkshopBytes *bytes,
                               WorkshopReply *reply, UrsatzError *error) {
    cJSON *command = cJSON_CreateObject();
    cJSON_AddStringToObject(command, "command", name);
    if (actor) {
        cJSON_AddStringToObject(command, "actor", actor);
    }
    if (target) {
        cJSON_AddStringToObject(command, "target", target);
    }
    if (base >= 0) {
        cJSON_AddNumberToObject(command, "base_version", (double)base);
    }
    if (params) {
        cJSON_AddItemToObject(command, "params", cJSON_Parse(params));
    }
    memset(error, 0, sizeof(*error));
    UrsatzStatus status = workshop_command(g_workshop, command, bytes, reply, error);
    cJSON_Delete(command);
    return status;
}

static UrsatzStatus command(const char *name, const char *target, const char *params, long base, WorkshopReply *reply, UrsatzError *error) {
    return command_as("test", name, target, params, base, NULL, reply, error);
}

static UrsatzStatus query(const char *name, const char *params_text, WorkshopReply *reply, UrsatzError *error) {
    cJSON *params = params_text ? cJSON_Parse(params_text) : NULL;
    memset(error, 0, sizeof(*error));
    UrsatzStatus status = workshop_query(g_workshop, name, params, NULL, reply, error);
    cJSON_Delete(params);
    return status;
}

/* Waits for the job a reply names (a command that queued one) and returns the job as the queue keeps it: {id, kind, state, ..., http_status,
 * result}; the caller frees it. NULL when the reply has no job or it does not finish. */
static cJSON *finish_job(const WorkshopReply *reply) {
    const char *id = str(member(reply->json, "job"), "id");
    if (id[0] == '\0') {
        return NULL;
    }
    char params[96];
    snprintf(params, sizeof(params), "{\"id\":\"%s\"}", id);
    for (int i = 0; i < 12000; i++) {
        WorkshopReply polled;
        UrsatzError error;
        if (query("job.get", params, &polled, &error) != URSATZ_OK) {
            workshop_reply_destroy(&polled);
            return NULL;
        }
        const char *state = str(member(polled.json, "job"), "state");
        if (strcmp(state, "queued") != 0 && strcmp(state, "running") != 0) {
            cJSON *job = cJSON_DetachItemFromObjectCaseSensitive(polled.json, "job");
            workshop_reply_destroy(&polled);
            return job;
        }
        workshop_reply_destroy(&polled);
        usleep(20 * 1000);
    }
    return NULL;
}

/* The journal records, in order (the caller frees the list), NULL when it cannot be read. */
static cJSON *journal_records(void) {
    WorkshopJournal journal;
    UrsatzError error = {{0}, {0}};
    if (workshop_journal_read(WORKSHOP_JOURNAL_FILE, &journal, &error) != URSATZ_OK) {
        return NULL;
    }
    cJSON *records = cJSON_CreateArray();
    for (size_t i = 0; i < journal.count; i++) {
        cJSON_AddItemToArray(records, cJSON_Duplicate(journal.records[i], true));
    }
    workshop_journal_free(&journal);
    return records;
}

/* The last record of the journal (a copy), NULL when it has none. */
static cJSON *last_record(void) {
    cJSON *records = journal_records();
    cJSON *last = records && cJSON_GetArraySize(records) > 0 ? cJSON_Duplicate(cJSON_GetArrayItem(records, cJSON_GetArraySize(records) - 1), true) : NULL;
    cJSON_Delete(records);
    return last;
}

#endif
