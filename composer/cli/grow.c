#include "grow.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-form/phrase_beginning.h>
#include <standard-form/phrase_ending.h>
#include <standard-form/phrase_level_form.h>
#include <standard-texture/texture_rules.h>

#include "cli_options.h"
#include "util/util.h"
#include "motive_extras.h"
#include "motive_input.h"

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>

#define MAX_SUGGESTIONS 16
#define MAX_FILES (4 * MAX_SUGGESTIONS)
#define MAX_CHORDS 96
#define MAX_LANE_SPANS 128

typedef enum { INTO_SENTENCE, INTO_PERIOD, INTO_HYBRID } Into;

typedef struct {
    bool help;
    const char *input;
    const char *from_corpus;
    const char *like_path;
    const char *into_text;
    const char *hybrid_text;
    const char *key_text;
    const char *mode_text;
    const char *meter_text;
    const char *bars_text;
    const char *cadence_text;
    const char *first_cadence_text;
    const char *accompaniment_text;
    const char *accompaniment_motive_path;
    const char *accompaniment_format_text;
    const char *accompaniment_pickup_text;
    const char *texture_text;
    bool voicing_flag;
    bool check;
    const char *style_path;
    const char *style_weight_text;
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
    ComposerRatio tempo;      /* quarter notes per minute from --tempo or the style; denominator 0 for none */
    const char *tempo_from;   /* "option", "style" or "none" */
    const char *mode_from;    /* "request" or "style" */
    GrowExt *ext;             /* the theme command's motives and output; NULL for grow itself */
    const char *explain_path;
    const char *format_text;
    MotiveExtras extras;
    /* Read from the text above. */
    Into into;
    PlanThemePairing pairing;
    AnalysisKey key;
    PlanMeter meter;
    size_t bars;
    CadenceType cadence;
    CadenceType first_cadence;
    bool has_accompaniment;
    PlanAccompaniment accompaniment;
    bool has_voicing;
    PlanVoicing voicing;
    bool has_accompaniment_motive;
    Motive accompaniment_motive; /* loaded from accompaniment_motive_path, role accompaniment */
    unsigned long seed;
    unsigned long suggest;
    ComposerRatio style_weight;
    ComposerStyleWeights blend_weights;
    ComposerStyle *style; /* loaded from style_path (blended with blend_path); NULL for none */
    bool seeded;          /* the motive is a passage of the corpus, kept in seed_found */
    CorpusSeed seed_found;
    PlanSource source; /* seed_found as the plan records it */
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
    fprintf(out, "                       tried, up to %d, and the one that worked is printed)\n", CLI_MAX_SEED_TRIES);
    fprintf(out, "  --suggest <n>        write n different themes (1 to %d) instead of one; with files, theme k goes to\n", MAX_SUGGESTIONS);
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

static const char *const BEGINNING_WORDS[] = {"presentation", "antecedent", "cbi"};
static const char *const BEGINNING_NAMES[] = {"presentation", "antecedent", "compound basic idea"};
static const char *const ENDING_NAMES[] = {"continuation", "consequent", "cadential"};

/* "antecedent+continuation": a beginning, a plus sign and an ending. */
static bool parse_hybrid(const char *text, PlanThemePairing *pairing) {
    const char *plus = strchr(text, '+');
    if (!plus) {
        return false;
    }
    size_t length = (size_t)(plus - text);
    bool beginning_found = false;
    bool ending_found = false;
    for (int i = 0; i < 3; i++) {
        if (strlen(BEGINNING_WORDS[i]) == length && strncmp(text, BEGINNING_WORDS[i], length) == 0) {
            pairing->beginning = (PlanThemeBeginning)i;
            beginning_found = true;
        }
        if (cli_equals_ignore_case(plus + 1, ENDING_NAMES[i])) {
            pairing->ending = (PlanThemeEnding)i;
            ending_found = true;
        }
    }
    return beginning_found && ending_found;
}

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
            target = &options->into_text;
        } else if (strcmp(arg, "--hybrid") == 0) {
            target = &options->hybrid_text;
        } else if (strcmp(arg, "--key") == 0) {
            target = &options->key_text;
        } else if (strcmp(arg, "--mode") == 0) {
            target = &options->mode_text;
        } else if (strcmp(arg, "--meter") == 0) {
            target = &options->meter_text;
        } else if (strcmp(arg, "--bars") == 0) {
            target = &options->bars_text;
        } else if (strcmp(arg, "--cadence") == 0) {
            target = &options->cadence_text;
        } else if (strcmp(arg, "--first-cadence") == 0) {
            target = &options->first_cadence_text;
        } else if (strcmp(arg, "--accompaniment") == 0) {
            target = &options->accompaniment_text;
        } else if (strcmp(arg, "--accompaniment-motive") == 0) {
            target = &options->accompaniment_motive_path;
        } else if (strcmp(arg, "--accompaniment-format") == 0) {
            target = &options->accompaniment_format_text;
        } else if (strcmp(arg, "--accompaniment-pickup") == 0) {
            target = &options->accompaniment_pickup_text;
        } else if (strcmp(arg, "--texture") == 0) {
            target = &options->texture_text;
        } else if (strcmp(arg, "--voicing") == 0) {
            options->voicing_flag = true;
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
            target = &options->style_weight_text;
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
    if (!options->input && !options->from_corpus && !options->ext) {
        return cli_fail(CLI_EXIT_USAGE, "grow needs a motive input\n%s", GROW_USAGE_LINE);
    }
    if (!options->into_text || !options->key_text || !options->meter_text) {
        return cli_fail(CLI_EXIT_USAGE, "grow needs --into, --key and --meter (see --help)");
    }
    if (cli_equals_ignore_case(options->into_text, "sentence")) {
        options->into = INTO_SENTENCE;
        options->pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
        options->pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    } else if (cli_equals_ignore_case(options->into_text, "period")) {
        options->into = INTO_PERIOD;
        options->pairing.beginning = PLAN_THEME_BEGINNING_ANTECEDENT;
        options->pairing.ending = PLAN_THEME_ENDING_CONSEQUENT;
    } else if (cli_equals_ignore_case(options->into_text, "hybrid")) {
        options->into = INTO_HYBRID;
    } else {
        return cli_fail(CLI_EXIT_USAGE, "--into must be sentence, period or hybrid, not '%s'", options->into_text);
    }
    if (options->into == INTO_HYBRID) {
        if (!options->hybrid_text) {
            return cli_fail(CLI_EXIT_USAGE,
                            "--into hybrid needs --hybrid <beginning>+<ending>, for example antecedent+continuation");
        }
        if (!parse_hybrid(options->hybrid_text, &options->pairing)) {
            return cli_fail(CLI_EXIT_USAGE,
                            "--hybrid must be a beginning (presentation, antecedent or cbi), a plus sign and an ending "
                            "(continuation, consequent or cadential), not '%s'",
                            options->hybrid_text);
        }
        bool sentence = options->pairing.beginning == PLAN_THEME_BEGINNING_PRESENTATION &&
                        options->pairing.ending == PLAN_THEME_ENDING_CONTINUATION;
        bool period = options->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT &&
                      options->pairing.ending == PLAN_THEME_ENDING_CONSEQUENT;
        if (sentence || period) {
            return cli_fail(CLI_EXIT_USAGE, "%s is a %s, not a hybrid; use --into %s", options->hybrid_text,
                            sentence ? "sentence" : "period", sentence ? "sentence" : "period");
        }
    } else if (options->hybrid_text) {
        return cli_fail(CLI_EXIT_USAGE, "--hybrid applies only to --into hybrid");
    }
    if (options->mode_text && !cli_equals_ignore_case(options->mode_text, "major") &&
        !cli_equals_ignore_case(options->mode_text, "minor") && !cli_equals_ignore_case(options->mode_text, "style")) {
        return cli_fail(CLI_EXIT_USAGE, "--mode must be major, minor or style, not '%s'", options->mode_text);
    }
    if (options->mode_text && cli_equals_ignore_case(options->mode_text, "style") && !options->style_path) {
        return cli_fail(CLI_EXIT_USAGE, "--mode style needs --style");
    }
    /* With --mode the key may be a tonic alone: the mode is the one --mode gives. */
    char tonic_key[64];
    const char *key_text = options->key_text;
    if (options->mode_text && strlen(key_text) + 7 < sizeof(tonic_key) && !strpbrk(key_text, " \t")) {
        snprintf(tonic_key, sizeof(tonic_key), "%s major", key_text);
        key_text = tonic_key;
    }
    if (!cli_parse_key(key_text, &options->key)) {
        return cli_fail(CLI_EXIT_USAGE,
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                        "for example \"D minor\"",
                        options->key_text);
    }
    if (options->mode_text && cli_equals_ignore_case(options->mode_text, "major")) {
        options->key.mode = KEY_STRUCTURE_MODE_MAJOR;
    } else if (options->mode_text && cli_equals_ignore_case(options->mode_text, "minor")) {
        options->key.mode = KEY_STRUCTURE_MODE_MINOR;
    }
    if (!cli_parse_meter(options->meter_text, &options->meter) || !harmony_meter_supported(&options->meter)) {
        return cli_fail(CLI_EXIT_USAGE, "meter '%s' is not supported; use 2/4, 3/4, 4/4 or 6/8", options->meter_text);
    }
    options->bars = 8;
    if (options->bars_text) {
        unsigned long bars;
        if (!cli_parse_whole_number(options->bars_text, 8, &bars) || (bars != 4 && bars != 8)) {
            return cli_fail(CLI_EXIT_USAGE, "--bars must be 4 or 8, not '%s'", options->bars_text);
        }
        options->bars = bars;
    }
    options->cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    if (options->cadence_text &&
        (!cli_parse_cadence(options->cadence_text, &options->cadence) || options->cadence == CADENCE_TYPE_DECEPTIVE ||
         options->cadence == CADENCE_TYPE_PLAGAL)) {
        return cli_fail(CLI_EXIT_USAGE, "--cadence must be PAC, IAC or HC, not '%s'", options->cadence_text);
    }
    options->first_cadence = CADENCE_TYPE_HALF;
    if (options->first_cadence_text) {
        if (options->pairing.beginning != PLAN_THEME_BEGINNING_ANTECEDENT) {
            return cli_fail(CLI_EXIT_USAGE, "--first-cadence applies only to a period or a hybrid that begins with an "
                                            "antecedent");
        }
        if (!cli_parse_cadence(options->first_cadence_text, &options->first_cadence) ||
            (options->first_cadence != CADENCE_TYPE_HALF && options->first_cadence != CADENCE_TYPE_IMPERFECT_AUTHENTIC)) {
            return cli_fail(CLI_EXIT_USAGE, "--first-cadence must be HC or IAC, not '%s'", options->first_cadence_text);
        }
    }
    /* The theme command's accompaniment motive is not a file. */
    bool accompaniment_motive = options->accompaniment_motive_path || (options->ext && options->ext->accompaniment_motive_given);
    /* A chorale is four parts with the melody written too: a voicing, not an accompaniment. */
    bool chorale = options->texture_text && cli_equals_ignore_case(options->texture_text, "chorale");
    if (chorale || options->voicing_flag) {
        if (options->accompaniment_text || (options->texture_text && !chorale)) {
            return cli_fail(CLI_EXIT_USAGE, "--voicing and --texture chorale write the inner voices themselves; they cannot "
                                            "be combined with --accompaniment or another --texture");
        }
        if (chorale && options->voicing_flag) {
            return cli_fail(CLI_EXIT_USAGE, "--texture chorale writes the melody, --voicing keeps the grown one; use one");
        }
        options->has_voicing = true;
        options->voicing.write_soprano = chorale;
    } else if (options->accompaniment_text || options->texture_text || accompaniment_motive) {
        /* The word "motive" names the accompaniment motive's pattern, which cli_read_accompaniment does not read. */
        bool motive_word = options->accompaniment_text && cli_equals_ignore_case(options->accompaniment_text, "motive");
        if (motive_word && !accompaniment_motive) {
            return cli_fail(CLI_EXIT_USAGE, "--accompaniment motive needs --accompaniment-motive <file>");
        }
        if (accompaniment_motive && options->accompaniment_text && !motive_word) {
            return cli_fail(CLI_EXIT_USAGE, "--accompaniment-motive is the accompaniment pattern; it cannot be combined with "
                                            "--accompaniment %s", options->accompaniment_text);
        }
        options->has_accompaniment = true;
        int read = cli_read_accompaniment(motive_word ? NULL : options->accompaniment_text, options->texture_text,
                                          options->meter_text, &options->meter, &options->accompaniment);
        if (read != 0) {
            return read;
        }
        if (accompaniment_motive) {
            options->accompaniment.pattern = PLAN_ACCOMPANIMENT_MOTIVE;
        }
    }
    if ((options->accompaniment_format_text || options->accompaniment_pickup_text) && !options->accompaniment_motive_path) {
        return cli_fail(CLI_EXIT_USAGE, "--accompaniment-format and --accompaniment-pickup need --accompaniment-motive <file>");
    }
    if (accompaniment_motive && (chorale || options->voicing_flag)) {
        return cli_fail(CLI_EXIT_USAGE, "--accompaniment-motive cannot be combined with --voicing or --texture chorale");
    }
    if (options->style_weight_text && !options->style_path) {
        return cli_fail(CLI_EXIT_USAGE, "--style-weight needs --style");
    }
    if (options->without.count > 0 && !options->style_path) {
        return cli_fail(CLI_EXIT_USAGE, "--style-without needs --style");
    }
    if (options->style_weight_text && !cli_parse_ratio(options->style_weight_text, &options->style_weight)) {
        return cli_fail(CLI_EXIT_USAGE, "--style-weight must be a fraction like 1/2 or a whole number, not '%s'",
                        options->style_weight_text);
    }
    int blend = cli_read_blend(options->style_path != NULL, options->blend_path, options->blend_weight_text,
                               options->blend_weights_text, &options->blend_weights);
    if (blend != 0) {
        return blend;
    }
    options->suggest = 1;
    if (options->suggest_text &&
        (!cli_parse_whole_number(options->suggest_text, MAX_SUGGESTIONS, &options->suggest) || options->suggest == 0)) {
        return cli_fail(CLI_EXIT_USAGE, "--suggest must be a whole number from 1 to %d, not '%s'", MAX_SUGGESTIONS,
                        options->suggest_text);
    }
    if (options->seed_text) {
        if (!cli_parse_whole_number(options->seed_text, CLI_SEED_MAX, &options->seed)) {
            return cli_fail(CLI_EXIT_USAGE, "--seed must be a whole number from 0 to 4294967295, not '%s'",
                            options->seed_text);
        }
    } else {
        options->seed = cli_default_seed();
    }
    {
        int export_status = cli_read_export_options(options->tempo_text, options->plain_midi, &options->export_options);
        if (export_status != 0) {
            return export_status;
        }
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

/* --- growing ----------------------------------------------------------------- */

typedef struct {
    PiecePlan *plan;
    GrowOutcome outcome;
    unsigned long seed;
    char *signature;
    AnalyzerCheck check; /* with --check, the Analyzer's check of the theme; empty otherwise */
    bool checked;
    size_t passed_before; /* how many of the seeds passed over came before this theme */
} Theme;

/* The seeds passed over, in the order tried (explain.h), kept for --explain. */
typedef struct {
    ComposeRejection *items;
    size_t count;
    long chord_bar; /* the bar of the first target a stuck run left open when it holds given chords, else -1 */
} Passed;

/* The bar (from 0) of a plan time in the theme. */
static long bar_at(Rational time, PlanSpan theme, Rational bar) {
    long long num = ((long long)time.num * theme.start.den - (long long)theme.start.num * time.den) * bar.den;
    long long den = (long long)time.den * theme.start.den * bar.num;
    return den > 0 && num >= 0 ? (long)(num / den) : 0;
}

/*
 * The bar (from 0) of a chord Given by the request that the earliest span the theme still has open lies in or beside, else
 * -1: the chords are what the stage could not write around.
 */
static long open_bar_with_chords(const PiecePlan *plan, NodeId theme_node, const PlanMeter *meter, const GrowExt *ext,
                                 GrowStage stage) {
    Rational bar;
    PlanSpan theme;
    PlanSpan open = {{0, 1}, {0, 1}};
    Rational open_end;
    bool any = false;
    /* Only the stages that write around the chords can be stopped by them. */
    PlanLaneKind lane = stage == GROW_STAGE_HARMONY ? PLAN_LANE_HARMONY
                        : stage == GROW_STAGE_BASS  ? PLAN_LANE_BASS
                                                    : PLAN_LANE_MELODY;
    if (ext->chord_count == 0 || theme_node == PLAN_NODE_NONE ||
        (stage != GROW_STAGE_HARMONY && stage != GROW_STAGE_BASS && stage != GROW_STAGE_MELODY) ||
        rational_make(meter->beats_per_bar, (long long)meter->beat_unit, &bar) != RATIONAL_OK) {
        return -1;
    }
    theme = plan_node_span(plan, theme_node);
    LaneSpanId ids[MAX_LANE_SPANS];
    size_t count = plan_lane_find(plan, lane, 0, theme, ids, MAX_LANE_SPANS);
    for (size_t i = 0; i < count && i < MAX_LANE_SPANS; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED &&
            (!any || rational_cmp(span.start, open.start) < 0)) {
            open = span;
            any = true;
        }
    }
    if (!any || plan_span_end(open, &open_end) != PLAN_OK) {
        return -1;
    }
    long first = bar_at(open.start, theme, bar);
    long last = bar_at(open_end, theme, bar);
    for (long at = first > 0 ? first - 1 : 0; at <= last; at++) {
        for (size_t c = 0; c < ext->chord_count; c++) {
            if (bar_at((Rational){ext->chords[c].onset.num, ext->chords[c].onset.den}, (PlanSpan){{0, 1}, {0, 1}}, bar) == at) {
                return at;
            }
        }
    }
    return -1;
}

static void passed_add(Passed *passed, const ComposeRejection *rejection) {
    ComposeRejection *grown = realloc(passed->items, (passed->count + 1) * sizeof(ComposeRejection));
    if (!grown) {
        free(rejection->items);
        return;
    }
    passed->items = grown;
    passed->items[passed->count++] = *rejection;
}

static void theme_destroy(Theme *theme) {
    plan_destroy(theme->plan);
    grow_outcome_destroy(&theme->outcome);
    analyzer_check_destroy(&theme->check);
    free(theme->signature);
    memset(theme, 0, sizeof(*theme));
}

static NodeId find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

typedef struct {
    char *text;
    size_t length;
    size_t capacity;
} Text;

static bool text_append(Text *text, const char *format, ...) __attribute__((format(printf, 2, 3)));

static bool text_append(Text *text, const char *format, ...) {
    char piece[256];
    va_list args;
    va_start(args, format);
    int written = vsnprintf(piece, sizeof(piece), format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= sizeof(piece)) {
        return false;
    }
    if (text->length + (size_t)written + 1 > text->capacity) {
        size_t capacity = text->capacity ? text->capacity * 2 : 1024;
        while (capacity < text->length + (size_t)written + 1) {
            capacity *= 2;
        }
        char *grown = realloc(text->text, capacity);
        if (!grown) {
            return false;
        }
        text->text = grown;
        text->capacity = capacity;
    }
    memcpy(text->text + text->length, piece, (size_t)written + 1);
    text->length += (size_t)written;
    return true;
}

/* Everything the harmony, the bass and the melody say over the theme, as text: two themes with the same text are one. */
static char *signature_of(const PiecePlan *plan, NodeId theme_node) {
    static const PlanLaneKind LANES[3] = {PLAN_LANE_HARMONY, PLAN_LANE_BASS, PLAN_LANE_MELODY};
    PlanSpan span = plan_node_span(plan, theme_node);
    Text text = {0};
    for (int lane = 0; lane < 3; lane++) {
        LaneSpanId ids[MAX_LANE_SPANS];
        size_t count = plan_lane_find(plan, LANES[lane], 0, span, ids, MAX_LANE_SPANS);
        for (size_t i = 0; i < count && i < MAX_LANE_SPANS; i++) {
            PlanSpan at = plan_lane_span_span(plan, ids[i]);
            const PlanLanePayload *payload = plan_lane_span_payload(plan, ids[i]);
            if (!text_append(&text, "|%d@%lld/%lld+%lld/%lld:", lane, at.start.num, at.start.den, at.length.num,
                             at.length.den)) {
                free(text.text);
                return NULL;
            }
            if (LANES[lane] == PLAN_LANE_HARMONY) {
                if (!text_append(&text, "%s", payload->harmony.roman_numeral)) {
                    free(text.text);
                    return NULL;
                }
                continue;
            }
            for (size_t n = 0; n < payload->notes.note_count; n++) {
                const PlanNote *note = &payload->notes.notes[n];
                if (!text_append(&text, "%d@%lld/%lld+%lld/%lld,", note->midi, note->offset.num, note->offset.den,
                                 note->duration.num, note->duration.den)) {
                    free(text.text);
                    return NULL;
                }
            }
        }
    }
    return text.text ? text.text : strdup("");
}

/* Builds the plan and runs the stages for one seed. GROW_ERR_STUCK is the only result a later seed can mend. */
static GrowResult grow_one(const Options *options, const Motive *motive, unsigned long seed, size_t attempt, Theme *theme,
                           Passed *passed) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.source = options->seeded ? &options->source : NULL;
    request.key = options->key;
    request.meter = options->meter;
    request.bars = options->bars;
    request.pairing = options->pairing;
    request.cadence = options->cadence;
    request.first_cadence = options->first_cadence;
    request.has_accompaniment = options->has_accompaniment;
    request.accompaniment = options->accompaniment;
    request.accompaniment_motive = options->has_accompaniment_motive ? &options->accompaniment_motive : NULL;
    request.has_voicing = options->has_voicing;
    request.voicing = options->voicing;
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; options->ext && slot < GROW_SLOT_COUNT; slot++) {
        request.slot_motives[slot] = options->ext->has_slot[slot] ? &options->ext->slot_motives[slot] : NULL;
    }
    for (int idea = 0; options->ext && idea < GROW_IDEA_COUNT; idea++) {
        request.idea_motives[idea] = options->ext->has_idea[idea] ? &options->ext->idea_motives[idea] : NULL;
    }
    if (options->ext) {
        request.chords = options->ext->chord_count > 0 ? options->ext->chords : NULL;
        request.chord_count = options->ext->chord_count;
    }
    memset(theme, 0, sizeof(*theme));
    theme->seed = seed;
    GrowResult built = grow_build_plan(&request, &theme->plan);
    if (built != GROW_OK) {
        return built;
    }
    GrowConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.basic = motive;
    config.style = options->style;
    config.style_weight = options->style_weight;
    config.tempo = options->tempo;
    if (options->ext) {
        config.pool = options->ext->pool;
        config.pool_count = options->ext->pool_count;
    }
    GrowResult ran = grow_run(theme->plan, &config, &theme->outcome);
    NodeId theme_node = find_node(theme->plan, "theme");
    if (ran == GROW_ERR_STUCK && options->ext) {
        passed->chord_bar = open_bar_with_chords(theme->plan, theme_node, &options->meter, options->ext,
                                                 theme->outcome.stuck_stage);
    }
    if (ran == GROW_ERR_STUCK && options->explain_path) {
        ComposeRejection rejection;
        memset(&rejection, 0, sizeof(rejection));
        rejection.round = attempt;
        rejection.seed = seed;
        snprintf(rejection.stage, sizeof(rejection.stage), "%s",
                 theme->outcome.stuck ? grow_stage_name(theme->outcome.stuck_stage) : "");
        snprintf(rejection.reason, sizeof(rejection.reason), "stuck");
        passed_add(passed, &rejection);
    }
    /* A theme the Analyzer cannot read is not held against it. */
    if (ran == GROW_OK && options->check && theme_node != PLAN_NODE_NONE) {
        theme->checked = analyzer_check_section(theme->plan, theme_node, &theme->check) == ANALYZER_CHECK_OK;
        if (theme->checked && theme->check.contradicts) {
            ComposeRejection rejection;
            if (options->explain_path &&
                explain_analyzer_rejection(theme->plan, theme_node, &theme->check, attempt, seed, &rejection) == EXPLAIN_OK) {
                passed_add(passed, &rejection);
            }
            ran = GROW_ERR_STUCK;
        }
    }
    if (ran == GROW_OK) {
        theme->signature = theme_node == PLAN_NODE_NONE ? NULL : signature_of(theme->plan, theme_node);
        if (!theme->signature) {
            ran = GROW_ERR_ALLOC;
        }
    }
    if (ran != GROW_OK) {
        theme_destroy(theme);
    }
    return ran;
}

