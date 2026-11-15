#include "grow.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_options.h"
#include "grow_text.h"
#include "motive_extras.h"
#include "motive_input.h"
#include "workflow.h"

#include <ursatz_composer/composer_api.h>

#define GROW_MAX_SEED_TRIES 16
#define GROW_MAX_SUGGESTIONS 16

typedef struct {
    bool help;
    const char *input;
    const char *from_corpus;
    const char *like_path;
    const char *into;
    const char *hybrid;
    const char *key;
    const char *mode;
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

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", GROW_USAGE_LINE + 7);
    fprintf(out, "Grows a theme from the motive in <motive-input> (a MIDI file, a MusicXML file, a note list or a motive\n");
    fprintf(out, "JSON file): its structure, a harmony, a bass line and a melody, ending in the cadence asked for. With\n");
    fprintf(out, "--from-corpus and --like the motive is not given but taken from a corpus.\n\n");
    fprintf(out, "Required:\n");
    fprintf(out, "  --into <form>        sentence (presentation + continuation), period (antecedent + consequent) or\n");
    fprintf(out, "                       hybrid (any other pairing, named with --hybrid)\n");
    fprintf(out, "  --key <key>          tonic and mode in one argument (\"D minor\", \"F# major\"); any case\n");
    fprintf(out, "  --mode <mode>        major, minor or style: replaces the mode of --key (the key may then be a tonic alone). style\n");
    fprintf(out, "                       takes the style's most common mode and needs --style with a mode category\n");
    fprintf(out, "  --meter <n/m>        2/4, 3/4, 4/4 or 6/8; the motive must fit in one bar of it\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --hybrid <b>+<e>     with --into hybrid only: the beginning (presentation, antecedent or cbi, a compound\n");
    fprintf(out, "                       basic idea) and the ending (continuation, consequent or cadential), for example\n");
    fprintf(out, "                       antecedent+continuation\n");
    fprintf(out, "  --bars <3-16>        length of the theme (default 8; 4 uses a one-bar basic idea, others a chosen construction)\n");
    fprintf(out, "  --cadence <type>     PAC (default), IAC or HC: the cadence the theme ends in\n");
    fprintf(out, "  --first-cadence <t>  HC (default) or IAC: the weaker cadence an antecedent ends in (a period or a hybrid\n");
    fprintf(out, "                       that begins with an antecedent only)\n");
    fprintf(out, "  --accompaniment <p>  write an accompaniment in the inner voices: block, arpeggiated, alberti (not in 6/8),\n");
    fprintf(out, "                       oom-pah or any (the pattern is chosen from the seed)\n");
    fprintf(out, "  --accompaniment-motive <file>  a saved accompaniment motive as the accompaniment: restated on each chord in\n");
    fprintf(out, "                       chord tones, liquidated in the last bar; implies --accompaniment motive. Read as a motive\n");
    fprintf(out, "                       input of the accompaniment role, in --meter, at most one bar long\n");
    fprintf(out, "  --accompaniment-format <f>, --accompaniment-pickup <b>   as --format and --pickup of the motive command,\n");
    fprintf(out, "                       for the accompaniment motive's file\n");
    fprintf(out, "  --texture <name>     the texture the accompaniment is to be heard as: \"homophony\", \"homorhythm\" (block\n");
    fprintf(out, "                       chords only) or \"melody and accompaniment\"; implies --accompaniment any. Or\n");
    fprintf(out, "                       \"chorale\": four parts in a note a chord, the melody written with the other three\n");
    fprintf(out, "                       (standard voicing rules), instead of the grown melody\n");
    fprintf(out, "  --voicing            keep the grown melody as the soprano and add an alto and a tenor under it, voiced\n");
    fprintf(out, "                       over the bass by the standard voicing rules (not with --accompaniment)\n");
    fprintf(out, "  --check              analyze the theme with the Ursatz Analyzer and pass over a seed whose analysis contradicts\n");
    fprintf(out, "                       the plan (the cadences or the key not heard as planned, a hard voice-leading fault)\n");
    fprintf(out, "  --style <file>       prefer the harmony and melody closest to a style: a style model file from the Ursatz\n");
    fprintf(out, "                       Analyzer (--style-profile). The Analyzer's distance from the style is printed\n");
    fprintf(out, "  --style-weight <n/d> how strongly the style is preferred (default 1/1; needs --style)\n");
    fprintf(out, "  --style-without <c>  read the style without the category c (repeatable, at most 8; needs --style): the motive\n");
    fprintf(out, "                       categories are preferences and this switches one off, for the scoring and the distance\n");
    fprintf(out, "  --blend <file>       a second style model file, blended with --style (needs --style)\n");
    fprintf(out, "  --blend-weight <n/d> the second style's share in every dimension, 0 to 1 (default 1/2; needs --blend)\n");
    fprintf(out, "  --blend-weights harmony=<n/d>,rhythm=<n/d>,melody=<n/d>,form=<n/d>\n");
    fprintf(out, "                       the share per dimension; one not named takes --blend-weight\n");
    fprintf(out, "  --from-corpus <dir>  start from material in a corpus instead of <motive-input>: the output directory of\n");
    fprintf(out, "                       ursatz-analyzer --corpus. Needs --like; the passage it names, the measures and the score\n");
    fprintf(out, "                       are printed and kept in the plan. Exit 2 when no passage scores 1/2 or more\n");
    fprintf(out, "  --like <file>        with --from-corpus: a MIDI or MusicXML passage; the first bar of a similar passage of the\n");
    fprintf(out, "                       corpus (picked by the seed among the best) becomes the motive, in --key\n");
    motive_extras_print_help(out, false);
    fprintf(out, "  --seed <n>           whole number from 0; the same seed gives the same output. Default: derived from\n");
    fprintf(out, "                       the clock. The seed used is always printed (if a seed gets stuck, the next seeds are\n");
    fprintf(out, "                       tried, up to %d, and the one that worked is printed)\n", GROW_MAX_SEED_TRIES);
    fprintf(out, "  --suggest <n>        write n different themes (1 to %d) instead of one; with files, theme k goes to\n", GROW_MAX_SUGGESTIONS);
    fprintf(out, "                       <name>-k.<extension>\n");
    fprintf(out, "  --out <file.json>    write the plan as JSON\n");
    fprintf(out, "  --midi <file>        write the melody, the bass and the accompaniment as a MIDI file\n");
    fprintf(out, "  --musicxml <file>    write the melody, the bass and the accompaniment as MusicXML\n");
    fprintf(out, "  --tempo <bpm>        MIDI and MusicXML: write a tempo, quarter notes per minute from 20 to 400 (a whole number or\n");
    fprintf(out, "                       a fraction like 195/2). Default: none, the file default of 120 applies\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --explain <file>     write why each candidate was chosen or rejected: per target the chosen candidate, its\n");
    fprintf(out, "                       runners-up and the rejected ones with their reasons, the form heard and the analyzer check\n");
    fprintf(out, "                       (with --check), and the seeds passed over. JSON when the file ends in .json, else text\n");
    fprintf(out, "  --format <text|json> how to print the theme on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "The form heard in the result (by Ursatz's standard-form) is printed with it; a theme need not be heard as\n");
    fprintf(out, "the form it was grown as, and that is reported, not an error.\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the request cannot be satisfied;\n");
    fprintf(out, "3 an output file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer grow theme.txt --into sentence --key \"D minor\" --meter 4/4 --seed 7 --out theme.json\n");
    fprintf(out, "  ursatz-composer grow theme.txt --into hybrid --hybrid antecedent+continuation --key \"G major\" --meter 3/4\n");
}

/* --- reading the options -------------------------------------------------- */

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    int extras_status = 0;
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (motive_extras_take(&options->extras, argc, argv, &i, false, &extras_status)) {
            if (extras_status != 0) {
                return extras_status;
            }
        } else if (strcmp(arg, "--from-corpus") == 0) {
            target = &options->from_corpus;
        } else if (strcmp(arg, "--like") == 0) {
            target = &options->like_path;
        } else if (strcmp(arg, "--into") == 0) {
            target = &options->into;
        } else if (strcmp(arg, "--hybrid") == 0) {
            target = &options->hybrid;
        } else if (strcmp(arg, "--key") == 0) {
            target = &options->key;
        } else if (strcmp(arg, "--mode") == 0) {
            target = &options->mode;
        } else if (strcmp(arg, "--meter") == 0) {
            target = &options->meter;
        } else if (strcmp(arg, "--bars") == 0) {
            target = &options->bars;
        } else if (strcmp(arg, "--cadence") == 0) {
            target = &options->cadence;
        } else if (strcmp(arg, "--first-cadence") == 0) {
            target = &options->first_cadence;
        } else if (strcmp(arg, "--accompaniment") == 0) {
            target = &options->accompaniment;
        } else if (strcmp(arg, "--accompaniment-motive") == 0) {
            target = &options->accompaniment_motive_path;
        } else if (strcmp(arg, "--accompaniment-format") == 0) {
            target = &options->accompaniment_format_text;
        } else if (strcmp(arg, "--accompaniment-pickup") == 0) {
            target = &options->accompaniment_pickup_text;
        } else if (strcmp(arg, "--texture") == 0) {
            target = &options->texture;
        } else if (strcmp(arg, "--voicing") == 0) {
            options->voicing = true;
        } else if (strcmp(arg, "--check") == 0) {
            options->check = true;
        } else if (strcmp(arg, "--style") == 0) {
            target = &options->style_path;
        } else if (strcmp(arg, "--style-without") == 0) {
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", arg);
            }
            int added = cli_style_without_add(&options->without, argv[++i]);
            if (added != 0) {
                return added;
            }
        } else if (strcmp(arg, "--style-weight") == 0) {
            target = &options->style_weight;
        } else if (strcmp(arg, "--blend") == 0) {
            target = &options->blend_path;
        } else if (strcmp(arg, "--blend-weight") == 0) {
            target = &options->blend_weight_text;
        } else if (strcmp(arg, "--blend-weights") == 0) {
            target = &options->blend_weights_text;
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
        } else if (strcmp(arg, "--explain") == 0) {
            target = &options->explain_path;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format_text;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer grow --help)", arg);
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
        return cli_fail(CLI_EXIT_USAGE, "grow needs a motive input\n%s", GROW_USAGE_LINE);
    }
    if (!options->into || !options->key || !options->meter) {
        return cli_fail(CLI_EXIT_USAGE, "grow needs --into, --key and --meter (see --help)");
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
              (!options->mode || cJSON_AddStringToObject(request, "mode", options->mode)) &&
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

int run_grow(int argc, char **argv) {
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

    /* The motive: a file read into the request, or a passage of a corpus. A tonic alone with --mode is completed for a MIDI
       file, which is read before the API sees the brief. */
    cJSON *request = NULL;
    MotiveFile file;
    MotiveFile accompaniment_file;
    char key_with_mode[96];
    memset(&file, 0, sizeof file);
    memset(&accompaniment_file, 0, sizeof accompaniment_file);
    if (options.from_corpus) {
        request = cJSON_CreateObject();
        if (!request || !cJSON_AddStringToObject(request, "key", options.key) ||
            !cJSON_AddStringToObject(request, "corpus", options.from_corpus) ||
            !cJSON_AddStringToObject(request, "like", options.like_path)) {
            cJSON_Delete(request);
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
        if (options.mode && !strpbrk(options.key, " \t")) {
            bool concrete = cli_equals_ignore_case(options.mode, "major") || cli_equals_ignore_case(options.mode, "minor");
            input.key_text = concrete && snprintf(key_with_mode, sizeof key_with_mode, "%s %s", options.key, options.mode) <
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

    ComposerResponse response;
    ComposerStatus called = cli_call(composer_api_grow, request, &response);
    if (called != COMPOSER_OK) {
        /* A bad option is reported before a file that cannot be read; the first file that could not be read is the one named. */
        MotiveFile *failed = file.read_status != 0 ? &file : &accompaniment_file;
        MotiveInputNames names = {options.input, NULL, NULL, options.accompaniment_motive_path};
        status = motive_input_fail(failed, called, &response.error, &names);
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
        fprintf(stderr, strncmp(notice->valuestring, "only ", 5) == 0 ? "Note: %s\n" : "Warning: %s\n", notice->valuestring);
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
    composer_response_destroy(&response);
    return status;
}
