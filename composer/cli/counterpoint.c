#include "counterpoint.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/composer_api.h>

#include "cli_options.h"
#include "motive_input.h"
#include "util/util.h"
#include "workflow.h"

#define MAX_SUGGESTIONS 16

typedef struct {
    bool help;
    const char *input;
    const char *species_text;
    bool above;
    bool below;
    const char *key_text;
    const char *mode_text;
    const char *meter_text;
    const char *seed_text;
    const char *suggest_text;
    const char *out_path;
    const char *midi_path;
    const char *musicxml_path;
    const char *tempo_text;
    bool plain_midi;
    const char *format_text;
    bool json;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", COUNTERPOINT_USAGE_LINE + 7);
    fprintf(out, "Writes a counter-melody against <line> (a MIDI file, a MusicXML file, a note list or a motive JSON file)\n");
    fprintf(out, "in the first, second, third or fourth species of strict counterpoint, and checks it with Ursatz's\n");
    fprintf(out, "standard-counterpoint rules.\n\n");
    fprintf(out, "Required:\n");
    fprintf(out, "  --species <1-4>      1 note against 1, 2 against 1, 4 against 1, or syncopated half notes tied over the\n");
    fprintf(out, "                       barline. Species 2 to 4 need a cantus firmus: one whole note a bar, three bars or more,\n");
    fprintf(out, "                       and species 3 a meter of four beats\n");
    fprintf(out, "  --key <key>          tonic and mode in one argument (\"D minor\"), or a tonic with --mode\n");
    fprintf(out, "  --meter <n/m>        the meter of the line, for example 4/4\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --below              the line is the melody and the counter-melody sings below it (default)\n");
    fprintf(out, "  --above              the line is the bass and the counter-melody sings above it\n");
    fprintf(out, "  --mode <major|minor> the mode, when --key gives only a tonic\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same seed gives the same counter-melody. Default: derived\n");
    fprintf(out, "                       from the clock; the seed used is always printed\n");
    fprintf(out, "  --suggest <n>        write n different counter-melodies (1 to %d); with files, number k goes to\n", MAX_SUGGESTIONS);
    fprintf(out, "                       <name>-k.<extension>\n");
    fprintf(out, "  --out <file.json>    write the plan as JSON\n");
    fprintf(out, "  --midi <file>        write the two parts as a MIDI file\n");
    fprintf(out, "  --musicxml <file>    write the two parts as MusicXML\n");
    fprintf(out, "  --tempo <bpm>        MIDI and MusicXML: write a tempo, quarter notes per minute from 20 to 400 (a whole number or\n");
    fprintf(out, "                       a fraction like 195/2). Default: none, the file default of 120 applies\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --format <text|json> how to print the result on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the request cannot be satisfied (the line cannot be\n");
    fprintf(out, "read or is not one the species can be written against, or no counter-melody holds the rules); 3 an output\n");
    fprintf(out, "file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer counterpoint cantus.txt --species 2 --above --key \"D minor\" --meter 4/4 --seed 3 --midi out.mid\n");
}