/* --- showing the result --------------------------------------------------------- */

static const char *const KIND_NAMES[] = {"form", "section", "phrase", "subphrase", "idea", "motive"};

static double in_bars(Rational time, Rational bar) {
    return ((double)time.num / (double)time.den) / ((double)bar.num / (double)bar.den);
}

static const char *function_name(const PlanHarmony *harmony) {
    const char *name = harmony->has_function ? harmonic_function_name(harmony->function) : NULL;
    return name ? name : "unknown";
}

static void pitch_name(int midi, char *out, size_t size) {
    util_pitch_name(midi, false, out, size);
}

typedef struct {
    int bar;
    int beat;
    char roman[PLAN_ROMAN_NUMERAL_MAX];
    const char *function;
    char bass[48];
} Row;

/* The chords of the theme, one row each, with the bass under them. */
static size_t collect_rows(const Options *options, const PiecePlan *plan, NodeId theme, Row *rows) {
    PlanSpan span = plan_node_span(plan, theme);
    Rational bar;
    rational_make(options->meter.beats_per_bar, (long long)options->meter.beat_unit, &bar);
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, MAX_CHORDS);
    if (count > MAX_CHORDS) {
        count = MAX_CHORDS;
    }
    for (size_t i = 0; i < count; i++) {
        PlanSpan chord = plan_lane_span_span(plan, ids[i]);
        const PlanHarmony *harmony = &plan_lane_span_payload(plan, ids[i])->harmony;
        /* The offset from the start of the theme, as a fraction of a whole note, split into bars and the rest. */
        long off_num = chord.start.num * (long)span.start.den - span.start.num * (long)chord.start.den;
        unsigned long off_den = chord.start.den * span.start.den;
        long bars = (off_num * (long)bar.den) / ((long)off_den * bar.num);
        long rest_num = off_num * (long)bar.den - bars * bar.num * (long)off_den;
        unsigned long rest_den = off_den * bar.den;
        rows[i].bar = (int)bars + 1;
        rows[i].beat = (int)(rest_num * options->meter.beat_unit / (long)rest_den) + 1;
        memcpy(rows[i].roman, harmony->roman_numeral, PLAN_ROMAN_NUMERAL_MAX);
        rows[i].function = function_name(harmony);
        rows[i].bass[0] = '\0';
        LaneSpanId basses[2];
        if (plan_lane_find(plan, PLAN_LANE_BASS, 0, chord, basses, 2) == 1) {
            const PlanNotes *notes = &plan_lane_span_payload(plan, basses[0])->notes;
            size_t used = 0;
            for (size_t n = 0; n < notes->note_count && used + 8 < sizeof(rows[i].bass); n++) {
                char name[16];
                pitch_name(notes->notes[n].midi, name, sizeof(name));
                used += (size_t)snprintf(rows[i].bass + used, sizeof(rows[i].bass) - used, n ? " %s" : "%s", name);
            }
        }
    }
    return count;
}

