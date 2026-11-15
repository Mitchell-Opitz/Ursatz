#include "variations.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <ursatz_composer/composer_api.h>

#include "cli_options.h"
#include "workflow.h"

/* The most versions one command keeps (VARIATIONS_MAX). */
#define VARIATIONS_MAX 16

typedef struct {
    bool help;
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

static void print_harmonize_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", HARMONIZE_USAGE_LINE + 7);
    fprintf(out, "Writes several distinct harmonizations of a melody that is given. The whole melody of the section must be\n");
    fprintf(out, "Given (a plan made with --given-melody for every bar of the section); its harmony, and what is written from\n");
    fprintf(out, "the harmony (the bass and the voices between), is written again as variations does, on a copy of the plan\n");
    fprintf(out, "for the seeds N, N+1, ..., at most four times --count of them. A version is kept when it sounds different\n");
    fprintf(out, "from the plan and from the versions kept before. Every melody note is kept. The plan file is not changed.\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --section <letter>   the section with that form letter (\"A\", \"B\", \"A'\") whose melody is given\n");
    fprintf(out, "  --count <n>          how many versions to keep, 1 to %d\n", VARIATIONS_MAX);
    fprintf(out, "  --seed <n>           whole number from 0 for the first seed tried. Default: from the clock\n");
    fprintf(out, "  --out-dir <dir>      where variation-<i>.json (the plan) is written, i from 1; the directory is created\n");
    fprintf(out, "  --midi               also write variation-<i>.mid\n");
    fprintf(out, "  --musicxml           also write variation-<i>.musicxml\n");
    fprintf(out, "  --tempo <bpm>        MIDI and MusicXML: write a tempo, quarter notes per minute from 20 to 400 (a whole number or\n");
    fprintf(out, "                       a fraction like 195/2). Default: none, the file default of 120 applies\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --format <text|json> how to print the result on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 at least one version was kept; 1 usage or an invalid argument; 2 none was kept, the plan\n");
    fprintf(out, "cannot be read, the section is not found or has no given melody (nothing is written); 3 an output file\n");
    fprintf(out, "could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer harmonize piece.json --section A --count 4 --seed 3 --out-dir harmonizations --midi\n");
}

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", VARIATIONS_USAGE_LINE + 7);
    fprintf(out, "Writes several distinct versions of one node of a filled plan. The node is written again (as regenerate\n");
    fprintf(out, "writes it) on a copy of the plan for the seeds N, N+1, ..., at most four times --count of them; a version\n");
    fprintf(out, "is kept when it sounds different from the plan and from the versions kept before. A seed that gets stuck\n");
    fprintf(out, "is skipped. The plan file is not changed. Given material is never changed.\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --node <id>          the node's id in the plan file (composer.id)\n");
    fprintf(out, "  --section <letter>   the section with that form letter (\"A\", \"B\", \"A'\") instead of --node\n");
    fprintf(out, "  --lane <lane>        harmony, bass, melody or inner: only that lane of the node and of what is under it\n");
    fprintf(out, "  --count <n>          how many versions to keep, 1 to %d\n", VARIATIONS_MAX);
    fprintf(out, "  --seed <n>           whole number from 0 for the first seed tried. Default: from the clock. The seed of\n");
    fprintf(out, "                       every version is printed\n");
    fprintf(out, "  --out-dir <dir>      where variation-<i>.json (the plan) is written, i from 1; the directory is created\n");
    fprintf(out, "  --midi               also write variation-<i>.mid\n");
    fprintf(out, "  --musicxml           also write variation-<i>.musicxml\n");
    fprintf(out, "  --tempo <bpm>        MIDI and MusicXML: write a tempo, quarter notes per minute from 20 to 400 (a whole number or\n");
    fprintf(out, "                       a fraction like 195/2). Default: none, the file default of 120 applies\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --format <text|json> how to print the result on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 at least one version was kept; 1 usage or an invalid argument; 2 none was kept, the plan\n");
    fprintf(out, "cannot be read, the node is not found, is Given or cannot be regenerated (nothing is written); 3 an output\n");
    fprintf(out, "file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer variations piece.json --section B --count 4 --seed 3 --out-dir variations --midi\n");
}

static int read_options(int argc, char **argv, Options *options, bool harmonize) {
    const char *command = harmonize ? "harmonize" : "variations";
    memset(options, 0, sizeof(*options));
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--midi") == 0) {
            options->midi = true;
        } else if (strcmp(arg, "--musicxml") == 0) {
            options->musicxml = true;
        } else if (strcmp(arg, "--plain-midi") == 0) {
            options->plain_midi = true;
        } else if (strcmp(arg, "--tempo") == 0) {
            target = &options->tempo_text;
        } else if (!harmonize && strcmp(arg, "--node") == 0) {
            target = &options->target.node_text;
        } else if (strcmp(arg, "--section") == 0) {
            target = &options->target.section_text;
        } else if (!harmonize && strcmp(arg, "--lane") == 0) {
            target = &options->target.lane_text;
        } else if (strcmp(arg, "--count") == 0) {
            target = &options->count_text;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed_text;
        } else if (strcmp(arg, "--out-dir") == 0) {
            target = &options->out_dir;
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

static int check_options(Options *options, bool harmonize) {
    const char *command = harmonize ? "harmonize" : "variations";
    const char *usage = harmonize ? HARMONIZE_USAGE_LINE : VARIATIONS_USAGE_LINE;
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
        ComposerResponse midi;
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
        composer_response_destroy(&midi);
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
static int run_command(int argc, char **argv, bool harmonize) {
    Options options;
    int status = read_options(argc, argv, &options, harmonize);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        harmonize ? print_harmonize_help(stdout) : print_help(stdout);
        return 0;
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
    ComposerResponse response;
    ComposerStatus called = cli_call(harmonize ? composer_api_harmonize : composer_api_variations, request, &response);
    if (called != COMPOSER_OK) {
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
    composer_response_destroy(&response);
    return status;
}

int run_variations(int argc, char **argv) {
    return run_command(argc, argv, false);
}

int run_harmonize(int argc, char **argv) {
    return run_command(argc, argv, true);
}