/* --- reading the options -------------------------------------------------- */

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--above") == 0) {
            options->above = true;
        } else if (strcmp(arg, "--below") == 0) {
            options->below = true;
        } else if (strcmp(arg, "--species") == 0) {
            target = &options->species_text;
        } else if (strcmp(arg, "--key") == 0) {
            target = &options->key_text;
        } else if (strcmp(arg, "--mode") == 0) {
            target = &options->mode_text;
        } else if (strcmp(arg, "--meter") == 0) {
            target = &options->meter_text;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed_text;
        } else if (strcmp(arg, "--suggest") == 0) {
            target = &options->suggest_text;
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
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format_text;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer counterpoint --help)", arg);
        } else if (!options->input) {
            options->input = arg;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unexpected argument '%s'; the input is one file", arg);
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

/* The checks of the command line itself; the values are checked by the composer API. */
static int check_options(Options *options) {
    if (!options->input) {
        return cli_fail(CLI_EXIT_USAGE, "counterpoint needs a line\n%s", COUNTERPOINT_USAGE_LINE);
    }
    if (!options->species_text || !options->key_text || !options->meter_text) {
        return cli_fail(CLI_EXIT_USAGE, "counterpoint needs --species, --key and --meter (see --help)");
    }
    if (options->above && options->below) {
        return cli_fail(CLI_EXIT_USAGE, "--above and --below cannot both be given");
    }
    if (options->format_text && strcmp(options->format_text, "text") != 0 && strcmp(options->format_text, "json") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", options->format_text);
    }
    options->json = options->format_text && strcmp(options->format_text, "json") == 0;
    return 0;
}

/* The request: the line as a motive, and the options of the command as members. */
static cJSON *build_request(const Options *options, cJSON *line) {
    cJSON_DeleteItemFromObjectCaseSensitive(line, "key");
    bool ok = cJSON_AddStringToObject(line, "key", options->key_text) &&
              cJSON_AddStringToObject(line, "species", options->species_text) &&
              (!options->above || cJSON_AddStringToObject(line, "position", "above")) &&
              (!options->mode_text || cJSON_AddStringToObject(line, "mode", options->mode_text)) &&
              (!options->seed_text || cJSON_AddStringToObject(line, "seed", options->seed_text)) &&
              (!options->suggest_text || cJSON_AddStringToObject(line, "suggest", options->suggest_text)) &&
              (!(options->out_path || options->midi_path) || cJSON_AddTrueToObject(line, "plans")) &&
              (!options->midi_path || cJSON_AddTrueToObject(line, "midi")) &&
              (!options->musicxml_path || cJSON_AddTrueToObject(line, "musicxml")) &&
              (!options->tempo_text || cJSON_AddStringToObject(line, "tempo", options->tempo_text)) &&
              (!options->plain_midi || cJSON_AddTrueToObject(line, "plain_midi"));
    if (!ok) {
        cJSON_Delete(line);
        return NULL;
    }
    return line;
}

/* --- printing ----------------------------------------------------------------------------------- */

/* A length as w, h, q, e, s or t with up to two dots, else as a fraction of a whole note. */
static void duration_name(long long num, long long den, char *out, size_t size) {
    static const struct {
        long ticks;
        const char *name;
    } NAMES[] = {{128, "w"}, {192, "w."}, {64, "h"}, {96, "h."}, {112, "h.."}, {32, "q"}, {48, "q."}, {56, "q.."},
                 {16, "e"},  {24, "e."},  {28, "e.."}, {8, "s"}, {12, "s."}, {4, "t"}};
    long long ticks = (num * 128) % den == 0 ? num * 128 / den : -1;
    for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
        if (NAMES[i].ticks == ticks) {
            snprintf(out, size, "%s", NAMES[i].name);
            return;
        }
    }
    snprintf(out, size, "%lld/%lld", num, den);
}

static void pitch_name(int pitch, char *out, size_t size) {
    util_pitch_name(pitch, false, out, size);
}

static long long greatest_common_divisor(long long a, long long b) {
    while (b != 0) {
        long long rest = a % b;
        a = b;
        b = rest;
    }
    return a < 0 ? -a : a;
}

/* The notes of a line as a note list, gaps as rests, a bar line between bars. bar is the length of a bar. */
static void print_line(const cJSON *notes, long long bar_num, long long bar_den) {
    long long at_num = 0;
    long long at_den = 1;
    const cJSON *note = NULL;
    cJSON_ArrayForEach(note, notes) {
        const cJSON *offset = cJSON_GetObjectItemCaseSensitive(note, "offset");
        const cJSON *duration = cJSON_GetObjectItemCaseSensitive(note, "duration");
        long long start_num = (long long)cli_json_number(offset, "num");
        long long start_den = (long long)cli_json_number(offset, "den");
        long long length_num = (long long)cli_json_number(duration, "num");
        long long length_den = (long long)cli_json_number(duration, "den");
        char pitch[16];
        char length[24];
        if (start_num * at_den > at_num * start_den) {
            long long gap_num = start_num * at_den - at_num * start_den;
            long long gap_den = start_den * at_den;
            long long divisor = greatest_common_divisor(gap_num, gap_den);
            gap_num /= divisor;
            gap_den /= divisor;
            duration_name(gap_num, gap_den, length, sizeof(length));
            printf(" r:%s", length);
        }
        if (bar_num > 0 && start_num * bar_den % (bar_num * start_den) == 0 && start_num > 0) {
            printf(" |");
        }
        pitch_name((int)cli_json_number(note, "midi_pitch"), pitch, sizeof(pitch));
        duration_name(length_num, length_den, length, sizeof(length));
        printf(" %s:%s", pitch, length);
        at_num = start_num * length_den + length_num * start_den;
        at_den = start_den * length_den;
    }
    printf("\n");
}

