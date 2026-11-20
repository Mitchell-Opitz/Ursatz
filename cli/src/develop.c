#include "develop.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_tools.h"

#include "cli_options.h"
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
    bool help;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", DEVELOP_USAGE_LINE + 7);
    fprintf(out, "Makes motives related to the one in <motive-input> (a MIDI file, a MusicXML file, a note list or a\n");
    fprintf(out, "motive JSON file) and prints them best first, each as a note list with its recognizability score.\n\n");
    fprintf(out, "  --op <name>          restate, invert, retrograde, interval, fragment, liquidate, augment, diminish\n");
    fprintf(out, "  --seed <n>           whole number, 0 to 4294967295 (default 0); the same seed gives the same output\n");
    fprintf(out, "  --suggest <n>        how many candidates to print, 1 to %d (default %d)\n", MAX_SUGGEST, DEFAULT_SUGGEST);
    fprintf(out, "  --meter <n/m>, --key <key>, --format <auto|midi|musicxml|notes|json>   as for the motive command\n");
    fprintf(out, "  --midi <file>, --musicxml <file>   write the source and the best candidate\n");
    fprintf(out, "  --tempo <bpm>        write a tempo in both files, quarter notes per minute from 20 to 400 (a whole number or\n");
    fprintf(out, "                       a fraction like 195/2); --plain-midi: no track names and no section markers in the MIDI\n\n");
    fprintf(out, "Options by operation (the operation chooses at random from what is left open):\n");
    fprintf(out, "  restate     --by <n> (semitones; steps with --tonal), --degree <n> (move the first note to scale\n");
    fprintf(out, "              degree n, tonal), --tonal;  with none, the exact repetition and transpositions\n");
    fprintf(out, "  invert      --axis <pitch> (D4 or a MIDI number; the first note by default), --tonal\n");
    fprintf(out, "  retrograde  none\n");
    fprintf(out, "  interval    --interval <n> (1-based; every one by default), --amount <n> (semitones, negative to\n");
    fprintf(out, "              narrow), --keep-contour\n");
    fprintf(out, "  fragment    --position <leading|trailing|middle>, --length <n>, --min <n>\n");
    fprintf(out, "  liquidate   --drop <n>, --keep-rhythm, --min <n>\n");
    fprintf(out, "  augment     --ratio <n:m> (2:1 by default)\n");
    fprintf(out, "  diminish    --ratio <n:m> (1:2 by default)\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the input cannot be loaded or nothing can be\n");
    fprintf(out, "developed as asked; 3 an output file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz develop theme.txt --op restate --tonal --meter 4/4 --key \"D minor\" --suggest 3\n");
}

/* --- reading the options -------------------------------------------------- */

static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->help = args->help;
    options->input = args->input_count > 0 ? args->inputs[0] : NULL;
    options->op_text = cli_value(args, "--op");
    options->seed_text = cli_value(args, "--seed");
    options->suggest_text = cli_value(args, "--suggest");
    options->meter_text = cli_value(args, "--meter");
    options->key_text = cli_value(args, "--key");
    options->format_text = cli_value(args, "--input-format");
    options->midi_path = cli_value(args, "--midi");
    options->musicxml_path = cli_value(args, "--musicxml");
    options->tempo_text = cli_value(args, "--tempo");
    options->plain_midi = cli_switch(args, "--plain-midi");
    options->by_text = cli_value(args, "--by");
    options->degree_text = cli_value(args, "--degree");
    options->axis_text = cli_value(args, "--axis");
    options->interval_text = cli_value(args, "--interval");
    options->amount_text = cli_value(args, "--amount");
    options->position_text = cli_value(args, "--position");
    options->length_text = cli_value(args, "--length");
    options->min_text = cli_value(args, "--min");
    options->drop_text = cli_value(args, "--drop");
    options->ratio_text = cli_value(args, "--ratio");
    options->tonal = cli_switch(args, "--tonal");
    options->keep_contour = cli_switch(args, "--keep-contour");
    options->keep_rhythm = cli_switch(args, "--keep-rhythm");
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
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    if (!options.input) {
        return cli_fail(EXIT_USAGE, "develop needs an input file\n%s", DEVELOP_USAGE_LINE);
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
