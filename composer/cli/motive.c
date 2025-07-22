#include "motive.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_render.h>

#include "cli_options.h"

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

/* "0", "1", "1/2": beats of the beat unit, as whole notes. */
static bool parse_pickup(const char *text, int beat_unit, PlanTime *out) {
    char *end = NULL;
    long numerator = strtol(text, &end, 10);
    long denominator = 1;
    if (end == text || numerator < 0) {
        return false;
    }
    if (*end == '/') {
        const char *rest = end + 1;
        denominator = strtol(rest, &end, 10);
        if (end == rest || denominator < 1) {
            return false;
        }
    }
    return *end == '\0' && plan_time_make(numerator, (unsigned long)(denominator * beat_unit), out) == PLAN_OK;
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
    for (int i = 2; i < argc; i++) {
        const char *argument = argv[i];
        if (strcmp(argument, "--help") == 0) {
            options->help = true;
        } else if (strcmp(argument, "--json") == 0) {
            options->json = true;
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

    MotiveOptions motive_options;
    memset(&motive_options, 0, sizeof(motive_options));
    if (options.meter_text) {
        if (!motive_parse_meter(options.meter_text, &motive_options.meter)) {
            return fail(EXIT_USAGE, "meter '%s' is not a meter; write beats over a beat unit, as in 4/4 or 6/8",
                        options.meter_text);
        }
        motive_options.has_meter = true;
    }
    if (options.key_text) {
        if (!motive_parse_key(options.key_text, &motive_options.key)) {
            return fail(EXIT_USAGE,
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                        "for example \"D minor\"",
                        options.key_text);
        }
        motive_options.has_key = true;
    }
    MotiveFormat format = MOTIVE_FORMAT_AUTO;
    if (options.format_text && !motive_format_parse(options.format_text, &format)) {
        return fail(EXIT_USAGE, "--format must be auto, midi, musicxml, notes, degrees, rhythm, contour or json, not '%s'",
                    options.format_text);
    }
    if (options.form_text) {
        MotiveForm form;
        if (!motive_form_parse(options.form_text, &form)) {
            return fail(EXIT_USAGE, "--form must be full, rhythm or contour, not '%s'", options.form_text);
        }
        /* A rhythm or a contour is read by its own reader; full leaves the format as it is. */
        MotiveFormat wanted = form == MOTIVE_FORM_RHYTHM ? MOTIVE_FORMAT_RHYTHM : MOTIVE_FORMAT_CONTOUR;
        if (form != MOTIVE_FORM_FULL && format != MOTIVE_FORMAT_AUTO && format != wanted) {
            return fail(EXIT_USAGE, "--form %s reads a %s, which --format %s does not", options.form_text,
                        options.form_text, options.format_text);
        }
        if (form == MOTIVE_FORM_FULL && (format == MOTIVE_FORMAT_RHYTHM || format == MOTIVE_FORMAT_CONTOUR)) {
            return fail(EXIT_USAGE, "--form full does not read a %s; use --form %s or another --format",
                        options.format_text, options.format_text);
        }
        if (form != MOTIVE_FORM_FULL) {
            format = wanted;
        }
    }

    Motive motive;
    MotiveError error;
    MotiveResult result = motive_load_file(options.input, format, &motive_options, &motive, &error);
    if (result == MOTIVE_OK && options.pickup_text) {
        /* The pickup is in beats of the motive's meter, which a file may declare, so it is read after the first load. */
        PlanTime pickup;
        if (!parse_pickup(options.pickup_text, motive.meter.beat_unit, &pickup)) {
            return fail(EXIT_USAGE, "pickup '%s' is not a number of beats; write 0, 1 or 1/2", options.pickup_text);
        }
        motive_options.has_pickup = true;
        motive_options.pickup = pickup;
        result = motive_load_file(options.input, format, &motive_options, &motive, &error);
    }
    if (result != MOTIVE_OK) {
        return fail(EXIT_UNLOADABLE, "cannot load '%s' (%s)%s%s: %s", options.input, motive_result_name(result),
                    error.location[0] ? " at " : "", error.location, error.message);
    }
    MotiveAnalysis analysis;
    if (!motive_analyze(&motive, &analysis)) {
        return fail(EXIT_UNLOADABLE, "cannot analyze '%s'", options.input);
    }

    bool render = options.render_key_text || options.midi_path || options.musicxml_path;
    Motive rendered = motive;
    if (options.render_key_text) {
        PlanKey render_key;
        if (!motive_parse_key(options.render_key_text, &render_key)) {
            return fail(EXIT_USAGE,
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                        "for example \"D minor\"",
                        options.render_key_text);
        }
        if (!motive_in_key(&motive, &render_key, &rendered)) {
            return fail(EXIT_UNLOADABLE, "cannot move '%s' to %s: it has no key, or a note leaves the MIDI range",
                        options.input, options.render_key_text);
        }
    }
    PlanExportOptions export_options;
    status = cli_read_export_options(options.tempo_text, false, &export_options);
    if (status != 0) {
        return status;
    }
    PlanExportError export_error;
    memset(&export_error, 0, sizeof(export_error));
    if (options.midi_path &&
        motive_export_midi(&rendered, &export_options, options.midi_path, &export_error) != PLAN_EXPORT_OK) {
        return fail(EXIT_OUTPUT, "cannot write '%s': %s", options.midi_path, export_error.message);
    }
    if (options.musicxml_path &&
        motive_export_musicxml(&rendered, &export_options, options.musicxml_path, &export_error) != PLAN_EXPORT_OK) {
        return fail(EXIT_OUTPUT, "cannot write '%s': %s", options.musicxml_path, export_error.message);
    }

    if (options.json) {
        char *json = render ? motive_analysis_to_json_rendered(&motive, &analysis, &rendered)
                            : motive_analysis_to_json(&motive, &analysis);
        if (!json) {
            return fail(EXIT_UNLOADABLE, "out of memory");
        }
        fputs(json, stdout);
        free(json);
    } else {
        motive_analysis_print(&motive, &analysis, stdout);
    }
    return 0;
}
