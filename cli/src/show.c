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

static const char *kind_name(CliAddressKind kind) {
    switch (kind) {
    case CLI_ADDRESS_ITEM:
        return "item";
    case CLI_ADDRESS_PIECE:
        return "piece";
    case CLI_ADDRESS_PROJECT:
        return "project";
    case CLI_ADDRESS_SUBJECT:
        return "subject";
    case CLI_ADDRESS_COLLECTION:
        return "collection";
    case CLI_ADDRESS_STYLE:
        return "style";
    case CLI_ADDRESS_LINKS:
        return "links";
    case CLI_ADDRESS_JOB:
        return "job";
    }
    return "";
}

/* The query that reads what the address names as JSON, with its params (a new object); NULL on allocation failure. */
static const char *json_query_of(const CliAddress *address, cJSON **params) {
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

/* The params of a view query or of the tool text: the view, the measures a-b of --measures and the section of the address. */
static bool add_view_params(cJSON *params, const char *view, const char *measures, const char *section) {
    bool ok = cJSON_AddStringToObject(params, "view", view) != NULL;
    if (ok && measures) {
        int first = 0;
        int last = 0;
        cJSON *range = cJSON_AddObjectToObject(params, "measures");
        ok = range && cli_parse_measure_range(measures, &first, &last) && cJSON_AddNumberToObject(range, "first", first) &&
             cJSON_AddNumberToObject(range, "last", last);
    }
    return ok && (!section || !section[0] || cJSON_AddStringToObject(params, "section", section));
}

int run_show(const CliArgs *args) {
    const char *view = cli_value(args, "--view");
    const char *measures = cli_value(args, "--measures");
    if (args->input_count == 0) {
        return cli_fail(CLI_EXIT_USAGE, "show needs an address or a file\n%s", cli_usage_of("show"));
    }
    const char *input = args->inputs[0];
    bool is_address = cli_is_address(input);
    CliAddress address;
    if (is_address && !cli_address_parse(input, &address)) {
        return cli_fail(CLI_EXIT_USAGE,
                        "'%s' is not an address: write <kind>:<id> (item, piece, project, subject, collection, style, links or job), or item:<id>@v<n> for one version, "
                        "and /<section> after an item or a piece",
                        input);
    }
    bool layered = !is_address || address.kind == CLI_ADDRESS_ITEM || address.kind == CLI_ADDRESS_PIECE;
    if (!view) {
        view = layered ? "summary" : "json";
    }
    if (is_address && !layered && strcmp(view, "json") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "%s addresses show only --view json", kind_name(address.kind));
    }
    int first = 0;
    int last = 0;
    if (measures && !cli_parse_measure_range(measures, &first, &last)) {
        return cli_fail(CLI_EXIT_USAGE, "--measures must be <first>-<last>, as in 12-16, not '%s'", measures);
    }
    bool json = strcmp(view, "json") == 0;
    const char *query = NULL;
    const char *tool = NULL;
    cJSON *params = NULL;
    if (!is_address) {
        tool = "text";
        params = cJSON_CreateObject();
        bool ok = params && cJSON_AddStringToObject(params, "path", input) && add_view_params(params, view, measures, NULL);
        if (!ok) {
            cJSON_Delete(params);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
    } else if (json || !layered) {
        query = json_query_of(&address, &params);
    } else {
        query = address.kind == CLI_ADDRESS_ITEM ? "item.view" : "library.view";
        params = cJSON_CreateObject();
        bool ok = params && cJSON_AddStringToObject(params, "id", address.id) && (address.version <= 0 || cJSON_AddNumberToObject(params, "version", (double)address.version)) &&
                  add_view_params(params, view, measures, address.section);
        if (!ok) {
            cJSON_Delete(params);
            params = NULL;
        }
    }
    if (!params) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    CliSession *session = NULL;
    int result = 0;
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus status;
    if (tool) {
        /* a file is read by this process: the tool needs no data folder */
        Workshop *workshop = cli_tools();
        status = workshop ? workshop_tool(workshop, tool, params, NULL, &reply, &error) : URSATZ_ERR_INTERNAL;
        cJSON_Delete(params);
    } else {
        int opened = cli_session_open(cli_global(), &session);
        if (opened != 0) {
            cJSON_Delete(params);
            return opened;
        }
        status = cli_session_query(session, query, params, &reply, &error);
        cJSON_Delete(params);
    }
    if (status != URSATZ_OK) {
        result = fail_call(status, &error);
    } else if (reply.bytes) {
        result = cli_fail(CLI_EXIT_UNSATISFIABLE, "%s answers a file", query ? query : tool);
    } else if (tool && json) {
        cJSON *tree = cJSON_Parse(cli_json_string(reply.json, "json"));
        if (tree) {
            cli_print_json(tree);
            cJSON_Delete(tree);
        } else {
            result = cli_fail(CLI_EXIT_UNSATISFIABLE, "the tool text did not answer JSON");
        }
    } else if (!json && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(reply.json, "text"))) {
        fputs(cli_json_string(reply.json, "text"), stdout);
    } else {
        cli_print_json(reply.json);
    }
    workshop_reply_destroy(&reply);
    if (session) {
        cli_session_close(session);
    }
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
