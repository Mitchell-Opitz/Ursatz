#include "session.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cli_options.h"
#include "cli_tools.h"
#include "remote.h"
#include <common/files.h>

struct CliSession {
    bool local;
    Workshop *workshop; /* a local session's */
    int port;           /* a remote session's */
    char data[PATH_MAX];
    char actor[64];
};

static CliGlobal g_global = {NULL, 0, NULL};
static char g_start_directory[PATH_MAX] = "";

const CliGlobal *cli_global(void) {
    return &g_global;
}

void cli_global_set(const CliGlobal *global) {
    g_global = *global;
    if (g_start_directory[0] == '\0' && !getcwd(g_start_directory, sizeof g_start_directory)) {
        g_start_directory[0] = '\0';
    }
}

char *cli_path_absolute(const char *path) {
    if (!path) {
        return NULL;
    }
    if (path[0] == '/' || path[0] == '\0') {
        return strdup(path);
    }
    char here[PATH_MAX];
    const char *base = g_start_directory[0] ? g_start_directory : (getcwd(here, sizeof here) ? here : "");
    return common_path_join_alloc(base, path);
}

/* The data folder as an absolute path: realpath of an existing folder, else the starting directory plus the name. */
static bool data_folder(const char *given, char *out, size_t size) {
    char resolved[PATH_MAX];
    if (realpath(given, resolved)) {
        return snprintf(out, size, "%s", resolved) < (int)size;
    }
    char *absolute = cli_path_absolute(given);
    bool ok = absolute && snprintf(out, size, "%s", absolute) < (int)size;
    free(absolute);
    return ok;
}

int cli_data_folder(const CliGlobal *global, char *out, size_t size) {
    const char *given = global->data && global->data[0] ? global->data : getenv("URSATZ_DATA");
    if (!given || !given[0]) {
        return cli_fail(CLI_EXIT_USAGE, "name the data folder with --data <dir> or URSATZ_DATA");
    }
    if (!data_folder(given, out, size)) {
        return cli_fail(CLI_EXIT_USAGE, "the data folder path is too long");
    }
    return 0;
}

int cli_session_open(const CliGlobal *global, CliSession **out) {
    *out = NULL;
    CliSession *session = calloc(1, sizeof *session);
    if (!session) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    snprintf(session->actor, sizeof session->actor, "%s", global->actor && global->actor[0] ? global->actor : "claude");
    int named = cli_data_folder(global, session->data, sizeof session->data);
    if (named != 0) {
        free(session);
        return named;
    }

    WorkshopConfig flags = {session->data, NULL, global->port, 0, 0};
    WorkshopConfig resolved;
    char storage[2048];
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus status = workshop_config_resolve(&flags, &resolved, storage, sizeof storage, &error);
    if (status == URSATZ_OK) {
        status = workshop_open(&resolved, &session->workshop, &error);
    }
    if (status == URSATZ_OK) {
        status = workshop_start(session->workshop, &error);
        if (status != URSATZ_OK) {
            workshop_close(session->workshop);
        }
    }
    if (status == URSATZ_OK) {
        session->local = true;
        cli_tools_use(session->workshop);
        *out = session;
        return 0;
    }

    if (status == URSATZ_ERR_CONFLICT) {
        /* Another process holds the folder: connect to it when it serves HTTP. */
        long pid = 0;
        int port = 0;
        UrsatzError holder_error;
        memset(&holder_error, 0, sizeof holder_error);
        if (workshop_lock_holder(session->data, &pid, &port, &holder_error) == URSATZ_OK) {
            if (port == 0) {
                free(session);
                return cli_fail(CLI_EXIT_UNSATISFIABLE, "%s; it takes no connections, so run again when it ends", error.message);
            }
            WorkshopReply reply;
            UrsatzError remote_error;
            memset(&remote_error, 0, sizeof remote_error);
            if (cli_remote_request(port, "GET", "/api/workshop/status", NULL, "", NULL, 0, &reply, &remote_error) != URSATZ_OK) {
                free(session);
                return cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", remote_error.message);
            }
            const cJSON *served = cJSON_GetObjectItemCaseSensitive(reply.json, "data_dir");
            bool same = cJSON_IsString(served) && strcmp(served->valuestring, session->data) == 0;
            int shown = same ? 0 : cli_fail(CLI_EXIT_UNSATISFIABLE, "port %d serves %s, not %s", port, cJSON_IsString(served) ? served->valuestring : "?", session->data);
            workshop_reply_destroy(&reply);
            if (!same) {
                free(session);
                return shown;
            }
            session->port = port;
            *out = session;
            return 0;
        }
    }
    free(session);
    return cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", error.message);
}