static const char *transform_name(PlanMotiveTransform transform) {
    switch (transform) {
    case PLAN_TRANSFORM_TRANSPOSED:
        return "transposed";
    case PLAN_TRANSFORM_INVERTED:
        return "inverted";
    case PLAN_TRANSFORM_RETROGRADE:
        return "retrograde";
    case PLAN_TRANSFORM_AUGMENTED:
        return "augmented";
    case PLAN_TRANSFORM_DIMINISHED:
        return "diminished";
    case PLAN_TRANSFORM_FRAGMENTED:
        return "fragmented";
    case PLAN_TRANSFORM_RESTATED:
        return "restated";
    case PLAN_TRANSFORM_INTERVAL_CHANGED:
        return "interval changed";
    case PLAN_TRANSFORM_LIQUIDATED:
        return "liquidated";
    default:
        return "none";
    }
}

/* "restated, exact", "restated, 1 step up (tonal)", "fragmented, leading, 3 notes": the development the ref asks for. */
static void describe_ref(const PlanMotiveRef *ref, char *out, size_t size) {
    static const char *const POSITIONS[] = {"", ", leading", ", trailing", ", middle"};
    size_t used = (size_t)snprintf(out, size, "%s", transform_name(ref->transform));
    char *end = out + size;
    bool moved = false;
    if (ref->steps != 0 && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", %d step%s %s%s", ref->steps > 0 ? ref->steps : -ref->steps,
                                 ref->steps == 1 || ref->steps == -1 ? "" : "s", ref->steps > 0 ? "up" : "down",
                                 ref->tonal ? " (tonal)" : "");
        moved = true;
    } else if (ref->semitones != 0 && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", %+d semitones", ref->semitones);
        moved = true;
    }
    if (ref->transform == PLAN_TRANSFORM_RESTATED && !moved && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", exact");
    }
    if (ref->tonal && !moved && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", tonal");
    }
    if (ref->transform == PLAN_TRANSFORM_FRAGMENTED && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, "%s", POSITIONS[ref->position]);
        if (ref->length > 0 && used < size) {
            used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", %d notes", ref->length);
        }
    }
    if (ref->drop > 0 && used < size) {
        used += (size_t)snprintf(out + used, (size_t)(end - out) - used, ", drop %d", ref->drop);
    }
    if (ref->amount != 0 && used < size) {
        snprintf(out + used, (size_t)(end - out) - used, ", %+d semitones on interval %d", ref->amount, ref->interval);
    }
}

