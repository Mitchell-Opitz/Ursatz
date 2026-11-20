#include "show.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "address.h"
#include "cli_options.h"
#include "cli_tools.h"
#include "session.h"
#include "workflow.h"

/* Prints the failure of a workshop call as "Error: <message>[ at <path>]"; returns the exit status (1 for a request that was refused, 2 otherwise). */
static int fail_call(UrsatzStatus status, const UrsatzError *error) {
    return cli_fail(status == URSATZ_ERR_REQUEST ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE, "%s%s%s", error->message, error->path[0] ? " at " : "",
                    error->path);
}

/* The query that reads what the address names, with its params (a new object); NULL on allocation failure. */
static const char *query_of(const CliAddress *address, cJSON **params) {
    *params = cJSON_CreateObject();
    bool ok = *params && cJSON_AddStringToObject(*params, "id", address->id);
    if (ok && address->version > 0) {
        ok = cJSON_AddNumberToObject(*params, "version", (double)address->version) != NULL;
    }
    if (!ok) {
        cJSON_Delete(*params);
        *params = NULL;
        return NULL;
    }
    switch (address->kind) {
    case CLI_ADDRESS_ITEM:
        return address->version > 0 ? "item.version" : "item.get";
    case CLI_ADDRESS_PIECE:
        return "library.piece";
    case CLI_ADDRESS_PROJECT:
        return "project.get";
    case CLI_ADDRESS_SUBJECT:
        return "subject.get";
    case CLI_ADDRESS_COLLECTION:
        return "collection.get";
    case CLI_ADDRESS_STYLE:
        return "style.get";
    case CLI_ADDRESS_LINKS:
        return "links.get";
    case CLI_ADDRESS_JOB:
        return "job.get";
    }
    return NULL;
}

int run_show(const CliArgs *args) {
    const char *view = cli_value(args, "--view");
    if (args->input_count == 0) {
        return cli_fail(CLI_EXIT_USAGE, "show needs an address\n%s", cli_usage_of("show"));
    }
    CliAddress address;
    if (!cli_address_parse(args->inputs[0], &address)) {
        return cli_fail(CLI_EXIT_USAGE,
                        "'%s' is not an address: write <kind>:<id> (item, piece, project, subject, collection, style, links or job), or item:<id>@v<n> for one version",
                        args->inputs[0]);
    }
    if (view && strcmp(view, "json") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "--view takes json (the other views come with Ursatz Text)");
    }
    cJSON *params = NULL;
    const char *query = query_of(&address, &params);
    if (!query) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    CliSession *session = NULL;
    int opened = cli_session_open(cli_global(), &session);
    if (opened != 0) {
        cJSON_Delete(params);
        return opened;
    }
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus status = cli_session_query(session, query, params, &reply, &error);
    cJSON_Delete(params);
    int result = 0;
    if (status != URSATZ_OK) {
        result = fail_call(status, &error);
    } else if (reply.bytes) {
        result = cli_fail(CLI_EXIT_UNSATISFIABLE, "%s answers a file", query);
    } else {
        cli_print_json(reply.json);
    }
    workshop_reply_destroy(&reply);
    cli_session_close(session);
    return result;
}

int run_history(const CliArgs *args) {
    const char *format = cli_value(args, "--format");
    bool json = false;
    if (format) {
        json = cli_equals_ignore_case(format, "json");
        if (!json && !cli_equals_ignore_case(format, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", format);
        }
    }
    CliAddress address;
    bool has_address = args->input_count > 0;
    if (has_address && !cli_address_parse(args->inputs[0], &address)) {
        return cli_fail(CLI_EXIT_USAGE, "'%s' is not an address: write <kind>:<id>", args->inputs[0]);
    }
    cJSON *params = cJSON_CreateObject();
    bool ok = params && (!has_address || cJSON_AddStringToObject(params, "target", address.id));
    if (ok && cli_value(args, "--limit")) {
        char *end = NULL;
        long limit = strtol(cli_value(args, "--limit"), &end, 10);
        if (end == cli_value(args, "--limit") || *end != '\0' || limit < 1 || limit > 1000) {
            cJSON_Delete(params);
            return cli_fail(CLI_EXIT_USAGE, "--limit must be a whole number from 1 to 1000, not '%s'", cli_value(args, "--limit"));
        }
        ok = cJSON_AddNumberToObject(params, "limit", (double)limit) != NULL;
    }
    if (!ok) {
        cJSON_Delete(params);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    CliSession *session = NULL;
    int opened = cli_session_open(cli_global(), &session);
    if (opened != 0) {
        cJSON_Delete(params);
        return opened;
    }
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus status = cli_session_query(session, "journal.list", params, &reply, &error);
    cJSON_Delete(params);
    int result = 0;
    if (status != URSATZ_OK) {
        result = fail_call(status, &error);
    } else if (json) {
        cli_print_json(reply.json);
    } else {
        const cJSON *record = NULL;
        cJSON_ArrayForEach(record, cJSON_GetObjectItemCaseSensitive(reply.json, "records")) {
            const char *target = cli_json_string(record, "target");
            printf("%.0f  %s  %s  %s  %s\n", cli_json_number(record, "seq"), cli_json_string(record, "at"), cli_json_string(record, "actor"),
                   cli_json_string(record, "command"), target[0] ? target : "-");
        }
    }
    workshop_reply_destroy(&reply);
    cli_session_close(session);
    return result;
}
