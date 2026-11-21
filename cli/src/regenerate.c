#include "regenerate.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_tools.h"

#include "address.h"
#include "cli_options.h"
#include "cli_params.h"
#include "items.h"
#include "workflow.h"

typedef struct {
    const char *input;
    CliTargetOptions target;
    const char *seed_text;
    const char *out_path;
    const char *midi_path;
    const char *musicxml_path;
    const char *tempo_text;
    bool plain_midi;
    const char *format_text;
    bool json;
} Options;



static const CliOptionRow ROWS[] = {
    {"--out", offsetof(Options, out_path), false},
    {"--seed", offsetof(Options, seed_text), false},
    {"--midi", offsetof(Options, midi_path), false},
    {"--musicxml", offsetof(Options, musicxml_path), false},
    {"--tempo", offsetof(Options, tempo_text), false},
    {"--plain-midi", offsetof(Options, plain_midi), true},
    {"--format", offsetof(Options, format_text), false},
};

static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->input = args->input_count > 0 ? args->inputs[0] : NULL;
    options->target.node_text = cli_value(args, "--node");
    options->target.section_text = cli_value(args, "--section");
    options->target.lane_text = cli_value(args, "--lane");
    cli_options_read(args, ROWS, sizeof(ROWS) / sizeof(ROWS[0]), options);
    return 0;
}