typedef struct {
    char motive[16];
    const char *idea;
    char how[96];
} Development;

#define MAX_DEVELOPMENTS 8

/* The motives made from m0 (every motive node with a motive_ref) and the idea each one is the melody of. */
static size_t collect_developments(const PiecePlan *plan, NodeId theme, Development *out) {
    size_t count = 0;
    LaneSpanId melodies[MAX_LANE_SPANS];
    size_t melody_count = plan_lane_find(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, theme), melodies, MAX_LANE_SPANS);
    for (NodeId id = 1; id <= plan_node_count(plan) && count < MAX_DEVELOPMENTS; id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) != PLAN_NODE_MOTIVE || !name || strcmp(name, "m0") == 0) {
            continue;
        }
        const PlanConstraint *ref = NULL;
        for (size_t c = 0; c < plan_node_constraint_count(plan, id) && !ref; c++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, id, c);
            ref = constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF ? constraint : NULL;
        }
        if (!ref) {
            continue;
        }
        snprintf(out[count].motive, sizeof(out[count].motive), "%s", name);
        out[count].idea = "";
        for (size_t m = 0; m < melody_count && m < MAX_LANE_SPANS; m++) {
            for (size_t c = 0; c < plan_lane_span_constraint_count(plan, melodies[m]); c++) {
                const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, melodies[m], c);
                if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && constraint->params.motive.motive == id) {
                    const char *idea = plan_node_name(plan, plan_lane_span_owner(plan, melodies[m]));
                    out[count].idea = idea ? idea : "";
                }
            }
        }
        describe_ref(&ref->params.motive, out[count].how, sizeof(out[count].how));
        count++;
    }
    return count;
}

