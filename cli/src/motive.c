#include "motive.h"
#include "cli_tools.h"

#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "cli_options.h"
#include "cli_params.h"
#include "motive_extras.h"
#include "motive_input.h"
#include "motive_text.h"

#define EXIT_USAGE 1
#define EXIT_UNLOADABLE 2
#define EXIT_OUTPUT 3

typedef struct {
    const char *input;
    const char *meter_text;
    const char *key_text;
    const char *format_text;
    const char *form_text;
    const char *pickup_text;
    const char *render_key_text;
    const char *midi_path;
    const char *musicxml_path;
    const char *tempo_text;
    MotiveExtras extras;
    const char *tone_texts[CLI_LIST_CAPACITY];
    size_t tone_count;
    bool json;
} Options;


static int fail(int status, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    return status;
}

static const CliOptionRow ROWS[] = {
    {"--meter", offsetof(Options, meter_text), false},
    {"--key", offsetof(Options, key_text), false},
    {"--input-format", offsetof(Options, format_text), false},
    {"--form", offsetof(Options, form_text), false},
    {"--pickup", offsetof(Options, pickup_text), false},
    {"--render-key", offsetof(Options, render_key_text), false},
    {"--midi", offsetof(Options, midi_path), false},
    {"--musicxml", offsetof(Options, musicxml_path), false},
    {"--tempo", offsetof(Options, tempo_text), false},
};

static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->input = args->input_count > 0 ? args->inputs[0] : NULL;
    cli_options_read(args, ROWS, sizeof(ROWS) / sizeof(ROWS[0]), options);
    const char *print_format = cli_value(args, "--format");
    if (print_format) {
        options->json = cli_equals_ignore_case(print_format, "json");
        if (!options->json && !cli_equals_ignore_case(print_format, "text")) {
            return fail(EXIT_USAGE, "--format must be text or json, not '%s'", print_format);
        }
    }
    int extras_status = motive_extras_read(&options->extras, args, false);
    if (extras_status != 0) {
        return extras_status;
    }
    for (size_t i = 0; i < cli_repeat_count(args, "--tone"); i++) {
        if (cli_list_full(options->tone_count, "motive_notes")) {
            return fail(EXIT_USAGE, "at most %ld --tone options", cli_limit("motive_notes"));
        }
        options->tone_texts[options->tone_count++] = cli_repeat_value(args, "--tone", i, NULL);
    }
    return 0;
}

/* Writes the bytes of an output the response holds to the path the user gave. Returns 0, or the status of the failure. */
static int write_output(const char *path, const void *bytes, size_t size) {
    return cli_write_bytes(path, bytes, size) ? 0 : fail(EXIT_OUTPUT, "cannot write '%s': could not write '%s'", path, path);
}

/* Adds the members of the command's own options to the request. False when out of memory. */
static bool add_command_members(cJSON *request, const Options *options, bool render) {
    bool ok = true;
    if (options->tone_count > 0) {
        cJSON *tones = cJSON_AddArrayToObject(request, "tones");
        ok = tones != NULL;
        for (size_t i = 0; ok && i < options->tone_count; i++) {
            cJSON *item = cJSON_CreateString(options->tone_texts[i]);
            ok = item && cJSON_AddItemToArray(tones, item);
            if (!ok) {
                cJSON_Delete(item);
            }
        }
    }
    if (ok && render && options->render_key_text) {
        ok = cJSON_AddStringToObject(request, "render_key", options->render_key_text) != NULL;
    }
    if (ok && render && options->tempo_text) {
        ok = cJSON_AddStringToObject(request, "tempo", options->tempo_text) != NULL;
    }
    if (ok && render && options->midi_path) {
        ok = cJSON_AddBoolToObject(request, "midi", true) != NULL;
    }
    if (ok && render && options->musicxml_path) {
        ok = cJSON_AddBoolToObject(request, "musicxml", true) != NULL;
    }
    return ok;
}

/* The status of a failed API call: a file that could not be made is a write failure, anything else as cli_fail_api. */
static int fail_response(UrsatzStatus status, const UrsatzError *error, const Options *options) {
    if (strcmp(error->path, "/midi") == 0 || strcmp(error->path, "/musicxml") == 0) {
        const char *path = strcmp(error->path, "/midi") == 0 ? options->midi_path : options->musicxml_path;
        return fail(EXIT_OUTPUT, "cannot write '%s': %s", path, error->message);
    }
    return cli_fail_api(status, error, options->input);
}

