#include "corpus.h"

#include <stdio.h>
#include <string.h>

#include <cJSON.h>

#include "cli_options.h"
#include "cli_tools.h"
#include "workflow.h"

/* Calls a tool of the workshop with params (taken and deleted); the reply is filled on success. Returns 0, or the exit status after printing "Error: <message>". */
static int call_tool(const char *name, cJSON *params, WorkshopReply *reply) {
    Workshop *workshop = cli_tools();
    if (!params || !workshop) {
        cJSON_Delete(params);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, params ? "a knowledge snapshot is required" : "out of memory");
    }
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus status = workshop_tool(workshop, name, params, NULL, reply, &error);
    cJSON_Delete(params);
    if (status != URSATZ_OK) {
        workshop_reply_destroy(reply);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", error.message);
    }
    return 0;
}

/* The importer's text of a failed piece and the warnings of one that worked, on stderr, then the piece's line on stdout. */
static void print_piece(const cJSON *piece) {
    const cJSON *message = cJSON_GetObjectItemCaseSensitive(piece, "import_message");
    if (cJSON_IsString(message)) {
        fprintf(stderr, "%s\n", message->valuestring);
    }
    const cJSON *warning = NULL;
    cJSON_ArrayForEach(warning, cJSON_GetObjectItemCaseSensitive(piece, "warnings")) {
        if (cJSON_IsString(warning)) {
            fprintf(stderr, "%s\n", warning->valuestring);
        }
    }
    const char *state = cli_json_string(piece, "state");
    const char *path = cli_json_string(piece, "path");
    const cJSON *error = cJSON_GetObjectItemCaseSensitive(piece, "error");
    if (cJSON_IsString(error)) {
        printf("%s %s: %s\n", state, path, error->valuestring);
    } else {
        printf("%s %s\n", state, path);
    }
}

int run_corpus(const CliArgs *args) {
    if (args->input_count == 0) {
        return cli_fail(CLI_EXIT_USAGE, "corpus needs the folder of pieces\n%s", cli_usage_of("corpus"));
    }
    cJSON *params = cJSON_CreateObject();
    if (params) {
        cJSON_AddStringToObject(params, "dir", args->inputs[0]);
        if (cli_value(args, "--out")) {
            cJSON_AddStringToObject(params, "out", cli_value(args, "--out"));
        }
        cJSON_AddBoolToObject(params, "resume", cli_switch(args, "--resume"));
    }
    WorkshopReply reply;
    int status = call_tool("corpus", params, &reply);
    if (status != 0) {
        return status;
    }
    const cJSON *piece = NULL;
    cJSON_ArrayForEach(piece, cJSON_GetObjectItemCaseSensitive(reply.json, "pieces")) {
        print_piece(piece);
    }
    printf("%.0f analyzed, %.0f skipped, %.0f failed\n", cli_json_number(reply.json, "analyzed"), cli_json_number(reply.json, "skipped"),
           cli_json_number(reply.json, "failed"));
    workshop_reply_destroy(&reply);
    return 0;
}

int run_style_profile(const CliArgs *args) {
    const char *name = cli_value(args, "--name");
    if (args->input_count == 0) {
        return cli_fail(CLI_EXIT_USAGE, "style-profile needs the output folder of a corpus run\n%s", cli_usage_of("style-profile"));
    }
    if (!name || name[0] == '\0') {
        return cli_fail(CLI_EXIT_USAGE, "style-profile needs --name <name>\n%s", cli_usage_of("style-profile"));
    }
    cJSON *params = cJSON_CreateObject();
    if (params) {
        cJSON_AddStringToObject(params, "dir", args->inputs[0]);
        cJSON_AddStringToObject(params, "name", name);
        if (cli_value(args, "--out")) {
            cJSON_AddStringToObject(params, "out", cli_value(args, "--out"));
        }
    }
    WorkshopReply reply;
    int status = call_tool("style-profile", params, &reply);
    if (status != 0) {
        return status;
    }
    double stale = cli_json_number(reply.json, "stale");
    if (stale > 0) {
        fprintf(stderr, "Warning: %.0f stale documents in '%s' (analyzed by another version; run the corpus again)\n", stale, args->inputs[0]);
    }
    printf("Style model '%s' from %.0f pieces written to %s\n", name, cli_json_number(reply.json, "pieces"), cli_json_string(reply.json, "path"));
    workshop_reply_destroy(&reply);
    return 0;
}
