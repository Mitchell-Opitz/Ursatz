#include "develop.h"

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

#define EXIT_USAGE CLI_EXIT_USAGE
#define EXIT_WRITE CLI_EXIT_WRITE

#define DEFAULT_SUGGEST 5
#define MAX_SUGGEST 16

typedef struct {
    const char *input;
    const char *op_text;
    const char *seed_text;
    const char *suggest_text;
    const char *meter_text;
    const char *key_text;
    const char *format_text;
    const char *midi_path;
    const char *musicxml_path;
    const char *by_text;
    const char *degree_text;
    const char *axis_text;
    const char *interval_text;
    const char *amount_text;
    const char *position_text;
    const char *length_text;
    const char *min_text;
    const char *drop_text;
    const char *ratio_text;
    const char *tempo_text;
    bool tonal;
    bool keep_contour;
    bool keep_rhythm;
    bool plain_midi;
} Options;


/* --- reading the options -------------------------------------------------- */

static const CliOptionRow ROWS[] = {
    {"--op", offsetof(Options, op_text), false},
    {"--seed", offsetof(Options, seed_text), false},
    {"--suggest", offsetof(Options, suggest_text), false},
    {"--meter", offsetof(Options, meter_text), false},
    {"--key", offsetof(Options, key_text), false},
    {"--input-format", offsetof(Options, format_text), false},
    {"--midi", offsetof(Options, midi_path), false},
    {"--musicxml", offsetof(Options, musicxml_path), false},
    {"--tempo", offsetof(Options, tempo_text), false},
    {"--plain-midi", offsetof(Options, plain_midi), true},
    {"--by", offsetof(Options, by_text), false},
    {"--degree", offsetof(Options, degree_text), false},
    {"--axis", offsetof(Options, axis_text), false},
    {"--interval", offsetof(Options, interval_text), false},
    {"--amount", offsetof(Options, amount_text), false},
    {"--position", offsetof(Options, position_text), false},
    {"--length", offsetof(Options, length_text), false},
    {"--min", offsetof(Options, min_text), false},
    {"--drop", offsetof(Options, drop_text), false},
    {"--ratio", offsetof(Options, ratio_text), false},
    {"--tonal", offsetof(Options, tonal), true},
    {"--keep-contour", offsetof(Options, keep_contour), true},
    {"--keep-rhythm", offsetof(Options, keep_rhythm), true},
};

static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->input = args->input_count > 0 ? args->inputs[0] : NULL;
    cli_options_read(args, ROWS, sizeof(ROWS) / sizeof(ROWS[0]), options);
    return 0;
}

/* The request: the motive and the members of the operation. */
static cJSON *build_request(const Options *options, cJSON *request) {
    bool ok = cJSON_AddStringToObject(request, "op", options->op_text) &&
              (!options->seed_text || cJSON_AddStringToObject(request, "seed", options->seed_text)) &&
              (!options->suggest_text || cJSON_AddStringToObject(request, "suggest", options->suggest_text)) &&
              (!options->by_text || cJSON_AddStringToObject(request, "by", options->by_text)) &&
              (!options->degree_text || cJSON_AddStringToObject(request, "degree", options->degree_text)) &&
              (!options->axis_text || cJSON_AddStringToObject(request, "axis", options->axis_text)) &&
              (!options->interval_text || cJSON_AddStringToObject(request, "interval", options->interval_text)) &&
              (!options->amount_text || cJSON_AddStringToObject(request, "amount", options->amount_text)) &&
              (!options->position_text || cJSON_AddStringToObject(request, "position", options->position_text)) &&
              (!options->length_text || cJSON_AddStringToObject(request, "length", options->length_text)) &&
              (!options->min_text || cJSON_AddStringToObject(request, "min", options->min_text)) &&
              (!options->drop_text || cJSON_AddStringToObject(request, "drop", options->drop_text)) &&
              (!options->ratio_text || cJSON_AddStringToObject(request, "ratio", options->ratio_text)) &&
              (!options->tonal || cJSON_AddTrueToObject(request, "tonal")) &&
              (!options->keep_contour || cJSON_AddTrueToObject(request, "keep_contour")) &&
              (!options->keep_rhythm || cJSON_AddTrueToObject(request, "keep_rhythm")) &&
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

/* Writes one file of the command; the failure is "<what> export failed". */
static int write_file(const char *what, const char *path, const void *data, size_t size) {
    if (!cli_write_bytes(path, data, size)) {
        return cli_fail(EXIT_WRITE, "%s export failed: cannot write '%s'", what, path);
    }
    printf("%s written to %s\n", what, path);
    return 0;
}

int run_develop(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    if (!options.input) {
        return cli_needs("develop", "an input file", true);
    }
    if (!options.op_text) {
        return cli_fail(EXIT_USAGE, "develop needs --op: restate, invert, retrograde, interval, fragment, liquidate, "
                                "augment or diminish");
    }

    MotiveInput input;
    memset(&input, 0, sizeof(input));
    input.path = options.input;
    input.meter_text = options.meter_text;
    input.key_text = options.key_text;
    input.format_text = options.format_text;
    cJSON *motive = NULL;
    MotiveFile file;
    memset(&file, 0, sizeof file);
    status = motive_input_text_request(&input, &motive, &file);
    if (status != 0) {
        return status;
    }
    cJSON *request = build_request(&options, motive);
    if (!request) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    CliResponse response;
    UrsatzStatus called = cli_call("develop", request, &response);
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
    printf("Source: %s\n", cli_json_string(document, "source"));
    printf("Operation: %s (seed %.0f)\n", cli_json_string(document, "operation"), cli_json_number(document, "seed"));
    const cJSON *candidate = NULL;
    cJSON_ArrayForEach(candidate, cJSON_GetObjectItemCaseSensitive(document, "candidates")) {
        double num = cli_json_number(candidate, "score_numerator");
        double den = cli_json_number(candidate, "score_denominator");
        printf("%.0f. %s  score %.0f/%.0f (%.0f%%)\n   %s\n", cli_json_number(candidate, "index"),
               cli_json_string(candidate, "label"), num, den, 100.0 * num / den, cli_json_string(candidate, "text"));
    }
    if (options.midi_path) {
        status = write_file("MIDI", options.midi_path, response.midi, response.midi_size);
    }
    if (status == 0 && options.musicxml_path) {
        status = write_file("MusicXML", options.musicxml_path, response.musicxml, response.musicxml ? strlen(response.musicxml) : 0);
    }
    cJSON_Delete(document);
    cli_response_destroy(&response);
    return status;
}
