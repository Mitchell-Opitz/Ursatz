#include "grow.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api/api_grow.h"
#include "api/api_internal.h"
#include "cli_options.h"
#include "grow_text.h"
#include "motive_extras.h"
#include "motive_input.h"
#include "util/request_parse.h"
#include "util/util.h"

#include <ursatz_composer/export.h>
#include <ursatz_composer/explain.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>

#define MAX_FILES (4 * GROW_MAX_SUGGESTIONS)

typedef struct {
    bool help;
    const char *input;
    const char *from_corpus;
    const char *like_path;
    GrowBriefText brief; /* the text of into, hybrid, key, mode, meter, bars, the cadences, the texture and the style weight */
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
    PlanExportOptions export_options;
    const char *explain_path;
    const char *format_text;
    MotiveExtras extras;
    ComposerStyleWeights blend_weights;
    Motive accompaniment_motive; /* loaded from accompaniment_motive_path, role accompaniment */
    GrowSetup setup;
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
    fprintf(out, "  --bars <4|8>         length of the theme (default 8; 4 uses a one-bar basic idea)\n");
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
            target = &options->brief.into;
        } else if (strcmp(arg, "--hybrid") == 0) {
            target = &options->brief.hybrid;
        } else if (strcmp(arg, "--key") == 0) {
            target = &options->brief.key;
        } else if (strcmp(arg, "--mode") == 0) {
            target = &options->brief.mode;
        } else if (strcmp(arg, "--meter") == 0) {
            target = &options->brief.meter;
        } else if (strcmp(arg, "--bars") == 0) {
            target = &options->brief.bars;
        } else if (strcmp(arg, "--cadence") == 0) {
            target = &options->brief.cadence;
        } else if (strcmp(arg, "--first-cadence") == 0) {
            target = &options->brief.first_cadence;
        } else if (strcmp(arg, "--accompaniment") == 0) {
            target = &options->brief.accompaniment;
        } else if (strcmp(arg, "--accompaniment-motive") == 0) {
            target = &options->accompaniment_motive_path;
        } else if (strcmp(arg, "--accompaniment-format") == 0) {
            target = &options->accompaniment_format_text;
        } else if (strcmp(arg, "--accompaniment-pickup") == 0) {
            target = &options->accompaniment_pickup_text;
        } else if (strcmp(arg, "--texture") == 0) {
            target = &options->brief.texture;
        } else if (strcmp(arg, "--voicing") == 0) {
            options->brief.voicing = true;
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
            target = &options->brief.style_weight;
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

/* Checks each value and fills the parsed members; returns 0, or the exit status. */
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
    if (!options->brief.into || !options->brief.key || !options->brief.meter) {
        return cli_fail(CLI_EXIT_USAGE, "grow needs --into, --key and --meter (see --help)");
    }
    GrowSetup *setup = &options->setup;
    options->brief.has_style = options->style_path != NULL;
    options->brief.without_count = options->without.count;
    options->brief.accompaniment_motive = options->accompaniment_motive_path != NULL;
    ComposerError error;
    memset(&error, 0, sizeof error);
    ComposerStatus status = api_grow_read_brief(&options->brief, setup, &error);
    if (status != COMPOSER_OK) {
        return cli_fail_api(status, &error, NULL);
    }
    if ((options->accompaniment_format_text || options->accompaniment_pickup_text) && !options->accompaniment_motive_path) {
        return cli_fail(CLI_EXIT_USAGE, "--accompaniment-format and --accompaniment-pickup need --accompaniment-motive <file>");
    }
    int blend = cli_read_blend(options->style_path != NULL, options->blend_path, options->blend_weight_text,
                               options->blend_weights_text, &options->blend_weights);
    if (blend != 0) {
        return blend;
    }
    setup->suggest = 1;
    if (options->suggest_text &&
        (!request_parse_whole_number(options->suggest_text, GROW_MAX_SUGGESTIONS, &setup->suggest) || setup->suggest == 0)) {
        return cli_fail(CLI_EXIT_USAGE, "--suggest must be a whole number from 1 to %d, not '%s'", GROW_MAX_SUGGESTIONS,
                        options->suggest_text);
    }
    setup->suggested = options->suggest_text != NULL;
    if (options->seed_text) {
        if (!request_parse_whole_number(options->seed_text, GROW_SEED_MAX, &setup->seed)) {
            return cli_fail(CLI_EXIT_USAGE, "--seed must be a whole number from 0 to 4294967295, not '%s'",
                            options->seed_text);
        }
    } else {
        setup->seed = cli_default_seed();
    }
    {
        int export_status = cli_read_export_options(options->tempo_text, options->plain_midi, &options->export_options);
        if (export_status != 0) {
            return export_status;
        }
    }
    if (options->format_text) {
        if (request_equals_ignore_case(options->format_text, "json")) {
            options->json = true;
        } else if (!request_equals_ignore_case(options->format_text, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", options->format_text);
        }
    }
    return 0;
}

/* --- writing the files ---------------------------------------------------------- */

/*
 * Writes every output to a temporary file next to its final name, and renames them all only when every one was
 * written, so that an error leaves no output file behind.
 */
static int write_files(const Options *options, const GrowRun *run, CliPendingFile *pending, size_t *pending_count) {
    const char *paths[4] = {options->out_path, options->midi_path, options->musicxml_path, options->explain_path};
    int status = 0;
    *pending_count = 0;
    for (size_t k = 0; k < run->made && status == 0; k++) {
        RenderedPlan rendered;
        bool have_rendering = false;
        for (int kind = 0; kind < 4 && status == 0; kind++) {
            if (!paths[kind]) {
                continue;
            }
            CliPendingFile *entry = &pending[*pending_count];
            if (!cli_alternative_path(paths[kind], k + 1, run->made > 1, entry->final_path, sizeof(entry->final_path)) ||
                snprintf(entry->temporary, sizeof(entry->temporary), "%s.tmp", entry->final_path) >=
                    (int)sizeof(entry->temporary)) {
                status = cli_fail(CLI_EXIT_WRITE, "file name '%s' is too long", paths[kind]);
                break;
            }
            (*pending_count)++;
            const GrowTheme *theme = &run->themes[k];
            if (kind == 0) {
                PlanJsonError error;
                if (plan_write_json_file(theme->plan, entry->temporary, &error) != PLAN_OK) {
                    status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", entry->final_path, error.message);
                }
                continue;
            }
            if (kind == 3) {
                Explanation explanation;
                ExplainResult built = explain_build_grow(theme->plan, api_find_node(theme->plan, "theme"), &theme->outcome,
                                                         theme->checked ? &theme->check : NULL, run->passed,
                                                         theme->passed_before, &explanation);
                if (built != EXPLAIN_OK || cli_write_explanation(entry->final_path, entry->temporary, &explanation) != 0) {
                    status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", entry->final_path);
                }
                if (built == EXPLAIN_OK) {
                    explain_destroy(&explanation);
                }
                continue;
            }
            PlanExportError error;
            if (!have_rendering) {
                if (plan_export_render(theme->plan, &rendered, NULL) != PLAN_EXPORT_OK) {
                    status = cli_fail(CLI_EXIT_WRITE, "cannot render the plan");
                    break;
                }
                have_rendering = true;
            }
            PlanExportResult result = kind == 1 ? rendered_plan_export_midi(&rendered, &options->export_options, entry->temporary, &error)
                                                : rendered_plan_export_musicxml(&rendered, &options->export_options, entry->temporary, &error);
            if (result != PLAN_EXPORT_OK) {
                status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", entry->final_path, error.message);
            }
        }
        if (have_rendering) {
            rendered_plan_destroy(&rendered);
        }
    }
    if (status != 0) {
        cli_discard_pending(pending, *pending_count);
        return status;
    }
    for (size_t i = 0; i < *pending_count; i++) {
        if (rename(pending[i].temporary, pending[i].final_path) != 0) {
            status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", pending[i].final_path);
            break;
        }
    }
    if (status != 0) {
        cli_discard_pending(pending, *pending_count);
    }
    return status;
}

/* --- the command ---------------------------------------------------------------- */

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
    GrowSetup *setup = &options.setup;
    ComposerError error;
    memset(&error, 0, sizeof error);
    ComposerStyle *style = NULL;
    if (options.style_path) {
        status = options.blend_path
                     ? cli_load_blend(options.style_path, options.blend_path, &options.blend_weights, options.without.names, options.without.count, &style)
                     : cli_load_style(options.style_path, options.without.names, options.without.count, &style);
        if (status != 0) {
            return status;
        }
    }
    setup->style = style;
    setup->without = options.without.names;
    setup->without_count = options.without.count;
    setup->check = options.check;
    setup->explain = options.explain_path != NULL;
    ComposerRatio option_tempo = {options.export_options.tempo_numerator, options.export_options.tempo_denominator};
    ComposerStatus api_status = api_grow_set_tempo_and_mode(setup, options.tempo_text ? &option_tempo : NULL, &error);
    if (api_status != COMPOSER_OK) {
        status = cli_fail(CLI_EXIT_USAGE, "--mode style needs a --style with a mode category");
        composer_style_destroy(style);
        return status;
    }
    if (!options.tempo_text && setup->tempo.denominator > 0) {
        options.export_options.tempo_numerator = setup->tempo.numerator;
        options.export_options.tempo_denominator = setup->tempo.denominator;
    }

    MotiveOptions motive_options;
    memset(&motive_options, 0, sizeof(motive_options));
    motive_options.has_meter = true;
    motive_options.meter = setup->meter;
    motive_options.has_key = true;
    motive_options.key = setup->key;
    Motive motive;
    if (options.from_corpus && motive_extras_given(&options.extras)) {
        composer_style_destroy(style);
        return cli_fail(CLI_EXIT_USAGE, "--harmony, --feature and --role describe a motive file, not one seeded from a corpus");
    }
    status = motive_extras_options(&options.extras, &setup->meter, &motive_options);
    if (status == 0 && options.from_corpus) {
        /* The passage is chosen once, by the first seed; the seeds tried after it grow from the same motive. */
        status = cli_seed_motive(options.from_corpus, options.like_path, &setup->meter, &setup->key, setup->seed,
                                 &setup->seed_found);
        if (status == 0) {
            status = cli_seed_source(&setup->seed_found, &setup->source);
        }
        if (status == 0) {
            setup->seeded = true;
            motive = setup->seed_found.motive;
        }
    } else if (status == 0) {
        MotiveError motive_error;
        MotiveResult loaded = motive_load_file(options.input, MOTIVE_FORMAT_AUTO, &motive_options, &motive, &motive_error);
        if (loaded != MOTIVE_OK) {
            status = cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot load '%s' (%s)%s%s: %s", options.input, motive_result_name(loaded),
                              motive_error.location[0] ? " at " : "", motive_error.location, motive_error.message);
        }
    }
    if (status == 0 && options.accompaniment_motive_path) {
        MotiveInput input;
        memset(&input, 0, sizeof(input));
        input.path = options.accompaniment_motive_path;
        input.meter_text = options.brief.meter;
        input.format_text = options.accompaniment_format_text;
        input.pickup_text = options.accompaniment_pickup_text;
        input.extras.role_text = "accompaniment";
        status = motive_input_load(&input, &options.accompaniment_motive);
        setup->accompaniment_motive = &options.accompaniment_motive;
    }
    if (status != 0) {
        composer_style_destroy(style);
        return status;
    }
    setup->motive = &motive;
    setup->text = &options.brief;

    /* The seeds are tried one after another; a stuck run is passed over, and a theme that is the same as one already
       made is not counted. A request no seed can mend ends the search at once. */
    GrowRun run;
    api_status = api_grow_run(setup, &run, &error);
    if (api_status != COMPOSER_OK) {
        status = cli_fail_api(api_status, &error, NULL);
    }
    if (status == 0 && run.made < setup->suggest && options.suggest_text) {
        fprintf(stderr, "Note: only %zu different theme%s found for this request\n", run.made, run.made == 1 ? "" : "s");
    }
    if (status == 0) {
        CliPendingFile pending[MAX_FILES];
        const char *files[MAX_FILES];
        size_t pending_count = 0;
        status = write_files(&options, &run, pending, &pending_count);
        if (status == 0) {
            for (size_t i = 0; i < pending_count; i++) {
                files[i] = pending[i].final_path;
            }
            cJSON *document = api_grow_result_json(setup, &run, NULL);
            if (!document) {
                status = cli_fail(CLI_EXIT_USAGE, "out of memory");
            } else if (options.json) {
                cJSON *list = cJSON_AddArrayToObject(document, "files");
                for (size_t i = 0; list && i < pending_count; i++) {
                    cJSON_AddItemToArray(list, cJSON_CreateString(files[i]));
                }
                char *text = cJSON_PrintUnformatted(document);
                printf("%s\n", text ? text : "{}");
                cJSON_free(text);
            } else {
                cli_print_grow_text(document, setup->suggested || setup->suggest > 1, setup->seeded ? &setup->seed_found : NULL,
                                    files, pending_count);
            }
            cJSON_Delete(document);
        }
    }
    api_grow_run_destroy(&run);
    composer_style_destroy(style);
    return status;
}
