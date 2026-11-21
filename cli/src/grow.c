#include "grow.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_options.h"
#include "cli_params.h"
#include "grow_text.h"
#include "motive_extras.h"
#include "motive_input.h"
#include "workflow.h"

#include "cli_tools.h"

#define GROW_MAX_SEED_TRIES 16
#define GROW_MAX_SUGGESTIONS 16

typedef struct {
    const char *input;
    const char *from_corpus;
    const char *like_path;
    const char *into;
    const char *hybrid;
    const char *key;
    const char *material;
    const char *meter;
    const char *bars;
    const char *cadence;
    const char *first_cadence;
    const char *accompaniment;
    const char *texture;
    const char *style_weight;
    bool voicing;
    const char *accompaniment_motive_path;
    const char *accompaniment_format_text;
    const char *accompaniment_pickup_text;
    bool check;
    const char *style_path;
    const char *blend_path;
    CliStyleWithout without;
    const char *blend_weight_text;
    const char *blend_weights_text;
    const char *seed_text;
    const char *suggest_text;
    const char *out_path;
    const char *midi_path;
    const char *musicxml_path;
    const char *tempo_text;
    bool plain_midi;
    const char *explain_path;
    const char *format_text;
    MotiveExtras extras;
    bool json;
} Options;


/* --- reading the options -------------------------------------------------- */

static const CliOptionRow ROWS[] = {
    {"--from-corpus", offsetof(Options, from_corpus), false},
    {"--like", offsetof(Options, like_path), false},
    {"--into", offsetof(Options, into), false},
    {"--hybrid", offsetof(Options, hybrid), false},
    {"--key", offsetof(Options, key), false},
    {"--material", offsetof(Options, material), false},
    {"--meter", offsetof(Options, meter), false},
    {"--bars", offsetof(Options, bars), false},
    {"--cadence", offsetof(Options, cadence), false},
    {"--first-cadence", offsetof(Options, first_cadence), false},
    {"--accompaniment", offsetof(Options, accompaniment), false},
    {"--accompaniment-motive", offsetof(Options, accompaniment_motive_path), false},
    {"--accompaniment-format", offsetof(Options, accompaniment_format_text), false},
    {"--accompaniment-pickup", offsetof(Options, accompaniment_pickup_text), false},
    {"--texture", offsetof(Options, texture), false},
    {"--voicing", offsetof(Options, voicing), true},
    {"--check", offsetof(Options, check), true},
    {"--style", offsetof(Options, style_path), false},
    {"--style-weight", offsetof(Options, style_weight), false},
    {"--blend", offsetof(Options, blend_path), false},
    {"--blend-weight", offsetof(Options, blend_weight_text), false},
    {"--blend-weights", offsetof(Options, blend_weights_text), false},
    {"--seed", offsetof(Options, seed_text), false},
    {"--suggest", offsetof(Options, suggest_text), false},
    {"--out", offsetof(Options, out_path), false},
    {"--midi", offsetof(Options, midi_path), false},
    {"--musicxml", offsetof(Options, musicxml_path), false},
    {"--tempo", offsetof(Options, tempo_text), false},
    {"--plain-midi", offsetof(Options, plain_midi), true},
    {"--explain", offsetof(Options, explain_path), false},
    {"--format", offsetof(Options, format_text), false},
};

static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->input = args->input_count > 0 ? args->inputs[0] : NULL;
    cli_options_read(args, ROWS, sizeof(ROWS) / sizeof(ROWS[0]), options);
    int extras_status = motive_extras_read(&options->extras, args, false);
    if (extras_status != 0) {
        return extras_status;
    }
    return cli_style_without_read(&options->without, args);
}