/* "hybrid (compound basic idea + continuation)" for a hybrid with its beginning and ending, else the form's name. */
static void form_label(int form, int beginning, int ending, char *out, size_t size) {
    if (form == PHRASE_LEVEL_FORM_HYBRID && beginning >= 0 && ending >= 0) {
        snprintf(out, size, "%s (%s + %s)", form_hear_form_name(form), phrase_beginning_name((PhraseBeginning)beginning),
                 phrase_ending_name((PhraseEnding)ending));
    } else {
        snprintf(out, size, "%s", form >= 0 ? form_hear_form_name(form) : "none");
    }
}

/* What was heard, in words; heard_form and requested are the labels of the two forms. */
static void heard_text(const GrowOutcome *outcome, char *heard_form, size_t form_size, char *requested, size_t requested_size,
                       char *out, size_t size) {
    form_label(outcome->requested_form, outcome->requested_beginning, outcome->requested_ending, requested, requested_size);
    heard_form[0] = '\0';
    if (outcome->heard_result != FORM_HEAR_OK) {
        snprintf(out, size, "not heard (%s)", form_hear_result_name(outcome->heard_result));
    } else if (outcome->heard.count == 0) {
        snprintf(out, size, "no theme heard");
    } else {
        const FormHeardTheme *first = &outcome->heard.themes[0];
        form_label(first->form, first->beginning, first->ending, heard_form, form_size);
        if (outcome->heard_as_requested) {
            snprintf(out, size, "heard as %s", heard_form);
        } else {
            snprintf(out, size, "heard as %s, not %s", heard_form, requested);
        }
    }
}

