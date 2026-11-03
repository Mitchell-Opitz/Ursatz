#include "motive.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/motive.h>

#include "cli_options.h"
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
    const char *tone_texts[MOTIVE_MAX_NOTES];
    size_t tone_count;
    bool json;
    bool help;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", MOTIVE_USAGE_LINE + 7);
    fprintf(out, "Loads a motive from a file and prints its analysis: intervals, contour, rhythm, range,\n");
    fprintf(out, "scale degrees and implied harmony.\n\n");
    fprintf(out, "  <input>              a MIDI file, a MusicXML file, a note list or a motive JSON file\n");
    fprintf(out, "  --meter <n/m>        the meter, for input that has none (a note list); a MIDI or MusicXML\n");
    fprintf(out, "                       file that declares another meter is an error\n");
    fprintf(out, "  --key <key>          the key, for example \"D minor\"; replaces a key the file states\n");
    fprintf(out, "  --format <name>      auto (default), midi, musicxml, notes, degrees, rhythm, contour or json\n");
    fprintf(out, "  --form <name>        full (default), rhythm or contour; rhythm and contour read the text as a\n");
    fprintf(out, "                       rhythm (x:q x:e) or a contour (0 2 1 3) instead of a note list\n");
    fprintf(out, "  --pickup <beats>     the beats before the first barline, as 1 or 1/2; default 0\n");
    motive_extras_print_help(out, false);
    fprintf(out, "  --tone <n>=<role>    label note n (1-based) as chord or as a non-chord tone: passing, neighbor,\n");
    fprintf(out, "                       appoggiatura, suspension, retardation, escape, anticipation, double-neighbor,\n");
    fprintf(out, "                       changing-tone or pedal; repeatable\n");
    fprintf(out, "  --render-key <key>   hear the motive in this key (same degrees); default the motive's key\n");
    fprintf(out, "  --midi <file>        write the motive as a MIDI file\n");
    fprintf(out, "  --musicxml <file>    write the motive as a MusicXML file (a pickup is an implicit first measure)\n");
    fprintf(out, "  --tempo <bpm>        the tempo of the files, 20 to 400 quarter notes per minute\n");
    fprintf(out, "  --json               print the analysis as JSON, with the rendered notes when a render option is given\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the input cannot be loaded; 3 a file cannot be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer motive theme.txt --meter 4/4 --key \"D minor\"\n");
}

static int fail(int status, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    return status;
}

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    int extras_status = 0;
    for (int i = 2; i < argc; i++) {
        const char *argument = argv[i];
        if (strcmp(argument, "--help") == 0) {
            options->help = true;
        } else if (strcmp(argument, "--json") == 0) {
            options->json = true;
        } else if (strcmp(argument, "--tone") == 0) {
            if (i + 1 >= argc) {
                return fail(EXIT_USAGE, "--tone needs a value");
            }
            if (options->tone_count == MOTIVE_MAX_NOTES) {
                return fail(EXIT_USAGE, "at most %d --tone options", MOTIVE_MAX_NOTES);
            }
            options->tone_texts[options->tone_count++] = argv[++i];
        } else if (motive_extras_take(&options->extras, argc, argv, &i, false, &extras_status)) {
            if (extras_status != 0) {
                return extras_status;
            }
        } else if (strcmp(argument, "--meter") == 0 || strcmp(argument, "--key") == 0 ||
                   strcmp(argument, "--format") == 0 || strcmp(argument, "--form") == 0 ||
                   strcmp(argument, "--pickup") == 0 || strcmp(argument, "--render-key") == 0 ||
                   strcmp(argument, "--midi") == 0 || strcmp(argument, "--musicxml") == 0 ||
                   strcmp(argument, "--tempo") == 0) {
            const char **slot = strcmp(argument, "--meter") == 0        ? &options->meter_text
                                : strcmp(argument, "--render-key") == 0 ? &options->render_key_text
                                : strcmp(argument, "--midi") == 0       ? &options->midi_path
                                : strcmp(argument, "--musicxml") == 0   ? &options->musicxml_path
                                : strcmp(argument, "--tempo") == 0      ? &options->tempo_text
                                : strcmp(argument, "--key") == 0    ? &options->key_text
                                : strcmp(argument, "--form") == 0   ? &options->form_text
                                : strcmp(argument, "--pickup") == 0 ? &options->pickup_text
                                                                    : &options->format_text;
            if (i + 1 >= argc) {
                return fail(EXIT_USAGE, "%s needs a value", argument);
            }
            if (*slot) {
                return fail(EXIT_USAGE, "%s given twice", argument);
            }
            *slot = argv[++i];
        } else if (argument[0] == '-' && argument[1] != '\0') {
            return fail(EXIT_USAGE, "unknown option '%s'", argument);
        } else if (!options->input) {
            options->input = argument;
        } else {
            return fail(EXIT_USAGE, "unexpected argument '%s'; the input is one file", argument);
        }
    }
    return 0;
}

