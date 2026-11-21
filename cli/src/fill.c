#include "fill.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_tools.h"

#include "cli_options.h"
#include "cli_params.h"
#include "workflow.h"

#define MAX_CANDIDATES 32

typedef struct {
    const char *input;
    const char *seed_text;
    const char *out_path;
    const char *layers_path;
    const char *midi_path;
    const char *musicxml_path;
    const char *tempo_text;
    bool plain_midi;
    const char *record_path;
    const char *explain_path;
    const char *format_text;
    const char *candidates_text;
    const char *style_path;
    CliStyleWithout without;
    const char *style_weight_text;
    const char *blend_path;
    const char *blend_weight_text;
    const char *blend_weights_text;
    bool check;
    bool json;
} Options;


static const CliOptionRow ROWS[] = {
    {"--seed", offsetof(Options, seed_text), false},
    {"--check", offsetof(Options, check), true},
    {"--candidates", offsetof(Options, candidates_text), false},
    {"--style", offsetof(Options, style_path), false},
    {"--style-weight", offsetof(Options, style_weight_text), false},
    {"--blend", offsetof(Options, blend_path), false},
    {"--blend-weight", offsetof(Options, blend_weight_text), false},
    {"--blend-weights", offsetof(Options, blend_weights_text), false},
    {"--out", offsetof(Options, out_path), false},
    {"--layers", offsetof(Options, layers_path), false},
    {"--midi", offsetof(Options, midi_path), false},
    {"--musicxml", offsetof(Options, musicxml_path), false},
    {"--tempo", offsetof(Options, tempo_text), false},
    {"--plain-midi", offsetof(Options, plain_midi), true},
    {"--record", offsetof(Options, record_path), false},
    {"--explain", offsetof(Options, explain_path), false},
    {"--format", offsetof(Options, format_text), false},
};

static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->input = args->input_count > 0 ? args->inputs[0] : NULL;
    cli_options_read(args, ROWS, sizeof(ROWS) / sizeof(ROWS[0]), options);
    return cli_style_without_read(&options->without, args);
}