/* A time as bars from the start of the theme. */
static double offset_in_bars(Rational time, PlanSpan theme, Rational bar) {
    Rational offset;
    offset.num = time.num * (long)theme.start.den - theme.start.num * (long)time.den;
    offset.den = time.den * theme.start.den;
    return in_bars(offset, bar);
}

static void print_node(const PiecePlan *plan, NodeId node, PlanSpan theme, Rational bar, int depth) {
    PlanSpan span = plan_node_span(plan, node);
    Rational end;
    rational_add(span.start, span.length, &end);
    const char *name = plan_node_name(plan, node);
    printf("%*s%s (%s) bars %g-%g\n", 2 + 2 * depth, "", name ? name : "?", KIND_NAMES[plan_node_kind(plan, node)],
           offset_in_bars(span.start, theme, bar), offset_in_bars(end, theme, bar));
    for (size_t i = 0; i < plan_node_child_count(plan, node); i++) {
        print_node(plan, plan_node_child_at(plan, node, i), theme, bar, depth + 1);
    }
}

static void print_json_node(const PiecePlan *plan, NodeId node, PlanSpan theme, Rational bar, bool *first) {
    PlanSpan span = plan_node_span(plan, node);
    Rational end;
    rational_add(span.start, span.length, &end);
    const char *name = plan_node_name(plan, node);
    printf("%s{\"name\":", *first ? "" : ",");
    cli_print_json_string(name ? name : "");
    printf(",\"kind\":\"%s\",\"start\":%g,\"end\":%g}", KIND_NAMES[plan_node_kind(plan, node)],
           offset_in_bars(span.start, theme, bar), offset_in_bars(end, theme, bar));
    *first = false;
    for (size_t i = 0; i < plan_node_child_count(plan, node); i++) {
        print_json_node(plan, plan_node_child_at(plan, node, i), theme, bar, first);
    }
}

static unsigned long percent(const GrowScore *score) {
    return score->denominator ? (100UL * score->numerator + score->denominator / 2) / score->denominator : 0;
}

static void print_theme_text(const Options *options, const Theme *theme, Rational bar) {
    const PiecePlan *plan = theme->plan;
    NodeId node = find_node(plan, "theme");
    printf("seed %lu\n", theme->seed);
    printf("\nstructure (bars from the start of the theme)\n");
    print_node(plan, node, plan_node_span(plan, node), bar, 0);

    Row rows[MAX_CHORDS];
    size_t count = collect_rows(options, plan, node, rows);
    printf("\n%-4s %-4s %-10s %-12s %s\n", "bar", "beat", "chord", "function", "bass");
    for (size_t i = 0; i < count; i++) {
        printf("%-4d %-4d %-10s %-12s %s\n", rows[i].bar, rows[i].beat, rows[i].roman, rows[i].function, rows[i].bass);
    }

    Development developments[MAX_DEVELOPMENTS];
    size_t development_count = collect_developments(plan, node, developments);
    printf("\nmotive developments (m0 is the motive given)\n");
    for (size_t i = 0; i < development_count; i++) {
        printf("  %s for %s: %s\n", developments[i].motive, developments[i].idea[0] ? developments[i].idea : "(unused)",
               developments[i].how);
    }
    if (options->ext) {
        options->ext->print_theme(options->ext, plan, &theme->outcome, false);
    }

    char heard_form[96];
    char requested[96];
    char heard[256];
    heard_text(&theme->outcome, heard_form, sizeof(heard_form), requested, sizeof(requested), heard, sizeof(heard));
    printf("\nstandard-form rule score %lu/%lu (%lu%%)\n", theme->outcome.score.numerator,
           theme->outcome.score.denominator, percent(&theme->outcome.score));
    printf("%s\n", heard);
    if (theme->outcome.style_measured) {
        cli_print_style_distance(&theme->outcome.style_distance);
    }
}

static void print_theme_json(const Options *options, const Theme *theme, Rational bar) {
    const PiecePlan *plan = theme->plan;
    NodeId node = find_node(plan, "theme");
    printf("{\"seed\":%lu,\"structure\":[", theme->seed);
    bool first = true;
    print_json_node(plan, node, plan_node_span(plan, node), bar, &first);
    printf("],\"chords\":[");
    Row rows[MAX_CHORDS];
    size_t count = collect_rows(options, plan, node, rows);
    for (size_t i = 0; i < count; i++) {
        printf("%s{\"bar\":%d,\"beat\":%d,\"chord\":\"%s\",\"function\":\"%s\",\"bass\":\"%s\"}", i ? "," : "",
               rows[i].bar, rows[i].beat, rows[i].roman, rows[i].function, rows[i].bass);
    }
    printf("],\"developments\":[");
    Development developments[MAX_DEVELOPMENTS];
    size_t development_count = collect_developments(plan, node, developments);
    for (size_t i = 0; i < development_count; i++) {
        printf("%s{\"motive\":\"%s\",\"idea\":", i ? "," : "", developments[i].motive);
        cli_print_json_string(developments[i].idea);
        printf(",\"development\":");
        cli_print_json_string(developments[i].how);
        printf("}");
    }
    printf("]");
    if (options->ext) {
        printf(",");
        options->ext->print_theme(options->ext, plan, &theme->outcome, true);
    }
    char heard_form[96];
    char requested[96];
    char heard[256];
    heard_text(&theme->outcome, heard_form, sizeof(heard_form), requested, sizeof(requested), heard, sizeof(heard));
    printf(",\"score\":{\"numerator\":%lu,\"denominator\":%lu},\"requested\":", theme->outcome.score.numerator,
           theme->outcome.score.denominator);
    cli_print_json_string(requested);
    printf(",\"heard\":");
    if (heard_form[0]) {
        cli_print_json_string(heard_form);
    } else {
        printf("null");
    }
    printf(",\"heard_as_requested\":%s,\"heard_text\":", theme->outcome.heard_as_requested ? "true" : "false");
    cli_print_json_string(heard);
    if (theme->outcome.style_measured) {
        printf(",");
        cli_print_style_distance_json(&theme->outcome.style_distance);
    }
    printf("}");
}

static void print_tempo_number(const ComposerRatio *tempo) {
    printf("tempo: ");
    if (tempo->denominator == 1) {
        printf("%lu", tempo->numerator);
    } else {
        printf("%g", (double)tempo->numerator / (double)tempo->denominator);
    }
}