/* Writes the bytes of an output the response holds to the path the user gave. Returns 0, or the status of the failure. */
static int write_output(const char *path, const void *bytes, size_t size) {
    FILE *file = fopen(path, "wb");
    bool ok = file && (size == 0 || fwrite(bytes, 1, size, file) == size);
    if (file && fclose(file) != 0) {
        ok = false;
    }
    return ok ? 0 : fail(EXIT_OUTPUT, "cannot write '%s': could not write '%s'", path, path);
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
static int fail_response(ComposerStatus status, const ComposerError *error, const Options *options) {
    if (strcmp(error->path, "/midi") == 0 || strcmp(error->path, "/musicxml") == 0) {
        const char *path = strcmp(error->path, "/midi") == 0 ? options->midi_path : options->musicxml_path;
        return fail(EXIT_OUTPUT, "cannot write '%s': %s", path, error->message);
    }
    return cli_fail_api(status, error, options->input);
}

/*
 * The request is run through composer_api_motive, or composer_api_motive_from_midi for a MIDI file. A motive that is a MIDI
 * file and is also rendered is read once by the bytes entry; its motive document, which holds the motive whole, is the input of
 * the render entry, which takes text only.
 */
static ComposerStatus call_api(const Options *options, const MotiveInput *input, const unsigned char *bytes, size_t size,
                               bool midi, bool render, cJSON *request, ComposerResponse *response) {
    char *text = cJSON_PrintUnformatted(request);
    if (!text) {
        memset(response, 0, sizeof *response);
        snprintf(response->error.message, sizeof response->error.message, "out of memory");
        return COMPOSER_ERR_ALLOC;
    }
    ComposerStatus status;
    if (!midi) {
        status = render ? composer_api_motive_render(text, NULL, response) : composer_api_motive(text, NULL, response);
    } else if (!render) {
        status = composer_api_motive_from_midi(bytes, size, text, NULL, response);
    } else {
        /* Read once with the input's options; render the motive that came out. */
        cJSON *plain = motive_input_request(input, bytes, size, &(bool){false});
        bool ok = plain && add_command_members(plain, options, false);
        char *plain_text = ok ? cJSON_PrintUnformatted(plain) : NULL;
        cJSON_Delete(plain);
        status = plain_text ? composer_api_motive_from_midi(bytes, size, plain_text, NULL, response) : COMPOSER_ERR_ALLOC;
        free(plain_text);
        if (status == COMPOSER_OK) {
            cJSON *document = cJSON_Parse(response->json);
            const cJSON *motive = document ? cJSON_GetObjectItemCaseSensitive(document, "motive") : NULL;
            cJSON *second = cJSON_CreateObject();
            cJSON *source = cJSON_AddObjectToObject(second, "input");
            char *motive_text = motive ? cJSON_PrintUnformatted(motive) : NULL;
            ok = second && source && motive_text && cJSON_AddStringToObject(source, "format", "json") &&
                 cJSON_AddStringToObject(source, "text", motive_text) && add_command_members(second, options, true);
            composer_response_destroy(response);
            char *second_text = ok ? cJSON_PrintUnformatted(second) : NULL;
            status = second_text ? composer_api_motive_render(second_text, NULL, response) : COMPOSER_ERR_ALLOC;
            free(second_text);
            free(motive_text);
            cJSON_Delete(second);
            cJSON_Delete(document);
        }
    }
    free(text);
    return status;
}

int run_motive(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    if (!options.input) {
        return fail(EXIT_USAGE, "motive needs an input file\n%s", MOTIVE_USAGE_LINE);
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

    ComposerResponse response;
    ComposerStatus api_status = call_api(&options, &input, bytes, size, midi, render, request, &response);
    cJSON_Delete(request);
    free(bytes);
    if (read_status != 0 && api_status != COMPOSER_ERR_REQUEST) {
        /* A bad option is reported before a file that cannot be read, as it always was. */
        composer_response_destroy(&response);
        return fail(read_status, "%s", read_error);
    }
    if (api_status != COMPOSER_OK) {
        status = fail_response(api_status, &response.error, &options);
        composer_response_destroy(&response);
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
    composer_response_destroy(&response);
    return status;
}