static int check_options(Options *options) {
    if (!options->input) {
        return cli_needs("fill", "a plan file", true);
    }
    if (options->format_text) {
        if (cli_equals_ignore_case(options->format_text, "json")) {
            options->json = true;
        } else if (!cli_equals_ignore_case(options->format_text, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", options->format_text);
        }
    }
    return 0;
}

/* The request: the plan and the options of the command as members. */
static cJSON *build_request(const Options *options, cJSON *plan) {
    cJSON *request = cJSON_CreateObject();
    cJSON *without = cJSON_CreateArray();
    bool ok = request && without;
    ok = ok && (!plan || cJSON_AddItemToObject(request, "plan", plan));
    ok = ok && (!options->seed_text || cJSON_AddStringToObject(request, "seed", options->seed_text)) &&
         (!options->candidates_text || cJSON_AddStringToObject(request, "candidates", options->candidates_text)) &&
         (!options->check || cJSON_AddTrueToObject(request, "check")) &&
         (!options->style_path || cJSON_AddStringToObject(request, "style", options->style_path)) &&
         (!options->style_weight_text || cJSON_AddStringToObject(request, "style_weight", options->style_weight_text)) &&
         (!options->blend_path || cJSON_AddStringToObject(request, "blend", options->blend_path)) &&
         (!options->blend_weight_text || cJSON_AddStringToObject(request, "blend_weight", options->blend_weight_text)) &&
         (!options->blend_weights_text || cJSON_AddStringToObject(request, "blend_weights", options->blend_weights_text)) &&
         (!options->midi_path || cJSON_AddTrueToObject(request, "midi")) &&
         (!options->musicxml_path || cJSON_AddTrueToObject(request, "musicxml")) &&
         (!options->tempo_text || cJSON_AddStringToObject(request, "tempo", options->tempo_text)) &&
         (!options->plain_midi || cJSON_AddTrueToObject(request, "plain_midi")) &&
         (!options->record_path || cJSON_AddTrueToObject(request, "record")) &&
         (!options->layers_path || cJSON_AddTrueToObject(request, "layers"));
    if (ok && options->explain_path) {
        size_t length = strlen(options->explain_path);
        bool json = length >= 5 && cli_equals_ignore_case(options->explain_path + length - 5, ".json");
        ok = cJSON_AddStringToObject(request, "explain", json ? "json" : "text") != NULL;
    }
    for (size_t i = 0; ok && i < options->without.count; i++) {
        ok = cJSON_AddItemToArray(without, cJSON_CreateString(options->without.names[i]));
    }
    ok = ok && (options->without.count == 0 || cJSON_AddItemToObject(request, "style_without", without));
    if (!ok || options->without.count == 0) {
        cJSON_Delete(without);
    }
    if (!ok) {
        cJSON_Delete(request);
        return NULL;
    }
    return request;
}

/* The files of a run, plan first; the paths written are listed in files. */
static int write_outputs(const Options *options, const CliResponse *response, const cJSON *document, const char **files,
                         size_t *file_count) {
    CliFile list[5];
    size_t count = 0;
    cJSON *plan = cJSON_Parse(response->plan_json);
    int status = 0;
    if (!plan) {
        return cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", options->out_path ? options->out_path : "");
    }
    if (options->out_path) {
        list[count++] = (CliFile){options->out_path, CLI_FILE_PLAN, plan, NULL, 0, false};
    }
    if (options->midi_path) {
        list[count++] = (CliFile){options->midi_path, CLI_FILE_BYTES, NULL, response->midi, response->midi_size, true};
    }
    if (options->musicxml_path) {
        list[count++] = (CliFile){options->musicxml_path, CLI_FILE_BYTES, NULL, response->musicxml,
                                  response->musicxml ? strlen(response->musicxml) : 0, true};
    }
    if (options->record_path) {
        const char *record = cli_json_string(document, "record");
        list[count++] = (CliFile){options->record_path, CLI_FILE_BYTES, NULL, record, strlen(record), false};
    }
    if (options->explain_path) {
        const char *explanation = cli_json_string(document, "explanation");
        list[count++] = (CliFile){options->explain_path, CLI_FILE_BYTES, NULL, explanation, strlen(explanation), false};
    }
    status = cli_write_files(list, count);
    for (size_t i = 0; status == 0 && i < count; i++) {
        files[i] = list[i].path;
    }
    *file_count = status == 0 ? count : 0;
    cJSON_Delete(plan);
    return status;
}

int run_fill(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    status = check_options(&options);
    if (status != 0) {
        return status;
    }
    /* A plan file that cannot be read is reported after the options are checked. */
    cJSON *plan = NULL;
    CliHeldFailure held;
    cli_read_plan_held(options.input, &plan, &held);
    cJSON *request = build_request(&options, plan);
    CliResponse response;
    UrsatzStatus called = request ? cli_call("fill", request, &response) : URSATZ_ERR_ALLOC;
    if (!request) {
        cJSON_Delete(plan);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    if (called == URSATZ_ERR_UNSATISFIABLE && response.json && !response.error.path[0]) {
        status = cli_report_stuck(&response.error, &response);
    } else if (called != URSATZ_OK) {
        status = cli_fail_plan_api(called, &response.error, options.input, &held);
    } else {
        cJSON *document = cJSON_Parse(response.json);
        const char *files[5];
        size_t file_count = 0;
        status = document ? write_outputs(&options, &response, document, files, &file_count)
                          : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        if (status == 0 && options.layers_path) {
            status = cli_write_layers(options.layers_path, &response);
        }
        if (status == 0) {
            cli_print_notices(document);
            cJSON_DeleteItemFromObjectCaseSensitive(document, "realization");
            cJSON_DeleteItemFromObjectCaseSensitive(document, "reading");
            cJSON_DeleteItemFromObjectCaseSensitive(document, "record");
            cJSON_DeleteItemFromObjectCaseSensitive(document, "explanation");
            if (options.json) {
                cli_print_composed_json(document, files, file_count);
            } else {
                cli_print_candidates_text(document);
                cli_print_composed_text(document, files, file_count);
            }
        }
        cJSON_Delete(document);
    }
    cli_response_destroy(&response);
    return status;
}
