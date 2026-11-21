#include "counterpoint.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_tools.h"

#include "cli_options.h"
#include "cli_params.h"
#include "motive_input.h"
#include "workflow.h"

#define MAX_SUGGESTIONS 16

typedef struct {
    const char *input;
    const char *species_text;
    bool above;
    bool below;
    const char *key_text;
    const char *material_text;
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


/* --- reading the options -------------------------------------------------- */

static const CliOptionRow ROWS[] = {
    {"--above", offsetof(Options, above), true},
    {"--below", offsetof(Options, below), true},
    {"--species", offsetof(Options, species_text), false},
    {"--key", offsetof(Options, key_text), false},
    {"--material", offsetof(Options, material_text), false},
    {"--meter", offsetof(Options, meter_text), false},
    {"--seed", offsetof(Options, seed_text), false},
    {"--suggest", offsetof(Options, suggest_text), false},
    {"--out", offsetof(Options, out_path), false},
    {"--midi", offsetof(Options, midi_path), false},
    {"--musicxml", offsetof(Options, musicxml_path), false},
    {"--tempo", offsetof(Options, tempo_text), false},
    {"--plain-midi", offsetof(Options, plain_midi), true},
    {"--format", offsetof(Options, format_text), false},
};

static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->input = args->input_count > 0 ? args->inputs[0] : NULL;
    cli_options_read(args, ROWS, sizeof(ROWS) / sizeof(ROWS[0]), options);
    return 0;
}

static int check_options(Options *options) {
    if (!options->input) {
        return cli_needs("counterpoint", "a line", true);
    }
    if (!options->species_text || !options->key_text || !options->meter_text) {
        return cli_needs("counterpoint", "--species, --key and --meter", false);
    }
    if (options->above && options->below) {
        return cli_fail(CLI_EXIT_USAGE, "--above and --below cannot both be given");
    }
    if (options->format_text && !cli_equals_ignore_case(options->format_text, "text") && !cli_equals_ignore_case(options->format_text, "json")) {
        return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", options->format_text);
    }
    options->json = options->format_text && cli_equals_ignore_case(options->format_text, "json");
    return 0;
}

/* The request: the line as a motive, and the options of the command as members. */
static cJSON *build_request(const Options *options, cJSON *line) {
    cJSON_DeleteItemFromObjectCaseSensitive(line, "key");
    bool ok = cJSON_AddStringToObject(line, "key", options->key_text) &&
              cJSON_AddStringToObject(line, "species", options->species_text) &&
              (!options->above || cJSON_AddStringToObject(line, "position", "above")) &&
              (!options->material_text || cJSON_AddStringToObject(line, "material", options->material_text)) &&
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

/* The notes of a line as a note list, gaps as rests, a bar line between bars. bar is the length of a bar. names are the pitch names of the
 * notes, in order, from the one pitch-names call of the document. */
static void print_line(const cJSON *notes, char (*names)[8], long long bar_num, long long bar_den) {
    size_t index = 0;
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
        char length[24];
        if (start_den <= 0 || length_den <= 0) {
            /* a time with no denominator cannot be placed: the note is left out, its name is not reused */
            index++;
            continue;
        }
        if (start_num * at_den > at_num * start_den) {
            long long gap_num = start_num * at_den - at_num * start_den;
            long long gap_den = start_den * at_den;
            duration_name(gap_num, gap_den, length, sizeof(length));
            printf(" r:%s", length);
        }
        if (bar_num > 0 && start_num * bar_den % (bar_num * start_den) == 0 && start_num > 0) {
            printf(" |");
        }
        duration_name(length_num, length_den, length, sizeof(length));
        printf(" %s:%s", names[index++], length);
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
    /* The names of every note of the document, line then counter-melody of each result: one pitch-names call. */
    size_t total = 0;
    const cJSON *result = NULL;
    cJSON_ArrayForEach(result, results) {
        total += (size_t)cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(result, "line")) +
                 (size_t)cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(result, "counter_melody"));
    }
    int *midi = calloc(total ? total : 1, sizeof *midi);
    char(*names)[8] = calloc(total ? total : 1, sizeof *names);
    size_t at = 0;
    cJSON_ArrayForEach(result, results) {
        static const char *const LINES[2] = {"line", "counter_melody"};
        for (int l = 0; midi && l < 2; l++) {
            const cJSON *note = NULL;
            cJSON_ArrayForEach(note, cJSON_GetObjectItemCaseSensitive(result, LINES[l])) {
                midi[at++] = (int)cli_json_number(note, "midi_pitch");
            }
        }
    }
    if (!midi || !names) {
        free(midi);
        free(names);
        cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        return;
    }
    cli_pitch_names(midi, total, names);
    free(midi);
    char(*next)[8] = names;
    size_t k = 0;
    cJSON_ArrayForEach(result, results) {
        if (options->suggest_text) {
            printf("\ncounter-melody %zu of %zu, seed %.0f\n", k + 1, count, cli_json_number(result, "seed"));
        } else {
            printf("\nseed %.0f\n", cli_json_number(result, "seed"));
        }
        printf("line:          ");
        const cJSON *line = cJSON_GetObjectItemCaseSensitive(result, "line");
        const cJSON *counter_melody = cJSON_GetObjectItemCaseSensitive(result, "counter_melody");
        print_line(line, next, beats, unit);
        next += cJSON_GetArraySize(line);
        printf("counter-melody:");
        print_line(counter_melody, next, beats, unit);
        next += cJSON_GetArraySize(counter_melody);
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
    free(names);
    for (size_t i = 0; i < written->count; i++) {
        printf("wrote %s\n", written->paths[i]);
    }
}

/* --- the command ---------------------------------------------------------------------------------- */

int run_counterpoint(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
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
    if (options.material_text && !strchr(options.key_text, ' ') &&
        snprintf(key_with_mode, sizeof key_with_mode, "%s %s", options.key_text, options.material_text) < (int)sizeof key_with_mode) {
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
    CliResponse response;
    UrsatzStatus called = cli_call("counterpoint", request, &response);
    if (called != URSATZ_OK) {
        MotiveInputNames names = {options.input, NULL, NULL, NULL};
        status = motive_input_fail(&file, called, &response.error, &names);
        cli_response_destroy(&response);
        return status;
    }
    cJSON *document = cJSON_Parse(response.json);
    if (!document) {
        cli_response_destroy(&response);
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
    cli_response_destroy(&response);
    return status;
}