bool cli_session_is_local(const CliSession *session) {
    return session->local;
}

const char *cli_session_data_dir(const CliSession *session) {
    return session->workshop ? workshop_data_dir(session->workshop) : session->data;
}

Workshop *cli_session_workshop(const CliSession *session) {
    return session->local ? session->workshop : NULL;
}

int cli_session_port(const CliSession *session) {
    return session->port;
}

UrsatzStatus cli_session_query(CliSession *session, const char *name, const cJSON *params, WorkshopReply *reply, UrsatzError *error) {
    if (session->local) {
        return workshop_query(session->workshop, name, params, NULL, reply, error);
    }
    char *encoded = cli_percent_encode(name);
    char *text = params ? cJSON_PrintUnformatted(params) : strdup("{}");
    char path[512];
    snprintf(path, sizeof path, "/api/workshop/query/%s", encoded ? encoded : "");
    UrsatzStatus status = encoded && text ? cli_remote_request(session->port, "POST", path, "application/json", "", (const uint8_t *)text, strlen(text), reply, error)
                                          : ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    free(encoded);
    cJSON_free(text);
    return status;
}

UrsatzStatus cli_session_command(CliSession *session, cJSON *command, const WorkshopBytes *bytes, WorkshopReply *reply, UrsatzError *error) {
    cJSON_DeleteItemFromObjectCaseSensitive(command, "actor");
    if (!cJSON_AddStringToObject(command, "actor", session->actor)) {
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    if (session->local) {
        return workshop_command(session->workshop, command, bytes, reply, error);
    }
    char *text = cJSON_PrintUnformatted(command);
    if (!text) {
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    UrsatzStatus status;
    if (bytes && bytes->data) {
        char *command_header = cli_percent_encode(text);
        char *name_header = cli_percent_encode(bytes->name ? bytes->name : "");
        char headers[8192];
        snprintf(headers, sizeof headers, "X-Ursatz-Command: %s\r\nX-Filename: %s\r\n", command_header ? command_header : "", name_header ? name_header : "");
        status = command_header && name_header && strlen(command_header) < sizeof headers - 128
                     ? cli_remote_request(session->port, "POST", "/api/workshop/command", "application/octet-stream", headers, bytes->data, bytes->size, reply, error)
                     : ursatz_error_set(error, URSATZ_ERR_REQUEST, "", "the command is too long to send with a file");
        free(command_header);
        free(name_header);
    } else {
        status = cli_remote_request(session->port, "POST", "/api/workshop/command", "application/json", "", (const uint8_t *)text, strlen(text), reply, error);
    }
    cJSON_free(text);
    return status;
}

UrsatzStatus cli_session_wait_job(CliSession *session, const char *job_id, cJSON **job, UrsatzError *error) {
    *job = NULL;
    for (;;) {
        cJSON *params = cJSON_CreateObject();
        WorkshopReply reply;
        if (!params || !cJSON_AddStringToObject(params, "id", job_id)) {
            cJSON_Delete(params);
            return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
        UrsatzStatus status = cli_session_query(session, "job.get", params, &reply, error);
        cJSON_Delete(params);
        if (status != URSATZ_OK) {
            workshop_reply_destroy(&reply);
            return status;
        }
        cJSON *found = cJSON_DetachItemFromObjectCaseSensitive(reply.json, "job");
        workshop_reply_destroy(&reply);
        const cJSON *state = cJSON_GetObjectItemCaseSensitive(found, "state");
        if (cJSON_IsString(state) && (strcmp(state->valuestring, "done") == 0 || strcmp(state->valuestring, "failed") == 0 ||
                                      strcmp(state->valuestring, "cancelled") == 0)) {
            *job = found;
            return URSATZ_OK;
        }
        cJSON_Delete(found);
        struct timespec pause = {0, 200 * 1000 * 1000};
        nanosleep(&pause, NULL);
    }
}

void cli_session_close(CliSession *session) {
    if (!session) {
        return;
    }
    if (session->local) {
        cli_tools_use(NULL);
        workshop_close(session->workshop);
    }
    free(session);
}