static int check_options(Options *options, bool regenerate) {
    const char *usage = regenerate ? cli_usage_of("regenerate") : cli_usage_of("lock");
    const char *command = regenerate ? "regenerate" : "lock";
    if (!options->input) {
        return cli_fail(CLI_EXIT_USAGE, "%s needs a plan file\n%s", command, usage);
    }
    int status = cli_check_target(&options->target, command, usage);
    if (status != 0) {
        return status;
    }
    if (!options->out_path) {
        return cli_fail(CLI_EXIT_USAGE, "%s needs --out <plan.json>\n%s", command, usage);
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

/* The request: the plan, the node and the options of the command as members. */
static cJSON *build_request(const Options *options, cJSON *plan) {
    cJSON *request = cJSON_CreateObject();
    bool ok = request && (!plan || cJSON_AddItemToObject(request, "plan", plan)) && cli_add_target(request, &options->target) &&
              (!options->seed_text || cJSON_AddStringToObject(request, "seed", options->seed_text)) &&
              (!options->midi_path || cJSON_AddTrueToObject(request, "midi")) &&
              (!options->musicxml_path || cJSON_AddTrueToObject(request, "musicxml")) &&
              (!options->tempo_text || cJSON_AddStringToObject(request, "tempo", options->tempo_text)) &&
              (!options->plain_midi || cJSON_AddTrueToObject(request, "plain_midi"));
    if (!ok) {
        cJSON_Delete(request);
        return NULL;
    }
    return request;
}

/* --- lock ------------------------------------------------------------------------ */

int run_lock(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    status = check_options(&options, false);
    if (status != 0) {
        return status;
    }
    cJSON *plan = NULL;
    CliHeldFailure held;
    cli_read_plan_held(options.input, &plan, &held);
    cJSON *request = build_request(&options, plan);
    if (!request) {
        cJSON_Delete(plan);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    CliResponse response;
    UrsatzStatus called = cli_call("lock", request, &response);
    if (called != URSATZ_OK) {
        status = cli_fail_plan_api(called, &response.error, options.input, &held);
    } else {
        cJSON *document = cJSON_Parse(response.json);
        cJSON *locked_plan = cJSON_Parse(response.plan_json);
        status = document && locked_plan ? cli_write_plan_object(locked_plan, options.out_path, options.out_path)
                                         : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        if (status == 0) {
            const cJSON *locked = cJSON_GetObjectItemCaseSensitive(document, "locked");
            double nodes = cli_json_number(locked, "nodes");
            double spans = cli_json_number(locked, "spans");
            if (options.json) {
                cJSON_DeleteItemFromObjectCaseSensitive(document, "what");
                cJSON_AddStringToObject(document, "plan", options.out_path);
                char *text = cJSON_PrintUnformatted(document);
                printf("%s\n", text ? text : "{}");
                cJSON_free(text);
            } else {
                printf("locked %s: %.0f %s and %.0f lane %s now Given\n", cli_json_string(document, "what"), nodes,
                       nodes == 1 ? "node" : "nodes", spans, spans == 1 ? "span" : "spans");
                printf("wrote %s\n", options.out_path);
            }
        }
        cJSON_Delete(document);
        cJSON_Delete(locked_plan);
    }
    cli_response_destroy(&response);
    return status;
}

/* --- regenerate -------------------------------------------------------------------- */

/* What was reopened, from the "reopened" member of the result. */
static void print_reopened_text(const cJSON *reopened) {
    const cJSON *sections = cJSON_GetObjectItemCaseSensitive(reopened, "sections");
    if (cJSON_GetArraySize(sections) > 0) {
        printf("reopened sections");
        size_t at = 0;
        const cJSON *letter = NULL;
        cJSON_ArrayForEach(letter, sections) {
            printf(at++ ? ", %s" : " %s", letter->valuestring);
        }
        printf("\n");
    }
    static const char *const LANES[] = {"harmony", "bass", "melody", "inner_voice"};
    static const char *const LABELS[] = {"harmony", "bass", "melody", "inner voice"};
    const cJSON *spans = cJSON_GetObjectItemCaseSensitive(reopened, "spans");
    bool any = false;
    for (int lane = 0; lane < 4; lane++) {
        double count = cli_json_number(spans, LANES[lane]);
        if (count > 0) {
            printf("%s %s %.0f", any ? "," : "reopened lane spans:", LABELS[lane], count);
            any = true;
        }
    }
    if (any) {
        printf("\n");
    }
}

int run_regenerate(const CliArgs *args) {
    if (args->input_count > 0 && cli_is_address(args->inputs[0])) {
        return run_regenerate_item(args);
    }
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    status = check_options(&options, true);
    if (status != 0) {
        return status;
    }
    cJSON *plan = NULL;
    CliHeldFailure held;
    cli_read_plan_held(options.input, &plan, &held);
    cJSON *request = build_request(&options, plan);
    if (!request) {
        cJSON_Delete(plan);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    CliResponse response;
    UrsatzStatus called = cli_call("regenerate", request, &response);
    if (called == URSATZ_ERR_UNSATISFIABLE && response.json && !response.error.path[0]) {
        status = cli_report_stuck(&response.error, &response);
    } else if (called != URSATZ_OK) {
        status = cli_fail_plan_api(called, &response.error, options.input, &held);
    } else {
        cJSON *document = cJSON_Parse(response.json);
        cJSON *written = cJSON_Parse(response.plan_json);
        CliFile list[3];
        size_t count = 0;
        const char *files[3];
        status = document && written ? 0 : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        if (status == 0) {
            list[count++] = (CliFile){options.out_path, CLI_FILE_PLAN, written, NULL, 0, false};
            if (options.midi_path) {
                list[count++] = (CliFile){options.midi_path, CLI_FILE_BYTES, NULL, response.midi, response.midi_size, true};
            }
            if (options.musicxml_path) {
                list[count++] = (CliFile){options.musicxml_path, CLI_FILE_BYTES, NULL, response.musicxml,
                                          response.musicxml ? strlen(response.musicxml) : 0, true};
            }
            status = cli_write_files(list, count);
        }
        for (size_t i = 0; status == 0 && i < count; i++) {
            files[i] = list[i].path;
        }
        if (status == 0) {
            if (options.json) {
                cli_print_composed_json(document, files, count);
            } else {
                print_reopened_text(cJSON_GetObjectItemCaseSensitive(document, "reopened"));
                cli_print_composed_text(document, files, count);
            }
        }
        cJSON_Delete(document);
        cJSON_Delete(written);
    }
    cli_response_destroy(&response);
    return status;
}