static void print_result_text(const Options *options, const cJSON *document, const CliWrittenFiles *written) {
    const cJSON *results = cJSON_GetObjectItemCaseSensitive(document, "results");
    size_t count = (size_t)cJSON_GetArraySize(results);
    int species = (int)cli_json_number(document, "species");
    long beats = 0;
    long unit = 1;
    sscanf(cli_json_string(document, "meter"), "%ld/%ld", &beats, &unit);
    printf("species %d counterpoint %s the line, key %s, meter %s\n", species, cli_json_string(document, "position"),
           cli_json_string(document, "key"), cli_json_string(document, "meter"));
    size_t k = 0;
    const cJSON *result = NULL;
    cJSON_ArrayForEach(result, results) {
        if (options->suggest_text) {
            printf("\ncounter-melody %zu of %zu, seed %.0f\n", k + 1, count, cli_json_number(result, "seed"));
        } else {
            printf("\nseed %.0f\n", cli_json_number(result, "seed"));
        }
        printf("line:          ");
        print_line(cJSON_GetObjectItemCaseSensitive(result, "line"), beats, unit);
        printf("counter-melody:");
        print_line(cJSON_GetObjectItemCaseSensitive(result, "counter_melody"), beats, unit);
        printf("rules (standard-counterpoint, species %d):\n", species);
        const cJSON *group = NULL;
        cJSON_ArrayForEach(group, cJSON_GetObjectItemCaseSensitive(result, "groups")) {
            bool hard = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(group, "hard_held"));
            bool soft = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(group, "soft_held"));
            printf("  %-18s %s%s\n", cli_json_string(group, "name"), hard ? "held" : "BROKEN",
                   hard && !soft ? " (some advice not followed)" : "");
        }
        k++;
    }
    for (size_t i = 0; i < written->count; i++) {
        printf("wrote %s\n", written->paths[i]);
    }
}

/* --- the command ---------------------------------------------------------------------------------- */

int run_counterpoint(int argc, char **argv) {
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

    /* The line, read in the key and meter; a tonic alone is completed with the mode for a MIDI file. */
    char key_with_mode[96];
    MotiveInput input;
    memset(&input, 0, sizeof(input));
    input.path = options.input;
    input.meter_text = options.meter_text;
    input.key_text = options.key_text;
    if (options.mode_text && !strchr(options.key_text, ' ') &&
        snprintf(key_with_mode, sizeof key_with_mode, "%s %s", options.key_text, options.mode_text) < (int)sizeof key_with_mode) {
        input.key_text = key_with_mode;
    }
    cJSON *line = NULL;
    MotiveFile file;
    memset(&file, 0, sizeof file);
    status = motive_input_text_request(&input, &line, &file);
    if (status != 0) {
        return status;
    }
    cJSON *request = build_request(&options, line);
    if (!request) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    ComposerResponse response;
    ComposerStatus called = cli_call(composer_api_counterpoint, request, &response);
    if (called != COMPOSER_OK) {
        MotiveInputNames names = {options.input, NULL, NULL, NULL};
        status = motive_input_fail(&file, called, &response.error, &names);
        composer_response_destroy(&response);
        return status;
    }
    cJSON *document = cJSON_Parse(response.json);
    if (!document) {
        composer_response_destroy(&response);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    const cJSON *notice = NULL;
    cJSON_ArrayForEach(notice, cJSON_GetObjectItemCaseSensitive(document, "notices")) {
        fprintf(stderr, "Note: %s\n", notice->valuestring);
    }
    cJSON_DeleteItemFromObjectCaseSensitive(document, "notices");

    const cJSON *entries[MAX_SUGGESTIONS];
    size_t count = 0;
    const cJSON *result = NULL;
    cJSON_ArrayForEach(result, cJSON_GetObjectItemCaseSensitive(document, "results")) {
        if (count < MAX_SUGGESTIONS) {
            entries[count++] = result;
        }
    }
    CliAlternativePaths paths = {options.out_path, options.midi_path, options.musicxml_path, NULL};
    CliWrittenFiles written;
    memset(&written, 0, sizeof written);
    status = cli_write_alternatives(&paths, entries, count, &response, options.tempo_text, options.plain_midi, &written);
    if (status == 0) {
        if (options.json) {
            cJSON_ArrayForEach(result, cJSON_GetObjectItemCaseSensitive(document, "results")) {
                cli_strip_entry_files((cJSON *)result);
            }
            cli_print_document_with_files(document, &written);
        } else {
            print_result_text(&options, document, &written);
        }
        cli_written_free(&written);
    }
    cJSON_Delete(document);
    composer_response_destroy(&response);
    return status;
}
