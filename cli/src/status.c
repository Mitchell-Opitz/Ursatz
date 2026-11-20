#include "status.h"

#include <stdio.h>
#include <string.h>

#include <cJSON.h>

#include "cli_options.h"
#include "cli_tools.h"
#include "remote.h"
#include "session.h"
#include "workflow.h"

/* The workshop's status members of the session: a new object (the caller deletes it). */
static cJSON *read_status(CliSession *session, UrsatzError *error) {
    cJSON *object = NULL;
    if (cli_session_is_local(session)) {
        if (workshop_status(cli_session_workshop(session), &object, error) != URSATZ_OK) {
            return NULL;
        }
        cJSON_AddStringToObject(object, "recovery", workshop_recovery_summary(cli_session_workshop(session)));
        return object;
    }
    WorkshopReply reply;
    if (cli_remote_request_status(cli_session_port(session), &reply, error) != URSATZ_OK) {
        workshop_reply_destroy(&reply);
        return NULL;
    }
    object = reply.json;
    reply.json = NULL;
    workshop_reply_destroy(&reply);
    return object;
}

int run_status(const CliArgs *args) {
    const char *format = cli_value(args, "--format");
    bool json = false;
    if (format) {
        json = cli_equals_ignore_case(format, "json");
        if (!json && !cli_equals_ignore_case(format, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", format);
        }
    }
    CliSession *session = NULL;
    int opened = cli_session_open(cli_global(), &session);
    if (opened != 0) {
        return opened;
    }
    UrsatzError error;
    memset(&error, 0, sizeof error);
    cJSON *status = read_status(session, &error);
    if (!status) {
        cli_session_close(session);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", error.message);
    }
    const char *recovery = cli_json_string(status, "recovery");
    if (json) {
        cJSON *object = cJSON_CreateObject();
        cJSON_AddStringToObject(object, "data_dir", cli_session_data_dir(session));
        cJSON_AddStringToObject(object, "mode", cli_session_is_local(session) ? "local" : "remote");
        if (cli_session_is_local(session)) {
            cJSON_AddNumberToObject(object, "job_workers", workshop_job_workers(cli_session_workshop(session)));
        } else {
            cJSON_AddNumberToObject(object, "pid", cli_json_number(status, "pid"));
            cJSON_AddNumberToObject(object, "port", cli_session_port(session));
        }
        cJSON_AddStringToObject(object, "composer_version", cli_json_string(status, "composer_version"));
        cJSON_AddStringToObject(object, "analyzer_version", cli_json_string(status, "analyzer_version"));
        cJSON_AddStringToObject(object, "recovery", recovery);
        cli_print_json(object);
        cJSON_Delete(object);
    } else {
        printf("data folder %s\n", cli_session_data_dir(session));
        if (cli_session_is_local(session)) {
            printf("opened by this command (job workers %d)\n", workshop_job_workers(cli_session_workshop(session)));
        } else {
            printf("served by the Ursatz process %.0f on port %d\n", cli_json_number(status, "pid"), cli_session_port(session));
        }
        printf("composer %s\n", cli_json_string(status, "composer_version"));
        printf("analyzer %s\n", cli_json_string(status, "analyzer_version"));
        if (recovery[0] != '\0') {
            printf("%s\n", recovery);
        }
    }
    cJSON_Delete(status);
    cli_session_close(session);
    return 0;
}
