#include "query.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "cli_options.h"
#include "cli_params.h"
#include "cli_tools.h"
#include "session.h"
#include "workflow.h"

/* The params object: the file's JSON ("-" is standard input), {} when none is named. NULL after printing the failure (*status is the exit status). */
static cJSON *read_params(const char *path, int *status) {
    if (!path) {
        return cJSON_CreateObject();
    }
    char *text = NULL;
    size_t size = 0;
    bool read = false;
    if (strcmp(path, "-") == 0) {
        size_t capacity = 4096;
        text = malloc(capacity + 1);
        while (text) {
            size_t got = fread(text + size, 1, capacity - size, stdin);
            size += got;
            if (size < capacity) {
                break;
            }
            capacity *= 2;
            char *grown = realloc(text, capacity + 1);
            if (!grown) {
                free(text);
                text = NULL;
            } else {
                text = grown;
            }
        }
        if (text) {
            text[size] = '\0';
            read = true;
        }
    } else {
        read = cli_read_file(path, &text, &size);
    }
    if (!read) {
        free(text);
        *status = cli_fail(CLI_EXIT_USAGE, "cannot read the params '%s'", path);
        return NULL;
    }
    cJSON *params = cJSON_Parse(text);
    free(text);
    if (!cJSON_IsObject(params)) {
        cJSON_Delete(params);
        *status = cli_fail(CLI_EXIT_USAGE, "the params '%s' are not a JSON object", path);
        return NULL;
    }
    return params;
}

int run_query(const CliArgs *args) {
    if (args->input_count == 0) {
        return cli_needs("query", "the name of a query", true);
    }
    const char *name = args->inputs[0];
    const char *format = cli_value(args, "--format");
    const char *out = cli_value(args, "--out");
    bool json = false;
    if (format) {
        json = cli_equals_ignore_case(format, "json");
        if (!json && !cli_equals_ignore_case(format, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", format);
        }
    }
    /* Every path is made absolute before the session opens: the workshop works in the data folder. */
    char *params_path = cli_value(args, "--params") && strcmp(cli_value(args, "--params"), "-") != 0 ? cli_path_absolute(cli_value(args, "--params")) : NULL;
    char *out_path = out ? cli_path_absolute(out) : NULL;
    int status = 0;
    cJSON *params = read_params(params_path ? params_path : cli_value(args, "--params"), &status);
    free(params_path);
    if (!params) {
        free(out_path);
        return status;
    }
    CliSession *session = NULL;
    int opened = cli_session_open(cli_global(), &session);
    if (opened != 0) {
        cJSON_Delete(params);
        free(out_path);
        return opened;
    }
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus result = cli_session_query(session, name, params, &reply, &error);
    cJSON_Delete(params);
    if (result != URSATZ_OK) {
        workshop_reply_destroy(&reply);
        cli_session_close(session);
        free(out_path);
        return cli_fail(result == URSATZ_ERR_REQUEST ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE, "%s%s%s", error.message, error.path[0] ? " at " : "", error.path);
    }
    if (reply.bytes) {
        if (!out_path) {
            status = cli_fail(CLI_EXIT_USAGE, "%s answers a file (%s): name it with --out <file>", name, reply.content_type);
        } else if (!cli_write_bytes(out_path, reply.bytes, reply.size)) {
            status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", out);
        } else {
            printf("wrote %s\n", out);
        }
    } else if (json) {
        char *text = cJSON_PrintUnformatted(reply.json);
        printf("%s\n", text ? text : "{}");
        cJSON_free(text);
    } else {
        cli_print_json(reply.json);
    }
    workshop_reply_destroy(&reply);
    cli_session_close(session);
    free(out_path);
    return status;
}
