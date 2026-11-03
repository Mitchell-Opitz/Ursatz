#include "fill.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/composer_api.h>

#include "cli_options.h"
#include "workflow.h"

#define MAX_CANDIDATES 32

typedef struct {
    bool help;
    const char *input;
    const char *seed_text;
    const char *out_path;
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

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", FILL_USAGE_LINE + 7);
    fprintf(out, "Realizes a plan: composes every open theme section of <plan.json> (a plan from 'ursatz-composer plan' or\n");
    fprintf(out, "'grow --out') in stages, structure, motives, harmony, bass, melody, texture and the returns, and writes the\n");
    fprintf(out, "piece. A plan with no open theme section (one from 'grow --out' is written already) is only exported.\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same plan and seed give the same output. Default: from the\n");
    fprintf(out, "                       clock. The seed used is always printed (if a seed gets stuck, the next seeds are\n");
    fprintf(out, "                       tried, up to %d, and the one that worked is printed)\n", CLI_MAX_SEED_TRIES);
    fprintf(out, "  --check              analyze every theme section with the Ursatz Analyzer and reject a run whose analysis\n");
    fprintf(out, "                       contradicts the plan (a planned cadence or the key not heard, a hard voice-leading fault)\n");
    fprintf(out, "  --candidates <n>     compose n candidates (1 to %d, seeds from --seed) and write the best, ranked by agreement\n", MAX_CANDIDATES);
    fprintf(out, "                       with the Analyzer, then score, then the lower seed; every candidate is listed\n");
    fprintf(out, "  --style <file>       prefer the harmony and melody closest to a style: a style model file from the Ursatz\n");
    fprintf(out, "                       Analyzer (--style-profile). Overrides the plan's own style. The Analyzer's distance from the\n");
    fprintf(out, "                       style is printed with the result\n");
    fprintf(out, "  --style-weight <n/d> how strongly the style is preferred (default 1/1; overrides the plan's)\n");
    fprintf(out, "  --style-without <c>  read the style without the category c (repeatable, at most 8; overrides the plan's list)\n");
    fprintf(out, "  --blend <file>       a second style model file, blended with the style (--style, or the plan's). Overrides the\n");
    fprintf(out, "                       plan's blend; --style without --blend drops the plan's blend\n");
    fprintf(out, "  --blend-weight <n/d> the second style's share in every dimension, 0 to 1 (default 1/2; overrides the plan's)\n");
    fprintf(out, "  --blend-weights harmony=<n/d>,rhythm=<n/d>,melody=<n/d>,form=<n/d>\n");
    fprintf(out, "                       the share per dimension; one not named takes --blend-weight\n");
    fprintf(out, "  --out <file.json>    write the realized plan as JSON\n");
    fprintf(out, "  --midi <file>        write the piece as a MIDI file\n");
    fprintf(out, "  --musicxml <file>    write the piece as MusicXML\n");
    fprintf(out, "  --tempo <bpm>        MIDI and MusicXML: write a tempo, quarter notes per minute from 20 to 400 (a whole number or\n");
    fprintf(out, "                       a fraction like 195/2). Default: none, the file default of 120 applies\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --record <file.json> write the records of the stages (what each choice was and why) as one JSON array\n");
    fprintf(out, "  --explain <file>     write why each candidate was chosen or rejected: the chosen candidate, its runners-up and the\n");
    fprintf(out, "                       rejected ones with their reasons per target, the analyzer check and the form heard per section, and\n");
    fprintf(out, "                       the runs thrown away. JSON when the file ends in .json, else text\n");
    fprintf(out, "  --format <text|json> how to print the result on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "The form and cadences heard in each section, and the thematic returns heard, are printed with the result; a\n");
    fprintf(out, "section need not be heard as the form it was planned as, and that is reported, not an error. When no seed\n");
    fprintf(out, "completes the plan, the targets left open are printed with the reason each is stuck, and nothing is written.\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the plan cannot be read or cannot be completed;\n");
    fprintf(out, "3 an output file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer fill piece.json --seed 7 --midi piece.mid --musicxml piece.xml\n");
}

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed_text;
        } else if (strcmp(arg, "--check") == 0) {
            options->check = true;
        } else if (strcmp(arg, "--candidates") == 0) {
            target = &options->candidates_text;
        } else if (strcmp(arg, "--style") == 0) {
            target = &options->style_path;
        } else if (strcmp(arg, "--style-without") == 0) {
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", arg);
            }
            int added = cli_style_without_add(&options->without, argv[++i]);
            if (added != 0) {
                return added;
            }
        } else if (strcmp(arg, "--style-weight") == 0) {
            target = &options->style_weight_text;
        } else if (strcmp(arg, "--blend") == 0) {
            target = &options->blend_path;
        } else if (strcmp(arg, "--blend-weight") == 0) {
            target = &options->blend_weight_text;
        } else if (strcmp(arg, "--blend-weights") == 0) {
            target = &options->blend_weights_text;
        } else if (strcmp(arg, "--out") == 0) {
            target = &options->out_path;
        } else if (strcmp(arg, "--midi") == 0) {
            target = &options->midi_path;
        } else if (strcmp(arg, "--musicxml") == 0) {
            target = &options->musicxml_path;
        } else if (strcmp(arg, "--tempo") == 0) {
            target = &options->tempo_text;
        } else if (strcmp(arg, "--plain-midi") == 0) {
            options->plain_midi = true;
        } else if (strcmp(arg, "--record") == 0) {
            target = &options->record_path;
        } else if (strcmp(arg, "--explain") == 0) {
            target = &options->explain_path;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format_text;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer fill --help)", arg);
        } else if (!options->input) {
            options->input = arg;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unexpected argument '%s'; the input is one plan file", arg);
        }
        if (target) {
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", arg);
            }
            if (*target) {
                return cli_fail(CLI_EXIT_USAGE, "%s given twice", arg);
            }
            *target = argv[++i];
        }
    }
    return 0;
}

static int check_options(Options *options) {
    if (!options->input) {
        return cli_fail(CLI_EXIT_USAGE, "fill needs a plan file\n%s", FILL_USAGE_LINE);
    }
    if (options->format_text) {
        if (request_equals_ignore_case(options->format_text, "json")) {
            options->json = true;
        } else if (!request_equals_ignore_case(options->format_text, "text")) {
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
         (!options->record_path || cJSON_AddTrueToObject(request, "record"));
    if (ok && options->explain_path) {
        size_t length = strlen(options->explain_path);
        bool json = length >= 5 && request_equals_ignore_case(options->explain_path + length - 5, ".json");
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
static int write_outputs(const Options *options, const ComposerResponse *response, const cJSON *document, const char **files,
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

int run_fill(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
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
    ComposerResponse response;
    ComposerStatus called = request ? cli_call(composer_api_fill, request, &response) : COMPOSER_ERR_ALLOC;
    if (!request) {
        cJSON_Delete(plan);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    if (called == COMPOSER_ERR_UNSATISFIABLE && response.json && !response.error.path[0]) {
        status = cli_report_stuck(&response.error, &response);
    } else if (called != COMPOSER_OK) {
        status = cli_fail_plan_api(called, &response.error, options.input, &held);
    } else {
        cJSON *document = cJSON_Parse(response.json);
        const char *files[5];
        size_t file_count = 0;
        status = document ? write_outputs(&options, &response, document, files, &file_count)
                          : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        if (status == 0) {
            cli_print_notices(document);
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
    composer_response_destroy(&response);
    return status;
}
