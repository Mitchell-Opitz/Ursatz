#include "variations.h"

#include <stddef.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cli_tools.h"

#include "cli_options.h"
#include "cli_params.h"
#include "workflow.h"

/* The most versions one command keeps (VARIATIONS_MAX). */
#define VARIATIONS_MAX 16

typedef struct {
    const char *input;
    CliTargetOptions target;
    const char *count_text;
    const char *seed_text;
    const char *out_dir;
    const char *format_text;
    bool midi;
    bool musicxml;
    const char *tempo_text;
    bool plain_midi;
    bool json;
} Options;



static const CliOptionRow ROWS[] = {
    {"--write-midi", offsetof(Options, midi), true},
    {"--write-musicxml", offsetof(Options, musicxml), true},
    {"--plain-midi", offsetof(Options, plain_midi), true},
    {"--tempo", offsetof(Options, tempo_text), false},
    {"--count", offsetof(Options, count_text), false},
    {"--seed", offsetof(Options, seed_text), false},
    {"--out-dir", offsetof(Options, out_dir), false},
    {"--format", offsetof(Options, format_text), false},
};

static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->input = args->input_count > 0 ? args->inputs[0] : NULL;
    cli_options_read(args, ROWS, sizeof(ROWS) / sizeof(ROWS[0]), options);
    options->target.node_text = cli_value(args, "--node");
    options->target.section_text = cli_value(args, "--section");
    options->target.lane_text = cli_value(args, "--lane");
    return 0;
}

static int check_options(Options *options, bool harmonize) {
    const char *command = harmonize ? "harmonize" : "variations";
    const char *usage = harmonize ? cli_usage_of("harmonize") : cli_usage_of("variations");
    if (!options->input) {
        return cli_fail(CLI_EXIT_USAGE, "%s needs a plan file\n%s", command, usage);
    }
    if (harmonize) {
        if (!options->target.section_text) {
            return cli_fail(CLI_EXIT_USAGE, "harmonize needs --section <letter>\n%s", usage);
        }
    }
    int status = cli_check_target(&options->target, command, usage);
    if (status != 0) {
        return status;
    }
    if (!options->count_text) {
        return cli_fail(CLI_EXIT_USAGE, "%s needs --count <n>\n%s", command, usage);
    }
    if (!options->out_dir) {
        return cli_fail(CLI_EXIT_USAGE, "%s needs --out-dir <dir>\n%s", command, usage);
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
              cJSON_AddStringToObject(request, "count", options->count_text) &&
              (!options->seed_text || cJSON_AddStringToObject(request, "seed", options->seed_text)) &&
              (!options->musicxml || cJSON_AddTrueToObject(request, "musicxml")) &&
              (!options->tempo_text || cJSON_AddStringToObject(request, "tempo", options->tempo_text)) &&
              (!options->plain_midi || cJSON_AddTrueToObject(request, "plain_midi"));
    if (!ok) {
        cJSON_Delete(request);
        return NULL;
    }
    return request;
}

/* --- the files and the result --------------------------------------------------- */

#define PATH_SIZE 1024

static int make_directory(const char *path) {
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        return cli_fail(CLI_EXIT_WRITE, "cannot create '%s': %s", path, strerror(errno));
    }
    return 0;
}