/* The checks of the command line itself; the values are checked by the composer API. */
static int check_options(Options *options) {
    if (options->like_path && !options->from_corpus) {
        return cli_fail(CLI_EXIT_USAGE, "--like needs --from-corpus");
    }
    if (options->from_corpus && !options->like_path) {
        return cli_fail(CLI_EXIT_USAGE, "--from-corpus needs --like <file>");
    }
    if (options->from_corpus && options->input) {
        return cli_fail(CLI_EXIT_USAGE, "give a motive input or --from-corpus, not both");
    }
    if (!options->input && !options->from_corpus) {
        return cli_needs("grow", "a motive input", true);
    }
    if (!options->into || !options->key || !options->meter) {
        return cli_needs("grow", "--into, --key and --meter", false);
    }
    if ((options->accompaniment_format_text || options->accompaniment_pickup_text) && !options->accompaniment_motive_path) {
        return cli_fail(CLI_EXIT_USAGE, "--accompaniment-format and --accompaniment-pickup need --accompaniment-motive <file>");
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

/* The explanation file is JSON when its name ends in .json, else text. */
static bool explain_as_json(const char *path) {
    size_t length = strlen(path);
    return length >= 5 && cli_equals_ignore_case(path + length - 5, ".json");
}

/* Adds the options of the command to the request, the motive's members already in it. */
static bool add_members(const Options *options, cJSON *request) {
    bool ok = cJSON_AddStringToObject(request, "into", options->into) && cJSON_AddStringToObject(request, "meter", options->meter) &&
              (!options->hybrid || cJSON_AddStringToObject(request, "hybrid", options->hybrid)) &&
              (!options->material || cJSON_AddStringToObject(request, "material", options->material)) &&
              (!options->bars || cJSON_AddStringToObject(request, "bars", options->bars)) &&
              (!options->cadence || cJSON_AddStringToObject(request, "cadence", options->cadence)) &&
              (!options->first_cadence || cJSON_AddStringToObject(request, "first_cadence", options->first_cadence)) &&
              (!options->accompaniment || cJSON_AddStringToObject(request, "accompaniment", options->accompaniment)) &&
              (!options->texture || cJSON_AddStringToObject(request, "texture", options->texture)) &&
              (!options->voicing || cJSON_AddTrueToObject(request, "voicing")) &&
              (!options->check || cJSON_AddTrueToObject(request, "check")) &&
              (!options->style_path || cJSON_AddStringToObject(request, "style", options->style_path)) &&
              (!options->style_weight || cJSON_AddStringToObject(request, "style_weight", options->style_weight)) &&
              (!options->blend_path || cJSON_AddStringToObject(request, "blend", options->blend_path)) &&
              (!options->blend_weight_text || cJSON_AddStringToObject(request, "blend_weight", options->blend_weight_text)) &&
              (!options->blend_weights_text || cJSON_AddStringToObject(request, "blend_weights", options->blend_weights_text)) &&
              (!options->seed_text || cJSON_AddStringToObject(request, "seed", options->seed_text)) &&
              (!options->suggest_text || cJSON_AddStringToObject(request, "suggest", options->suggest_text)) &&
              (!options->tempo_text || cJSON_AddStringToObject(request, "tempo", options->tempo_text)) &&
              (!options->plain_midi || cJSON_AddTrueToObject(request, "plain_midi")) &&
              (!options->midi_path || cJSON_AddTrueToObject(request, "midi")) &&
              (!options->musicxml_path || cJSON_AddTrueToObject(request, "musicxml")) &&
              (!(options->out_path || options->midi_path) || cJSON_AddTrueToObject(request, "plans")) &&
              (!options->explain_path ||
               cJSON_AddStringToObject(request, "explain", explain_as_json(options->explain_path) ? "json" : "text"));
    if (ok && options->without.count > 0) {
        cJSON *list = cJSON_AddArrayToObject(request, "style_without");
        for (size_t i = 0; list && i < options->without.count; i++) {
            ok = cJSON_AddItemToArray(list, cJSON_CreateString(options->without.names[i]));
        }
        ok = ok && list;
    }
    return ok;
}

/* --- the command ---------------------------------------------------------------------------------- */

int run_grow(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    status = check_options(&options);
    if (status != 0) {
        return status;
    }

    /* The motive: a file read into the request, or a passage of a corpus. A tonic alone with --material is completed for a MIDI
       file, which is read before the API sees the brief. */
    cJSON *request = NULL;
    MotiveFile file;
    MotiveFile accompaniment_file;
    char key_with_mode[96];
    memset(&file, 0, sizeof file);
    memset(&accompaniment_file, 0, sizeof accompaniment_file);
    if (options.from_corpus) {
        /* The passages are found here, by the Analyzer, and given to the API inline (R-619). */
        cJSON *passages = cli_corpus_passages(options.from_corpus, options.like_path, NULL, &status);
        if (!passages) {
            return status;
        }
        request = cJSON_CreateObject();
        if (!request || !cJSON_AddStringToObject(request, "key", options.key) ||
            !cJSON_AddItemToObject(request, "corpus_passages", passages)) {
            cJSON_Delete(request);
            cJSON_Delete(passages);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
        if (motive_extras_given(&options.extras) && !motive_extras_add_members(&options.extras, request)) {
            cJSON_Delete(request);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
    } else {
        MotiveInput input;
        memset(&input, 0, sizeof(input));
        input.path = options.input;
        input.meter_text = options.meter;
        input.key_text = options.key;
        input.extras = options.extras;
        if (options.material && !strpbrk(options.key, " \t")) {
            bool concrete = cli_equals_ignore_case(options.material, "major") || cli_equals_ignore_case(options.material, "minor");
            input.key_text = concrete && snprintf(key_with_mode, sizeof key_with_mode, "%s %s", options.key, options.material) <
                                             (int)sizeof key_with_mode
                                 ? key_with_mode
                                 : NULL;
        }
        status = motive_input_text_request(&input, &request, &file);
        if (status != 0) {
            return status;
        }
        cJSON_DeleteItemFromObjectCaseSensitive(request, "key");
        if (!cJSON_AddStringToObject(request, "key", options.key)) {
            cJSON_Delete(request);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
    }
    if (options.accompaniment_motive_path) {
        MotiveInput input;
        cJSON *accompaniment = NULL;
        memset(&input, 0, sizeof(input));
        input.path = options.accompaniment_motive_path;
        input.meter_text = options.meter;
        input.format_text = options.accompaniment_format_text;
        input.pickup_text = options.accompaniment_pickup_text;
        input.extras.role_text = "accompaniment";
        status = motive_input_text_request(&input, &accompaniment, &accompaniment_file);
        if (status != 0 || !cJSON_AddItemToObject(request, "accompaniment_motive", accompaniment)) {
            cJSON_Delete(request);
            return status != 0 ? status : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
    }
    if (!add_members(&options, request)) {
        cJSON_Delete(request);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }

    CliResponse response;
    UrsatzStatus called = cli_call("grow", request, &response);
    if (called != URSATZ_OK) {
        /* A bad option is reported before a file that cannot be read; the first file that could not be read is the one named. */
        MotiveFile *failed = file.read_status != 0 ? &file : &accompaniment_file;
        MotiveInputNames names = {options.input, NULL, NULL, options.accompaniment_motive_path};
        status = motive_input_fail(failed, called, &response.error, &names);
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
        char text[1400];
        fprintf(stderr, strncmp(notice->valuestring, "only ", 5) == 0 ? "Note: %s\n" : "Warning: %s\n",
                cli_corpus_notice_text(notice->valuestring, options.from_corpus, text, sizeof text));
    }
    cJSON_DeleteItemFromObjectCaseSensitive(document, "notices");

    const cJSON *entries[GROW_MAX_SUGGESTIONS];
    size_t count = 0;
    const cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
    const cJSON *theme = NULL;
    cJSON_ArrayForEach(theme, themes) {
        if (count < GROW_MAX_SUGGESTIONS) {
            entries[count++] = theme;
        }
    }
    CliAlternativePaths paths = {options.out_path, options.midi_path, options.musicxml_path, options.explain_path};
    CliWrittenFiles written;
    memset(&written, 0, sizeof written);
    status = cli_write_alternatives(&paths, entries, count, &response, options.tempo_text, options.plain_midi, &written);
    if (status == 0) {
        if (options.json) {
            cJSON_ArrayForEach(theme, themes) {
                cli_strip_entry_files((cJSON *)theme);
            }
            cli_print_document_with_files(document, &written);
        } else {
            const char *files[4 * GROW_MAX_SUGGESTIONS];
            cli_print_grow_text(document, options.suggest_text != NULL, cli_written_list(&written, files, 4 * GROW_MAX_SUGGESTIONS),
                                written.count);
        }
        cli_written_free(&written);
    }
    cJSON_Delete(document);
    cli_response_destroy(&response);
    return status;
}