static void print_brief(const Options *options, bool json) {
    GrowBrief brief;
    memset(&brief, 0, sizeof(brief));
    brief.tempo = options->tempo;
    brief.tempo_from = options->tempo_from;
    brief.minor = options->key.mode == KEY_STRUCTURE_MODE_MINOR;
    brief.mode_from = options->mode_from;
    brief.has_density = composer_style_density(options->style, &brief.density);
    brief.without = options->without.names;
    brief.without_count = options->without.count;
    options->ext->print_brief(options->ext, &brief, json);
}

static void print_result(const Options *options, const Theme *themes, size_t count, const char *const *files,
                         size_t file_count) {
    char key[24];
    cli_key_name(&options->key, key, sizeof(key));
    char pairing[64];
    snprintf(pairing, sizeof(pairing), "%s + %s", BEGINNING_NAMES[options->pairing.beginning],
             ENDING_NAMES[options->pairing.ending]);
    Rational bar;
    rational_make(options->meter.beats_per_bar, (long long)options->meter.beat_unit, &bar);
    if (options->json) {
        printf("{\"key\":\"%s\",\"meter\":\"%d/%d\",\"bars\":%zu,\"into\":\"%s\",\"pairing\":\"%s\",\"cadence\":\"%s\"",
               key, options->meter.beats_per_bar, options->meter.beat_unit, options->bars, options->into_text, pairing,
               cli_cadence_name(options->cadence));
        if (options->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT) {
            printf(",\"first_cadence\":\"%s\"", cli_cadence_name(options->first_cadence));
        }
        if (options->has_accompaniment) {
            printf(",\"accompaniment\":\"%s\"", cli_accompaniment_name(options->accompaniment.pattern));
            if (options->accompaniment.texture >= 0) {
                printf(",\"texture\":\"%s\"", standard_texture_name((StandardTexture)options->accompaniment.texture));
            }
        }
        if (options->has_voicing) {
            printf(",\"voicing\":\"%s\"", options->voicing.write_soprano ? "chorale" : "satb");
        }
        if (options->seeded) {
            printf(",\"source\":");
            cli_print_seed_json(&options->seed_found);
        }
        printf(",\"tempo\":{\"qpm\":");
        if (options->tempo.denominator > 0) {
            printf("%g", (double)options->tempo.numerator / (double)options->tempo.denominator);
        } else {
            printf("null");
        }
        printf(",\"from\":\"%s\"}", options->tempo_from);
        if (options->ext) {
            printf(",");
            print_brief(options, true);
        }
        printf(",\"themes\":[");
        for (size_t k = 0; k < count; k++) {
            printf("%s", k ? "," : "");
            print_theme_json(options, &themes[k], bar);
        }
        printf("],\"files\":[");
        for (size_t i = 0; i < file_count; i++) {
            printf("%s", i ? "," : "");
            cli_print_json_string(files[i]);
        }
        printf("]}\n");
        return;
    }
    printf("%s (%s), key %s, meter %d/%d, %zu bars, cadence %s", options->into_text, pairing, key,
           options->meter.beats_per_bar, options->meter.beat_unit, options->bars, cli_cadence_name(options->cadence));
    if (options->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT) {
        printf(", first cadence %s", cli_cadence_name(options->first_cadence));
    }
    if (options->has_accompaniment) {
        printf(", accompaniment %s", cli_accompaniment_name(options->accompaniment.pattern));
        if (options->accompaniment.texture >= 0) {
            printf(" (%s)", standard_texture_name((StandardTexture)options->accompaniment.texture));
        }
    }
    if (options->has_voicing) {
        printf(", %s", options->voicing.write_soprano ? "chorale (four parts)" : "voiced in four parts");
    }
    printf("\n");
    if (options->tempo.denominator > 0) {
        print_tempo_number(&options->tempo);
        printf("%s\n", strcmp(options->tempo_from, "style") == 0 ? " (from the style)" : "");
    }
    if (options->ext) {
        print_brief(options, false);
    }
    if (options->seeded) {
        cli_print_seed_line("motive", &options->seed_found);
    }
    for (size_t k = 0; k < count; k++) {
        if (options->suggest > 1 || options->suggest_text) {
            printf("\ntheme %zu of %zu\n", k + 1, count);
        } else {
            printf("\n");
        }
        print_theme_text(options, &themes[k], bar);
    }
    for (size_t i = 0; i < file_count; i++) {
        printf("wrote %s\n", files[i]);
    }
}

/* --- writing the files ---------------------------------------------------------- */

/*
 * Writes every output to a temporary file next to its final name, and renames them all only when every one was
 * written, so that an error leaves no output file behind.
 */