/*
 * The request is run through the composer tool's motive entry, or motive_from_midi for a MIDI file. A motive that is a MIDI
 * file and is also rendered is read once by the bytes entry; its motive document, which holds the motive whole, is the input of
 * the render entry, which takes text only.
 */
static UrsatzStatus call_api(const Options *options, const MotiveInput *input, const unsigned char *bytes, size_t size,
                               bool midi, bool render, cJSON *request, CliResponse *response) {
    UrsatzStatus status;
    WorkshopBytes file = {bytes, size, NULL};
    if (!midi) {
        status = cli_composer(render ? "motive_render" : "motive", request, NULL, response);
    } else if (!render) {
        status = cli_composer("motive_from_midi", request, &file, response);
    } else {
        /* Read once with the input's options; render the motive that came out. */
        cJSON *plain = motive_input_request(input, bytes, size, &(bool){false});
        bool ok = plain && add_command_members(plain, options, false);
        status = ok ? cli_composer("motive_from_midi", plain, &file, response) : URSATZ_ERR_ALLOC;
        cJSON_Delete(plain);
        if (status == URSATZ_OK) {
            cJSON *document = cJSON_Parse(response->json);
            const cJSON *motive = document ? cJSON_GetObjectItemCaseSensitive(document, "motive") : NULL;
            cJSON *second = cJSON_CreateObject();
            cJSON *source = cJSON_AddObjectToObject(second, "input");
            char *motive_text = motive ? cJSON_PrintUnformatted(motive) : NULL;
            ok = second && source && motive_text && cJSON_AddStringToObject(source, "format", "json") &&
                 cJSON_AddStringToObject(source, "text", motive_text) && add_command_members(second, options, true);
            cli_response_destroy(response);
            status = ok ? cli_composer("motive_render", second, NULL, response) : URSATZ_ERR_ALLOC;
            free(motive_text);
            cJSON_Delete(second);
            cJSON_Delete(document);
        }
    }
    return status;
}

int run_motive(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    if (!options.input) {
        return cli_needs("motive", "an input file", true);
    }

    MotiveInput input;
    memset(&input, 0, sizeof input);
    input.path = options.input;
    input.meter_text = options.meter_text;
    input.key_text = options.key_text;
    input.format_text = options.format_text;
    input.form_text = options.form_text;
    input.pickup_text = options.pickup_text;
    input.extras = options.extras;

    unsigned char *bytes = NULL;
    size_t size = 0;
    char read_error[512];
    int read_status = motive_input_read_file(options.input, &bytes, &size, read_error, sizeof read_error);
    bool midi = false;
    cJSON *request = motive_input_request(&input, bytes, size, &midi);
    bool render = options.render_key_text || options.midi_path || options.musicxml_path;
    if (!request || !add_command_members(request, &options, render)) {
        cJSON_Delete(request);
        free(bytes);
        return fail(EXIT_USAGE, "out of memory");
    }

    CliResponse response;
    UrsatzStatus api_status = call_api(&options, &input, bytes, size, midi, render, request, &response);
    cJSON_Delete(request);
    free(bytes);
    if (read_status != 0 && api_status != URSATZ_ERR_REQUEST) {
        /* A bad option is reported before a file that cannot be read, as it always was. */
        cli_response_destroy(&response);
        return fail(read_status, "%s", read_error);
    }
    if (api_status != URSATZ_OK) {
        status = fail_response(api_status, &response.error, &options);
        cli_response_destroy(&response);
        return status;
    }

    if (options.midi_path) {
        status = write_output(options.midi_path, response.midi, response.midi_size);
    }
    if (status == 0 && options.musicxml_path) {
        status = write_output(options.musicxml_path, response.musicxml, strlen(response.musicxml));
    }
    if (status == 0 && options.json) {
        fputs(response.json, stdout);
    } else if (status == 0 && !motive_text_print(response.json, stdout)) {
        status = fail(EXIT_UNLOADABLE, "cannot analyze '%s'", options.input);
    }
    cli_response_destroy(&response);
    return status;
}
