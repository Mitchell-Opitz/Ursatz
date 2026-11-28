#include "knowledge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "cli_options.h"
#include "cli_params.h"
#include "cli_tools.h"
#include "session.h"

/* Calls the knowledge query `action` ("find", "show" or "check") on the data folder, or the tool of the same name on a tools workshop when there is no folder. */
static UrsatzStatus ask(const char *action, const cJSON *params, WorkshopReply *reply, UrsatzError *error, int *opened) {
    const CliGlobal *global = cli_global();
    bool has_folder = (global->data && global->data[0]) || (getenv("URSATZ_DATA") && getenv("URSATZ_DATA")[0]);
    char name[32];
    *opened = 0;
    if (!has_folder) {
        snprintf(name, sizeof name, "knowledge-%s", action);
        Workshop *workshop = cli_tools();
        return workshop != NULL ? workshop_tool(workshop, name, params, NULL, reply, error)
                                : ursatz_error_set(error, URSATZ_ERR_INTERNAL, "", "the knowledge cannot be read");
    }
    CliSession *session = NULL;
    *opened = cli_session_open(global, &session);
    if (*opened != 0) {
        return URSATZ_ERR_INTERNAL;
    }
    snprintf(name, sizeof name, "knowledge.%s", action);
    UrsatzStatus status = cli_session_query(session, name, params, reply, error);
    cli_session_close(session);
    return status;
}

static int print_find(const cJSON *reply) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(reply, "entries")) {
        const cJSON *subtype = cJSON_GetObjectItemCaseSensitive(entry, "subtype");
        char kind[160];
        snprintf(kind, sizeof kind, "%s%s%s", cJSON_GetObjectItemCaseSensitive(entry, "kind")->valuestring, cJSON_IsString(subtype) ? "/" : "",
                 cJSON_IsString(subtype) ? subtype->valuestring : "");
        printf("%s  %s  %s\n", cJSON_GetObjectItemCaseSensitive(entry, "id")->valuestring, kind, cJSON_GetObjectItemCaseSensitive(entry, "name")->valuestring);
    }
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(reply, "more"))) {
        printf("more entries match; name more words\n");
    }
    return 0;
}

int run_knowledge(const CliArgs *args) {
    if (args->input_count < 2) {
        return cli_needs("knowledge", "an action (find, show or check) and what it takes", true);
    }
    const char *action = args->inputs[0];
    const char *subject = args->inputs[1];
    const char *kind = cli_value(args, "--kind");
    const char *format = cli_value(args, "--format");
    if (strcmp(action, "find") != 0 && strcmp(action, "show") != 0 && strcmp(action, "check") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "unknown action '%s'; use find, show or check", action);
    }
    if (kind != NULL && strcmp(action, "find") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "--kind goes with find");
    }
    bool json = false;
    if (format != NULL) {
        json = cli_equals_ignore_case(format, "json");
        if (!json && !cli_equals_ignore_case(format, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", format);
        }
        if (strcmp(action, "show") != 0) {
            return cli_fail(CLI_EXIT_USAGE, "--format goes with show");
        }
    }
    cJSON *params = cJSON_CreateObject();
    if (params == NULL) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    bool built = true;
    if (strcmp(action, "find") == 0) {
        built = cJSON_AddStringToObject(params, "words", subject) && (kind == NULL || cJSON_AddStringToObject(params, "kind", kind));
    } else if (strcmp(action, "show") == 0) {
        built = cJSON_AddStringToObject(params, "id", subject) && cJSON_AddStringToObject(params, "view", json ? "json" : "text");
    } else {
        char *path = cli_path_absolute(subject);
        char *text = NULL;
        size_t size = 0;
        bool read = path != NULL && cli_read_file(path, &text, &size);
        free(path);
        if (!read) {
            cJSON_Delete(params);
            return cli_fail(CLI_EXIT_USAGE, "cannot read '%s'", subject);
        }
        cJSON *document = cJSON_Parse(text);
        free(text);
        if (!cJSON_IsObject(document)) {
            cJSON_Delete(document);
            cJSON_Delete(params);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "%s: not a knowledge document (a JSON object)", subject);
        }
        built = cJSON_AddItemToObject(params, "file", document);
        if (!built) {
            cJSON_Delete(document);
        }
    }
    if (!built) {
        cJSON_Delete(params);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    int opened = 0;
    UrsatzStatus status = ask(action, params, &reply, &error, &opened);
    cJSON_Delete(params);
    if (opened != 0) {
        return opened;
    }
    if (status != URSATZ_OK) {
        workshop_reply_destroy(&reply);
        if (strcmp(action, "check") == 0) {
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", error.message);
        }
        return cli_fail(status == URSATZ_ERR_REQUEST || status == URSATZ_ERR_NOT_FOUND ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE, "%s", error.message);
    }
    int result = 0;
    if (strcmp(action, "find") == 0) {
        result = print_find(reply.json);
    } else if (strcmp(action, "show") == 0 && json) {
        cli_print_json(cJSON_GetObjectItemCaseSensitive(reply.json, "entry"));
    } else if (strcmp(action, "show") == 0) {
        printf("%s\n", cJSON_GetObjectItemCaseSensitive(reply.json, "text")->valuestring);
    } else {
        printf("ok: %d entries\n", (int)cJSON_GetObjectItemCaseSensitive(reply.json, "entries")->valuedouble);
    }
    workshop_reply_destroy(&reply);
    return result;
}