static int write_files(const Options *options, const Theme *themes, size_t count, const Passed *passed, CliPendingFile *pending,
                       size_t *pending_count) {
    const char *paths[4] = {options->out_path, options->midi_path, options->musicxml_path, options->explain_path};
    int status = 0;
    *pending_count = 0;
    for (size_t k = 0; k < count && status == 0; k++) {
        RenderedPlan rendered;
        bool have_rendering = false;
        for (int kind = 0; kind < 4 && status == 0; kind++) {
            if (!paths[kind]) {
                continue;
            }
            CliPendingFile *entry = &pending[*pending_count];
            if (!cli_alternative_path(paths[kind], k + 1, count > 1, entry->final_path, sizeof(entry->final_path)) ||
                snprintf(entry->temporary, sizeof(entry->temporary), "%s.tmp", entry->final_path) >=
                    (int)sizeof(entry->temporary)) {
                status = cli_fail(CLI_EXIT_WRITE, "file name '%s' is too long", paths[kind]);
                break;
            }
            (*pending_count)++;
            if (kind == 0) {
                PlanJsonError error;
                if (plan_write_json_file(themes[k].plan, entry->temporary, &error) != PLAN_OK) {
                    status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", entry->final_path, error.message);
                }
                continue;
            }
            if (kind == 3) {
                Explanation explanation;
                const Theme *theme = &themes[k];
                ExplainResult built =
                    explain_build_grow(theme->plan, find_node(theme->plan, "theme"), &theme->outcome,
                                       theme->checked ? &theme->check : NULL, passed->items, theme->passed_before,
                                       &explanation);
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
                if (plan_render(themes[k].plan, &rendered) != PLAN_RENDER_OK) {
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
    return run_grow_ext(argc, argv, NULL);
}

int run_grow_ext(int argc, char **argv, GrowExt *ext) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    options.ext = ext;
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    status = check_options(&options);
    if (status != 0) {
        return status;
    }

    if (options.style_path) {
        status = options.blend_path
                     ? cli_load_blend(options.style_path, options.blend_path, &options.blend_weights, options.without.names, options.without.count, &options.style)
                     : cli_load_style(options.style_path, options.without.names, options.without.count, &options.style);
        if (status != 0) {
            return status;
        }
    }
    options.tempo.numerator = 0;
    options.tempo.denominator = 0;
    options.tempo_from = "none";
    if (options.tempo_text) {
        options.tempo.numerator = options.export_options.tempo_numerator;
        options.tempo.denominator = options.export_options.tempo_denominator;
        options.tempo_from = "option";
    } else if (composer_style_tempo(options.style, &options.tempo)) {
        options.export_options.tempo_numerator = options.tempo.numerator;
        options.export_options.tempo_denominator = options.tempo.denominator;
        options.tempo_from = "style";
    }
    if (options.mode_text && cli_equals_ignore_case(options.mode_text, "style")) {
        bool minor = false;
        if (!composer_style_mode(options.style, &minor)) {
            return cli_fail(CLI_EXIT_USAGE, "--mode style needs a --style with a mode category");
        }
        options.key.mode = minor ? KEY_STRUCTURE_MODE_MINOR : KEY_STRUCTURE_MODE_MAJOR;
    }
    options.mode_from = options.mode_text && cli_equals_ignore_case(options.mode_text, "style") ? "style" : "request";

    MotiveOptions motive_options;
    memset(&motive_options, 0, sizeof(motive_options));
    motive_options.has_meter = true;
    motive_options.meter = options.meter;
    motive_options.has_key = true;
    motive_options.key = options.key;
    if (options.from_corpus && motive_extras_given(&options.extras)) {
        return cli_fail(CLI_EXIT_USAGE, "--harmony, --feature and --role describe a motive file, not one seeded from a corpus");
    }
    status = motive_extras_options(&options.extras, &options.meter, &motive_options);
    if (status != 0) {
        return status;
    }
    Motive motive;
    if (options.ext) {
        status = options.ext->load(options.ext, &options.meter, &options.key, &options.pairing);
        if (status != 0) {
            composer_style_destroy(options.style);
            return status;
        }
        motive = options.ext->basic;
    } else if (options.from_corpus) {
        /* The passage is chosen once, by the first seed; the seeds tried after it grow from the same motive. */
        status = cli_seed_motive(options.from_corpus, options.like_path, &options.meter, &options.key, options.seed,
                                 &options.seed_found);
        if (status != 0) {
            return status;
        }
        status = cli_seed_source(&options.seed_found, &options.source);
        if (status != 0) {
            return status;
        }
        options.seeded = true;
        motive = options.seed_found.motive;
    } else {
        MotiveError motive_error;
        MotiveResult loaded = motive_load_file(options.input, MOTIVE_FORMAT_AUTO, &motive_options, &motive, &motive_error);
        if (loaded != MOTIVE_OK) {
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot load '%s' (%s)%s%s: %s", options.input,
                            motive_result_name(loaded), motive_error.location[0] ? " at " : "", motive_error.location,
                            motive_error.message);
        }
    }

    if (options.ext && options.ext->has_accompaniment_motive) {
        options.accompaniment_motive = options.ext->accompaniment_motive;
        options.has_accompaniment_motive = true;
    } else if (options.accompaniment_motive_path) {
        MotiveInput input;
        memset(&input, 0, sizeof(input));
        input.path = options.accompaniment_motive_path;
        input.meter_text = options.meter_text;
        input.format_text = options.accompaniment_format_text;
        input.pickup_text = options.accompaniment_pickup_text;
        input.extras.role_text = "accompaniment";
        status = motive_input_load(&input, &options.accompaniment_motive);
        if (status != 0) {
            return status;
        }
        options.has_accompaniment_motive = true;
    }

    /* The seeds are tried one after another; a stuck run is passed over, and a theme that is the same as one already
       made is not counted. A request no seed can mend ends the search at once. */
    Theme themes[MAX_SUGGESTIONS];
    memset(themes, 0, sizeof(themes));
    size_t made = 0;
    Passed passed;
    memset(&passed, 0, sizeof(passed));
    passed.chord_bar = -1;
    unsigned long tries = (unsigned long)CLI_MAX_SEED_TRIES * options.suggest;
    unsigned long last_seed = options.seed;
    for (unsigned long attempt = 0; attempt < tries && made < options.suggest; attempt++) {
        Theme theme;
        last_seed = (options.seed + attempt) & CLI_SEED_MAX;
        GrowResult result = grow_one(&options, &motive, last_seed, attempt, &theme, &passed);
        if (result == GROW_OK) {
            bool repeated = false;
            for (size_t k = 0; k < made; k++) {
                repeated = repeated || strcmp(themes[k].signature, theme.signature) == 0;
            }
            if (repeated) {
                theme_destroy(&theme);
            } else {
                theme.passed_before = passed.count;
                themes[made++] = theme;
            }
        } else if (result != GROW_ERR_STUCK) {
            status = result == GROW_ERR_MOTIVE_TOO_LONG
                         ? cli_fail(CLI_EXIT_UNSATISFIABLE, "the motive does not fit in one bar of %s", options.meter_text)
                     : result == GROW_ERR_BAD_REQUEST
                         ? cli_fail(CLI_EXIT_UNSATISFIABLE,
                                    "this theme cannot be grown as asked: the motive needs at least two notes, an "
                                    "antecedent needs --first-cadence weaker than --cadence, and the pairing must be "
                                    "one the harmony operators can write in %s",
                                    options.meter_text)
                         : cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", grow_result_name(result));
            break;
        }
    }
    if (status == 0 && made == 0 && passed.chord_bar >= 0) {
        status = cli_fail(CLI_EXIT_UNSATISFIABLE, "the given chords in bar %ld do not allow the %s", passed.chord_bar,
                          options.cadence_text ? options.cadence_text : "PAC");
    } else if (status == 0 && made == 0) {
        status = cli_fail(CLI_EXIT_UNSATISFIABLE, "no theme could be grown for %s, %s, %zu bars, %s from seeds %lu to %lu",
                          options.key_text, options.meter_text, options.bars, options.into_text, options.seed,
                          last_seed);
    }
    if (status == 0 && made < options.suggest && options.suggest_text) {
        fprintf(stderr, "Note: only %zu different theme%s found for this request\n", made, made == 1 ? "" : "s");
    }

    if (status == 0) {
        CliPendingFile pending[MAX_FILES];
        const char *files[MAX_FILES];
        size_t pending_count = 0;
        status = write_files(&options, themes, made, &passed, pending, &pending_count);
        if (status == 0) {
            for (size_t i = 0; i < pending_count; i++) {
                files[i] = pending[i].final_path;
            }
            print_result(&options, themes, made, files, pending_count);
        }
    }
    for (size_t k = 0; k < made; k++) {
        theme_destroy(&themes[k]);
    }
    compose_rejections_free(passed.items, passed.count);
    composer_style_destroy(options.style);
    return status;
}
