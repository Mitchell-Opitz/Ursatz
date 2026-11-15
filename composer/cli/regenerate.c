#include "regenerate.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/composer_api.h>

#include "cli_options.h"
#include "workflow.h"

typedef struct {
    bool help;
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

static void print_lock_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", LOCK_USAGE_LINE + 7);
    fprintf(out, "Locks material of a plan as Given: a node and everything under it (a section, a phrase, an idea), or, with\n");
    fprintf(out, "--lane, one lane of it. Nothing composes over Given material, and it is never regenerated. Locking is for\n");
    fprintf(out, "good in the plan; to unlock, go back to an earlier plan file.\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --node <id>          the node's id in the plan file (composer.id)\n");
    fprintf(out, "  --section <letter>   the section with that form letter (\"A\", \"B\", \"A'\") instead of --node\n");
    fprintf(out, "  --lane <lane>        harmony, bass, melody or inner: only that lane of the node and of what is under it\n");
    fprintf(out, "  --out <plan.json>    where the plan is written\n");
    fprintf(out, "  --format <text|json> how to print the result on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Only written material can be locked: a node with a lane span still open is refused.\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the plan cannot be read, the node is not found or has\n");
    fprintf(out, "open spans; 3 the output file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer lock piece.json --section A --out piece-locked.json\n");
}

static void print_regenerate_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", REGENERATE_USAGE_LINE + 7);
    fprintf(out, "Writes one node of a filled plan again and nothing but what depends on it. A section is built again whole\n");
    fprintf(out, "(its form and its returns too); a phrase or an idea keeps its shape and has its lanes written again; with\n");
    fprintf(out, "--lane, that lane of the node and what is written from it (the bass and melody over new chords, the next\n");
    fprintf(out, "chord or idea that joins them). Given material is never changed.\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --node <id>          the node's id in the plan file (composer.id)\n");
    fprintf(out, "  --section <letter>   the section with that form letter (\"A\", \"B\", \"A'\") instead of --node\n");
    fprintf(out, "  --lane <lane>        harmony, bass, melody or inner: only that lane of the node and of what is under it\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same plan, node and seed give the same output. Default: from\n");
    fprintf(out, "                       the clock. The seed used is always printed (if a seed gets stuck, the next seeds are\n");
    fprintf(out, "                       tried, up to %d, and the one that worked is printed)\n", CLI_MAX_SEED_TRIES);
    fprintf(out, "  --out <plan.json>    where the plan is written\n");
    fprintf(out, "  --midi <file>        write the piece as a MIDI file\n");
    fprintf(out, "  --musicxml <file>    write the piece as MusicXML\n");
    fprintf(out, "  --tempo <bpm>        MIDI and MusicXML: write a tempo, quarter notes per minute from 20 to 400 (a whole number or\n");
    fprintf(out, "                       a fraction like 195/2). Default: none, the file default of 120 applies\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --format <text|json> how to print the result on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "What was reopened, the seed, the form and cadences heard in each section written again, and the returns are\n");
    fprintf(out, "printed. The ids in a plan file run from 1: a plan written after a section was built again numbers its nodes\n");
    fprintf(out, "again, so read them from the file you are using.\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the plan cannot be read, the node is not found, is\n");
    fprintf(out, "Given or cannot be regenerated, or no seed completes the plan; 3 an output file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer regenerate piece.json --section B --seed 3 --out piece-2.json --midi piece-2.mid\n");
}

static int read_options(int argc, char **argv, Options *options, bool regenerate) {
    memset(options, 0, sizeof(*options));
    const char *command = regenerate ? "regenerate" : "lock";
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--node") == 0) {
            target = &options->target.node_text;
        } else if (strcmp(arg, "--section") == 0) {
            target = &options->target.section_text;
        } else if (strcmp(arg, "--lane") == 0) {
            target = &options->target.lane_text;
        } else if (strcmp(arg, "--out") == 0) {
            target = &options->out_path;
        } else if (regenerate && strcmp(arg, "--seed") == 0) {
            target = &options->seed_text;
        } else if (regenerate && strcmp(arg, "--midi") == 0) {
            target = &options->midi_path;
        } else if (regenerate && strcmp(arg, "--musicxml") == 0) {
            target = &options->musicxml_path;
        } else if (regenerate && strcmp(arg, "--tempo") == 0) {
            target = &options->tempo_text;
        } else if (regenerate && strcmp(arg, "--plain-midi") == 0) {
            options->plain_midi = true;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format_text;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer %s --help)", arg, command);
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

static int check_options(Options *options, bool regenerate) {
    const char *usage = regenerate ? REGENERATE_USAGE_LINE : LOCK_USAGE_LINE;
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

int run_lock(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options, false);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_lock_help(stdout);
        return 0;
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
    ComposerResponse response;
    ComposerStatus called = cli_call(composer_api_lock, request, &response);
    if (called != COMPOSER_OK) {
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
    composer_response_destroy(&response);
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

int run_regenerate(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options, true);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_regenerate_help(stdout);
        return 0;
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
    ComposerResponse response;
    ComposerStatus called = cli_call(composer_api_regenerate, request, &response);
    if (called == COMPOSER_ERR_UNSATISFIABLE && response.json && !response.error.path[0]) {
        status = cli_report_stuck(&response.error, &response);
    } else if (called != COMPOSER_OK) {
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
    composer_response_destroy(&response);
    return status;
}