/* Writes variation-<i>.json and, when asked, .mid and .musicxml; paths[i] are the three paths of variation i. */
static int write_variations(const Options *options, const cJSON *variations, char (*paths)[3][PATH_SIZE]) {
    int status = make_directory(options->out_dir);
    size_t index = 0;
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, variations) {
        if (status != 0) {
            break;
        }
        char (*entry_paths)[PATH_SIZE] = paths[index];
        if (snprintf(entry_paths[0], PATH_SIZE, "%s/variation-%zu.json", options->out_dir, index + 1) >= PATH_SIZE) {
            return cli_fail(CLI_EXIT_WRITE, "file name in '%s' is too long", options->out_dir);
        }
        snprintf(entry_paths[1], PATH_SIZE, "%s/variation-%zu.mid", options->out_dir, index + 1);
        snprintf(entry_paths[2], PATH_SIZE, "%s/variation-%zu.musicxml", options->out_dir, index + 1);
        const cJSON *plan = cJSON_GetObjectItemCaseSensitive(entry, "plan");
        const char *musicxml = cli_json_string(entry, "musicxml");
        CliResponse midi;
        memset(&midi, 0, sizeof midi);
        CliFile files[3];
        size_t count = 0;
        files[count++] = (CliFile){entry_paths[0], CLI_FILE_PLAN, plan, NULL, 0, false};
        if (options->midi) {
            status = cli_export_midi(plan, options->tempo_text, options->plain_midi, &midi);
            files[count++] = (CliFile){entry_paths[1], CLI_FILE_BYTES, NULL, midi.midi, midi.midi_size, true};
        }
        if (status == 0 && options->musicxml) {
            files[count++] = (CliFile){entry_paths[2], CLI_FILE_BYTES, NULL, musicxml, strlen(musicxml), true};
        }
        if (status == 0) {
            status = cli_write_files(files, count);
        }
        cli_response_destroy(&midi);
        index++;
    }
    return status;
}

static void print_result(const Options *options, cJSON *document, const char (*paths)[3][PATH_SIZE]) {
    cJSON *variations = cJSON_GetObjectItemCaseSensitive(document, "variations");
    if (options->json) {
        /* The entries carry the files written, not the plan and the MusicXML. */
        size_t index = 0;
        cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, variations) {
            cJSON *sections = cJSON_DetachItemFromObjectCaseSensitive(entry, "sections");
            cJSON_DeleteItemFromObjectCaseSensitive(entry, "plan");
            cJSON_DeleteItemFromObjectCaseSensitive(entry, "musicxml");
            cJSON_AddStringToObject(entry, "plan", paths[index][0]);
            if (options->midi) {
                cJSON_AddStringToObject(entry, "midi", paths[index][1]);
            } else {
                cJSON_AddNullToObject(entry, "midi");
            }
            if (options->musicxml) {
                cJSON_AddStringToObject(entry, "musicxml", paths[index][2]);
            } else {
                cJSON_AddNullToObject(entry, "musicxml");
            }
            cJSON_AddItemToObject(entry, "sections", sections);
            index++;
        }
        char *text = cJSON_PrintUnformatted(document);
        printf("%s\n", text ? text : "{}");
        cJSON_free(text);
        return;
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, variations) {
        printf("variation %.0f  seed %.0f  ", cli_json_number(entry, "index"), cli_json_number(entry, "seed"));
        cli_print_heard_line(cJSON_GetObjectItemCaseSensitive(entry, "sections"));
        printf("\n");
    }
    printf("kept %.0f of %.0f, %.0f seeds tried\n", cli_json_number(document, "kept"), cli_json_number(document, "requested"),
           cli_json_number(document, "seeds_tried"));
}

/* The shared body of the two commands. */
static int run_command(const CliArgs *args, bool harmonize) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    status = check_options(&options, harmonize);
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
    UrsatzStatus called = cli_call(harmonize ? "harmonize" : "variations", request, &response);
    if (called != URSATZ_OK) {
        status = cli_fail_plan_api(called, &response.error, options.input, &held);
    } else {
        cJSON *document = cJSON_Parse(response.json);
        size_t kept = (size_t)cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(document, "variations"));
        char (*paths)[3][PATH_SIZE] = calloc(kept ? kept : 1, sizeof(*paths));
        status = document && paths ? write_variations(&options, cJSON_GetObjectItemCaseSensitive(document, "variations"), paths)
                                   : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        if (status == 0) {
            print_result(&options, document, (const char (*)[3][PATH_SIZE])paths);
        }
        free(paths);
        cJSON_Delete(document);
    }
    cli_response_destroy(&response);
    return status;
}

int run_variations(const CliArgs *args) {
    return run_command(args, false);
}

int run_harmonize(const CliArgs *args) {
    return run_command(args, true);
}
