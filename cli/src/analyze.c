#include "analyze.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <common/files.h>

#include "cli_options.h"
#include "cli_tools.h"

/* Writes <dir>/<relative> (making the folders); the failure is printed. */
static bool write_layer(const char *dir, const char *relative, const char *text) {
    char path[1200];
    char folder[1200];
    if (!common_path_join(path, sizeof path, dir, relative)) {
        fprintf(stderr, "Error: cannot write '%s/%s'\n", dir, relative);
        return false;
    }
    snprintf(folder, sizeof folder, "%s", path);
    char *slash = strrchr(folder, '/');
    if (slash) {
        *slash = '\0';
    }
    bool ok = common_dir_make(folder, NULL) == URSATZ_OK && common_file_write_atomic(path, text, strlen(text), NULL) == URSATZ_OK;
    if (!ok) {
        fprintf(stderr, "Error: cannot write '%s'\n", path);
    }
    return ok;
}

/* The failure of the analyze tool as the command line says it; returns the exit status. */
static int report_failure(UrsatzStatus status, const UrsatzError *error, const char *path) {
    if (status == URSATZ_ERR_FORMAT && strcmp(error->path, "/params/path") == 0) {
        fprintf(stderr, "%s\n", error->message); /* the importer's own message */
        return 1;
    }
    if (strcmp(error->path, "/params/pickup") == 0 || strcmp(error->path, "/params/value_scale") == 0) {
        if (strstr(error->message, "must be shorter") || strstr(error->message, "too large")) {
            fprintf(stderr, "Error: cannot apply the notation reading to '%s': %s\n", path, error->message);
        } else {
            fprintf(stderr, "Error: %s: %s\n", strcmp(error->path, "/params/pickup") == 0 ? "--pickup" : "--value-scale", error->message);
        }
        return 1;
    }
    if (strcmp(error->path, "/params/meter") == 0) {
        fprintf(stderr, "Error: --meter: %s\n", error->message);
        return 1;
    }
    if (strcmp(error->path, "/params/input_format") == 0) {
        fprintf(stderr, "Error: --input-format must be auto, midi or musicxml\n");
        return 1;
    }
    if (strcmp(error->path, "/params/text") == 0) {
        fprintf(stderr, "Error: no measure of '%s' falls in the range given to --measures\n", path);
        return 1;
    }
    if (status == URSATZ_ERR_UNSATISFIABLE) {
        fprintf(stderr, "Error: cannot make a plan of the analysis: %s\n", error->message);
        return 2;
    }
    if (status == URSATZ_ERR_ALLOC) {
        fprintf(stderr, "Error: out of memory while analyzing '%s'\n", path);
        return 1;
    }
    if (strcmp(error->message, "the analysis failed") == 0) {
        fprintf(stderr, "Error: analysis failed for '%s'\n", path);
        return 1;
    }
    fprintf(stderr, "Error: %s\n", error->message);
    return 1;
}

