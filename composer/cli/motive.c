#include "motive.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>

#define EXIT_USAGE 1
#define EXIT_UNLOADABLE 2

typedef struct {
    const char *input;
    const char *meter_text;
    const char *key_text;
    const char *format_text;
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
    fprintf(out, "  --format <name>      auto (default), midi, musicxml, notes or json\n");
    fprintf(out, "  --json               print the analysis as JSON\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the input cannot be loaded.\n\n");
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
    for (int i = 2; i < argc; i++) {
        const char *argument = argv[i];
        if (strcmp(argument, "--help") == 0) {
            options->help = true;
        } else if (strcmp(argument, "--json") == 0) {
            options->json = true;
        } else if (strcmp(argument, "--meter") == 0 || strcmp(argument, "--key") == 0 ||
                   strcmp(argument, "--format") == 0) {
            const char **slot = strcmp(argument, "--meter") == 0 ? &options->meter_text
                                : strcmp(argument, "--key") == 0 ? &options->key_text
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
        return fail(EXIT_USAGE, "--format must be auto, midi, musicxml, notes or json, not '%s'", options.format_text);
    }

    Motive motive;
    MotiveError error;
    MotiveResult result = motive_load_file(options.input, format, &motive_options, &motive, &error);
    if (result != MOTIVE_OK) {
        return fail(EXIT_UNLOADABLE, "cannot load '%s' (%s)%s%s: %s", options.input, motive_result_name(result),
                    error.location[0] ? " at " : "", error.location, error.message);
    }
    MotiveAnalysis analysis;
    if (!motive_analyze(&motive, &analysis)) {
        return fail(EXIT_UNLOADABLE, "cannot analyze '%s'", options.input);
    }
    if (options.json) {
        char *json = motive_analysis_to_json(&motive, &analysis);
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