int run_analyze(const CliArgs *args) {
    const char *path = args->input_count > 0 ? args->inputs[0] : NULL;
    if (!path) {
        return cli_fail(CLI_EXIT_USAGE, "analyze needs an input file\n%s", cli_usage_of("analyze"));
    }
    const char *view = cli_value(args, "--view");
    const char *measures = cli_value(args, "--measures");
    const char *title = cli_value(args, "--title");
    const char *layers_dir = cli_value(args, "--layers");
    const char *model_plan = cli_value(args, "--model-plan");
    if (view && strcmp(view, "reading") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "--view must be reading, not '%s'", view);
    }
    if (!view && (measures || title)) {
        return cli_fail(CLI_EXIT_USAGE, "--measures and --title need --view reading");
    }
    if (view && (layers_dir || model_plan)) {
        return cli_fail(CLI_EXIT_USAGE, "--layers and --model-plan write files, so they do not go with --view reading");
    }
    int first = 0;
    int last = 0;
    if (measures && !cli_parse_measure_range(measures, &first, &last)) {
        return cli_fail(CLI_EXIT_USAGE, "--measures must be <first>-<last>, as in 12-16, not '%s'", measures);
    }

    cJSON *params = cJSON_CreateObject();
    bool ok = params && cJSON_AddStringToObject(params, "path", path) &&
              (!cli_value(args, "--input-format") || cJSON_AddStringToObject(params, "input_format", cli_value(args, "--input-format"))) &&
              (!cli_value(args, "--meter") || cJSON_AddStringToObject(params, "meter", cli_value(args, "--meter"))) &&
              (!cli_value(args, "--value-scale") || cJSON_AddStringToObject(params, "value_scale", cli_value(args, "--value-scale"))) &&
              (!cli_value(args, "--pickup") || cJSON_AddStringToObject(params, "pickup", cli_value(args, "--pickup"))) &&
              (!layers_dir || cJSON_AddTrueToObject(params, "layers")) && (!model_plan || cJSON_AddTrueToObject(params, "model_plan"));
    if (ok && view) {
        cJSON *text = cJSON_AddObjectToObject(params, "text");
        ok = text && (!measures || (cJSON_AddNumberToObject(text, "first_measure", first) && cJSON_AddNumberToObject(text, "last_measure", last))) &&
             (!title || cJSON_AddStringToObject(text, "title", title));
    }
    Workshop *workshop = cli_tools();
    if (!ok || !workshop) {
        cJSON_Delete(params);
        return cli_fail(CLI_EXIT_USAGE, ok ? "a knowledge snapshot is required" : "out of memory");
    }
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus status = workshop_tool(workshop, "analyze", params, NULL, &reply, &error);
    cJSON_Delete(params);
    if (status != URSATZ_OK) {
        workshop_reply_destroy(&reply);
        return report_failure(status, &error, path);
    }

    const cJSON *warning = NULL;
    cJSON_ArrayForEach(warning, cJSON_GetObjectItemCaseSensitive(reply.json, "warnings")) {
        if (cJSON_IsString(warning)) {
            fprintf(stderr, "%s\n", warning->valuestring);
        }
    }
    const char *text_member = NULL;
    int exit_status = 0;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(reply.json, "text");
    if (view && cJSON_IsString(item)) {
        text_member = item->valuestring;
        fputs(text_member, stdout);
    } else {
        const cJSON *realization = cJSON_GetObjectItemCaseSensitive(reply.json, "realization");
        const cJSON *reading = cJSON_GetObjectItemCaseSensitive(reply.json, "reading");
        const cJSON *plan = cJSON_GetObjectItemCaseSensitive(reply.json, "plan");
        const cJSON *document = cJSON_GetObjectItemCaseSensitive(reply.json, "document");
        const cJSON *document_path = cJSON_GetObjectItemCaseSensitive(reply.json, "document_path");
        if (layers_dir && !(cJSON_IsString(realization) && cJSON_IsString(reading) && write_layer(layers_dir, "realization.json", realization->valuestring) &&
                            write_layer(layers_dir, "readings/analyzer.json", reading->valuestring))) {
            exit_status = 2;
        } else if (model_plan && !(cJSON_IsString(plan) &&
                                   common_file_write_atomic(model_plan, plan->valuestring, strlen(plan->valuestring), NULL) == URSATZ_OK)) {
            fprintf(stderr, "Error: cannot write the plan of the analysis to '%s'\n", model_plan);
            exit_status = 2;
        } else if (!cJSON_IsString(document) || !cJSON_IsString(document_path)) {
            fprintf(stderr, "Error: out of memory while building analysis document for '%s'\n", path);
            exit_status = 1;
        } else if (common_file_write_atomic(document_path->valuestring, document->valuestring, strlen(document->valuestring), NULL) != URSATZ_OK) {
            fprintf(stderr, "Error: cannot write analysis document to '%s'\n", document_path->valuestring);
            exit_status = 1;
        } else {
            printf("Analysis document written to %s\n", document_path->valuestring);
        }
    }
    workshop_reply_destroy(&reply);
    return exit_status;
}
