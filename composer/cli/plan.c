#include "plan.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <texture_rules.h>

#include "cli_options.h"

#include <ursatz_composer/form_template.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_json.h>

#define SIZE_SEARCH_MAX 96
/* The most given phrases, progressions and bass lines one plan takes, and the numerals in one progression. */
#define MAX_GIVEN 8
#define MAX_NUMERALS 64
/* The form a plan with given material and no --form is made as, and the section the material goes in. */
#define DEFAULT_GIVEN_BARS 24
#define DEFAULT_GIVEN_SECTION "B"

typedef enum {
    GIVEN_MELODY,
    GIVEN_PROGRESSION,
    GIVEN_BASS
} GivenKind;

/* One --given-* option and the --at and --cells that follow it. */
typedef struct {
    GivenKind kind;
    const char *source; /* a file, or the numerals of a progression */
    bool from_corpus;   /* --like-progression: source is looked for in the corpus and replaced by the passage's numerals */
    char found_numerals[1024];
    CorpusSeed found;
    const char *at_text;
    const char *cells_text;
    /* Read from the text above. */
    char letter[PLAN_SECTION_LETTER_MAX];
    unsigned long bar; /* from 1 */
    unsigned long cells;
} GivenOption;

typedef struct {
    bool help;
    const char *form_text;
    const char *key_text;
    const char *mode_text;
    const char *meter_text;
    const char *bars_text;
    const char *count_text;
    bool modulate;
    const char *motive_path;
    const char *from_corpus;
    const char *like_path;
    const char *accompaniment_text;
    const char *texture_text;
    const char *seed_text;
    const char *style_path;
    const char *style_weight_text;
    const char *blend_path;
    const char *blend_weight_text;
    const char *blend_weights_text;
    const char *out_path;
    const char *format_text;
    GivenOption given[MAX_GIVEN];
    size_t given_count;
    /* Read from the text above. */
    bool form_defaulted; /* no --form: given material makes it a ternary form of 24 bars */
    size_t given_melodies;
    FormTemplateKind form;
    PlanKey key;
    char key_name[24];
    PlanMeter meter;
    unsigned long bars;
    unsigned long count;
    bool has_accompaniment;
    PlanAccompaniment accompaniment;
    unsigned long seed;
    ComposerRatio style_weight; /* 0/0 when --style-weight was not given (a weight of 1/1 at fill) */
    ComposerStyleWeights blend_weights; /* read with --blend */
    bool seeded; /* the motive is a passage of the corpus, kept in seed */
    CorpusSeed seed_found;
    PlanSource source; /* seed_found as the plan records it */
    bool json;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", PLAN_USAGE_LINE + 7);
    fprintf(out, "Writes the plan of a whole short piece from a form template: its sections, their keys and cadences, the\n");
    fprintf(out, "sections that return an earlier one, and the motive. Nothing is composed yet; 'ursatz-composer fill'\n");
    fprintf(out, "realizes the plan.\n\n");
    fprintf(out, "Required:\n");
    fprintf(out, "  --form <name>        binary, rounded-binary, ternary, variations, loop or layered; with --given-* material\n");
    fprintf(out, "                       it may be left out, and the form is a ternary form of 24 bars\n");
    fprintf(out, "  --key <K>            the tonic (\"D\", \"F#\", \"Bb\"); or tonic and mode in one argument (\"D minor\")\n");
    fprintf(out, "  --meter <n/m>        2/4, 3/4, 4/4 or 6/8\n");
    fprintf(out, "  --out <plan.json>    where the plan is written\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --mode <major|minor> the mode when --key is a tonic only (default: major)\n");
    fprintf(out, "  --bars <n>           the size of the piece in bars; the sizes of each form are listed when one is refused\n");
    fprintf(out, "  --count <n>          the variations (variations) or the layers (layered), instead of --bars\n");
    fprintf(out, "  --modulate           plan the keys and cadences across the sections from the seed (otherwise every section\n");
    fprintf(out, "                       is in the home key and the cadences are the form's)\n");
    fprintf(out, "  --motive <file>      the motive: a MIDI file, a MusicXML file, a note list or a motive JSON file; without\n");
    fprintf(out, "                       it one is invented when the plan is filled\n");
    fprintf(out, "  --from-corpus <dir>  the output directory of ursatz-analyzer --corpus, for --like and --like-progression; the\n");
    fprintf(out, "                       passage used, its measures and its score are printed and kept in the plan (SOURCE). Exit 2\n");
    fprintf(out, "                       when no passage scores 1/2 or more\n");
    fprintf(out, "  --like <file>        the motive: the first bar of a passage of the corpus similar to this MIDI or MusicXML\n");
    fprintf(out, "                       passage (picked by the seed among the best), in --key; instead of --motive\n");
    fprintf(out, "  --like-progression \"<I IV V I>\"\n");
    fprintf(out, "                       a progression to place like --given-progression, but the chords are those of the passage of\n");
    fprintf(out, "                       the corpus that best matches these numerals; goes with --at (and --cells)\n");
    fprintf(out, "  --accompaniment <p>  block, arpeggiated, alberti (not in 6/8), oom-pah or any (a layered form has one in any\n");
    fprintf(out, "                       case)\n");
    fprintf(out, "  --texture <name>     the texture the accompaniment is to be heard as (see grow --help); implies\n");
    fprintf(out, "                       --accompaniment any\n");
    fprintf(out, "  --given-melody <f>   a phrase to place in the piece: a MIDI file, a MusicXML file, a note list or a motive JSON\n");
    fprintf(out, "                       file of any length; fill never changes it (repeatable)\n");
    fprintf(out, "  --given-progression \"<I IV V I>\"\n");
    fprintf(out, "                       Roman numerals, one chord each, to place in the piece (repeatable)\n");
    fprintf(out, "  --given-bass <f>     a bass line to place in the piece, the same file formats, no rests (repeatable)\n");
    fprintf(out, "  --at <L>[:<bar>]     where the --given-* before it goes: the section's letter (\"B\", \"A'\") and, from 1, the bar\n");
    fprintf(out, "                       of it the material starts in (default 1); without --form, the default is B:1\n");
    fprintf(out, "  --cells <n>          the cells each chord of the --given-progression before it lasts (default: a bar)\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same seed gives the same plan. Default: from the clock\n");
    fprintf(out, "  --style <file>       a style model file from the Ursatz Analyzer (--style-profile): fill prefers the harmony\n");
    fprintf(out, "                       and melody closest to it. The path is kept in the plan as given, so fill finds the file\n");
    fprintf(out, "                       from where it is run\n");
    fprintf(out, "  --style-weight <n/d> how strongly fill prefers the style (default 1/1; needs --style)\n");
    fprintf(out, "  --blend <file>       a second style model file, blended with --style (needs --style); kept in the plan\n");
    fprintf(out, "  --blend-weight <n/d> the second style's share in every dimension, 0 to 1 (default 1/2; needs --blend)\n");
    fprintf(out, "  --blend-weights harmony=<n/d>,rhythm=<n/d>,melody=<n/d>,form=<n/d>\n");
    fprintf(out, "                       the share per dimension; one not named takes --blend-weight\n");
    fprintf(out, "  --format <text|json> how to print the plan on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument (an unknown form, a size the form does not have, a bad\n");
    fprintf(out, "key or meter); 2 the request cannot be satisfied (an unreadable motive, a motive too long for a bar);\n");
    fprintf(out, "3 the plan could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer plan --form ternary --key \"A\" --mode minor --meter 3/4 --out piece.json\n");
    fprintf(out, "  ursatz-composer plan --form binary --key \"D minor\" --meter 4/4 --bars 8 --motive m.txt --out piece.json\n");
}

/* --- reading the options -------------------------------------------------- */

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--modulate") == 0) {
            options->modulate = true;
        } else if (strcmp(arg, "--form") == 0) {
            target = &options->form_text;
        } else if (strcmp(arg, "--key") == 0) {
            target = &options->key_text;
        } else if (strcmp(arg, "--mode") == 0) {
            target = &options->mode_text;
        } else if (strcmp(arg, "--meter") == 0) {
            target = &options->meter_text;
        } else if (strcmp(arg, "--bars") == 0) {
            target = &options->bars_text;
        } else if (strcmp(arg, "--count") == 0) {
            target = &options->count_text;
        } else if (strcmp(arg, "--motive") == 0) {
            target = &options->motive_path;
        } else if (strcmp(arg, "--from-corpus") == 0) {
            target = &options->from_corpus;
        } else if (strcmp(arg, "--like") == 0) {
            target = &options->like_path;
        } else if (strcmp(arg, "--accompaniment") == 0) {
            target = &options->accompaniment_text;
        } else if (strcmp(arg, "--texture") == 0) {
            target = &options->texture_text;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed_text;
        } else if (strcmp(arg, "--style") == 0) {
            target = &options->style_path;
        } else if (strcmp(arg, "--style-weight") == 0) {
            target = &options->style_weight_text;
        } else if (strcmp(arg, "--blend") == 0) {
            target = &options->blend_path;
        } else if (strcmp(arg, "--blend-weight") == 0) {
            target = &options->blend_weight_text;
        } else if (strcmp(arg, "--blend-weights") == 0) {
            target = &options->blend_weights_text;
        } else if (strcmp(arg, "--out") == 0) {
            target = &options->out_path;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format_text;
        } else if (strcmp(arg, "--given-melody") == 0 || strcmp(arg, "--given-progression") == 0 ||
                   strcmp(arg, "--given-bass") == 0 || strcmp(arg, "--like-progression") == 0) {
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", arg);
            }
            if (options->given_count == MAX_GIVEN) {
                return cli_fail(CLI_EXIT_USAGE, "plan takes %d given phrases, progressions and bass lines at most", MAX_GIVEN);
            }
            GivenOption *given = &options->given[options->given_count++];
            given->from_corpus = arg[2] == 'l';
            given->kind = given->from_corpus ? GIVEN_PROGRESSION
                          : arg[8] == 'm'    ? GIVEN_MELODY
                          : arg[8] == 'p'    ? GIVEN_PROGRESSION
                                             : GIVEN_BASS;
            given->source = argv[++i];
        } else if (strcmp(arg, "--at") == 0 || strcmp(arg, "--cells") == 0) {
            bool at = arg[2] == 'a';
            if (options->given_count == 0) {
                return cli_fail(CLI_EXIT_USAGE, "%s goes after a --given-melody, --given-progression or --given-bass", arg);
            }
            GivenOption *given = &options->given[options->given_count - 1];
            const char **slot = at ? &given->at_text : &given->cells_text;
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", arg);
            }
            if (*slot) {
                return cli_fail(CLI_EXIT_USAGE, "%s given twice for the same --given-*", arg);
            }
            *slot = argv[++i];
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer plan --help)", arg);
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unexpected argument '%s'; plan takes options only", arg);
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

static void append_number(char *out, size_t size, unsigned long value, bool first) {
    size_t used = strlen(out);
    snprintf(out + used, size - used, first ? "%lu" : ", %lu", value);
}

/* The sizes the form has, for the message about one it has not. */
static void describe_sizes(FormTemplateKind kind, char *out, size_t size) {
    FormTemplate layout;
    out[0] = '\0';
    bool first = true;
    for (unsigned long bars = 1; bars <= SIZE_SEARCH_MAX; bars++) {
        if (form_template_layout(kind, bars, 0, &layout)) {
            append_number(out, size, bars, first);
            first = false;
        }
    }
    size_t used = strlen(out);
    snprintf(out + used, size - used, " bars");
    bool counted = false;
    for (unsigned long count = 1; count <= FORM_TEMPLATE_MAX_SECTIONS; count++) {
        if (form_template_layout(kind, 0, count, &layout)) {
            used = strlen(out);
            snprintf(out + used, size - used, counted ? ", %lu" : " (or --count %lu", count);
            counted = true;
        }
    }
    if (counted) {
        used = strlen(out);
        snprintf(out + used, size - used, ")");
    }
}

/* The cells of a bar of the meter, which the harmony operators place chords on (harmony_rhythm.c). */
static unsigned long cells_in_bar(const PlanMeter *meter) {
    return (meter->beat_unit == 4 && meter->beats_per_bar == 4) || meter->beat_unit == 8 ? 2 : 1;
}

/* "B", "B:3", "A'" and "A':5": the section's letter and the bar (from 1) of it. */
static int read_at(GivenOption *given, bool form_defaulted) {
    if (!given->at_text) {
        if (!form_defaulted) {
            return cli_fail(CLI_EXIT_USAGE, "'%s' needs an --at: the section it goes in, \"B\" or \"B:3\"", given->source);
        }
        snprintf(given->letter, sizeof(given->letter), "%s", DEFAULT_GIVEN_SECTION);
        given->bar = 1;
        return 0;
    }
    const char *colon = strchr(given->at_text, ':');
    size_t length = colon ? (size_t)(colon - given->at_text) : strlen(given->at_text);
    if (length == 0 || length >= sizeof(given->letter)) {
        return cli_fail(CLI_EXIT_USAGE, "--at '%s' must be a section's letter, then optionally a colon and a bar from 1",
                        given->at_text);
    }
    snprintf(given->letter, sizeof(given->letter), "%.*s", (int)length, given->at_text);
    given->bar = 1;
    if (colon && (!cli_parse_whole_number(colon + 1, 100000, &given->bar) || given->bar < 1)) {
        return cli_fail(CLI_EXIT_USAGE, "--at '%s': the bar must be a whole number from 1", given->at_text);
    }
    return 0;
}

static int check_given(Options *options) {
    for (size_t i = 0; i < options->given_count; i++) {
        GivenOption *given = &options->given[i];
        int status = read_at(given, options->form_defaulted);
        if (status != 0) {
            return status;
        }
        if (given->cells_text && given->kind != GIVEN_PROGRESSION) {
            return cli_fail(CLI_EXIT_USAGE, "--cells goes with --given-progression, not '%s'", given->source);
        }
        given->cells = cells_in_bar(&options->meter);
        if (given->cells_text && (!cli_parse_whole_number(given->cells_text, 64, &given->cells) || given->cells < 1)) {
            return cli_fail(CLI_EXIT_USAGE, "--cells must be a whole number from 1, not '%s'", given->cells_text);
        }
        options->given_melodies += given->kind == GIVEN_MELODY;
    }
    return 0;
}

static int check_options(Options *options) {
    if (!options->form_text && options->given_count > 0) {
        /* Given material with no form to put it in: the middle of a ternary form, B. */
        options->form_text = "ternary";
        options->form_defaulted = true;
        options->bars = DEFAULT_GIVEN_BARS;
    }
    if (!options->form_text || !options->key_text || !options->meter_text || !options->out_path) {
        return cli_fail(CLI_EXIT_USAGE, "plan needs --form, --key, --meter and --out (see --help)\n%s", PLAN_USAGE_LINE);
    }
    if (!form_template_from_name(options->form_text, &options->form)) {
        char names[160] = "";
        for (int kind = 0; kind < FORM_TEMPLATE_KIND_COUNT; kind++) {
            size_t used = strlen(names);
            snprintf(names + used, sizeof(names) - used, kind ? ", %s" : "%s", form_template_name((FormTemplateKind)kind));
        }
        return cli_fail(CLI_EXIT_USAGE, "unknown form '%s'; the forms are %s", options->form_text, names);
    }
    /* The key is a tonic, with the mode from --mode (major by default), or tonic and mode together. */
    char key_text[64];
    bool has_mode_in_key = strchr(options->key_text, ' ') != NULL;
    if (has_mode_in_key && options->mode_text) {
        return cli_fail(CLI_EXIT_USAGE, "--key '%s' names a mode already; leave out --mode", options->key_text);
    }
    if (options->mode_text &&
        !cli_equals_ignore_case(options->mode_text, "major") && !cli_equals_ignore_case(options->mode_text, "minor")) {
        return cli_fail(CLI_EXIT_USAGE, "--mode must be major or minor, not '%s'", options->mode_text);
    }
    if (snprintf(key_text, sizeof(key_text), has_mode_in_key ? "%s" : "%s %s", options->key_text,
                 options->mode_text ? options->mode_text : "major") >= (int)sizeof(key_text) ||
        !cli_parse_key(key_text, &options->key)) {
        return cli_fail(CLI_EXIT_USAGE,
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, for example \"D\" with "
                        "--mode minor, or \"D minor\"",
                        options->key_text);
    }
    cli_key_name(&options->key, options->key_name, sizeof(options->key_name));
    if (!cli_parse_meter(options->meter_text, &options->meter) || !harmony_meter_supported(&options->meter)) {
        return cli_fail(CLI_EXIT_USAGE, "meter '%s' is not supported; use 2/4, 3/4, 4/4 or 6/8", options->meter_text);
    }
    if (options->bars_text && !cli_parse_whole_number(options->bars_text, SIZE_SEARCH_MAX * 8, &options->bars)) {
        return cli_fail(CLI_EXIT_USAGE, "--bars must be a whole number, not '%s'", options->bars_text);
    }
    if (options->count_text && !cli_parse_whole_number(options->count_text, 64, &options->count)) {
        return cli_fail(CLI_EXIT_USAGE, "--count must be a whole number, not '%s'", options->count_text);
    }
    FormTemplate layout;
    if (!form_template_layout(options->form, options->bars, options->count, &layout)) {
        char sizes[160];
        describe_sizes(options->form, sizes, sizeof(sizes));
        return cli_fail(CLI_EXIT_USAGE, "%s has no size of %s%s%s%s%s; it has %s", form_template_name(options->form),
                        options->bars_text ? options->bars_text : "", options->bars_text ? " bars" : "",
                        options->bars_text && options->count_text ? " with " : "",
                        options->count_text ? "--count " : "", options->count_text ? options->count_text : "", sizes);
    }
    int given_status = check_given(options);
    if (given_status != 0) {
        return given_status;
    }
    bool likes_progression = false;
    for (size_t i = 0; i < options->given_count; i++) {
        likes_progression = likes_progression || options->given[i].from_corpus;
    }
    if ((options->like_path || likes_progression) && !options->from_corpus) {
        return cli_fail(CLI_EXIT_USAGE, "%s needs --from-corpus <dir>", options->like_path ? "--like" : "--like-progression");
    }
    if (options->from_corpus && !options->like_path && !likes_progression) {
        return cli_fail(CLI_EXIT_USAGE, "--from-corpus needs --like <file> or --like-progression \"<numerals>\"");
    }
    if (options->like_path && options->motive_path) {
        return cli_fail(CLI_EXIT_USAGE, "--like and --motive both give the motive; use one");
    }
    if (options->accompaniment_text || options->texture_text) {
        options->has_accompaniment = true;
        int read = cli_read_accompaniment(options->accompaniment_text, options->texture_text, options->meter_text,
                                          &options->meter, &options->accompaniment);
        if (read != 0) {
            return read;
        }
    }
    if (options->style_weight_text && !options->style_path) {
        return cli_fail(CLI_EXIT_USAGE, "--style-weight needs --style");
    }
    if (options->style_weight_text && !cli_parse_ratio(options->style_weight_text, &options->style_weight)) {
        return cli_fail(CLI_EXIT_USAGE, "--style-weight must be a fraction like 1/2 or a whole number, not '%s'",
                        options->style_weight_text);
    }
    if (options->style_path && strlen(options->style_path) >= PLAN_STYLE_PATH_MAX) {
        return cli_fail(CLI_EXIT_USAGE, "--style path is longer than %d characters", PLAN_STYLE_PATH_MAX - 1);
    }
    if (options->blend_path && strlen(options->blend_path) >= PLAN_STYLE_PATH_MAX) {
        return cli_fail(CLI_EXIT_USAGE, "--blend path is longer than %d characters", PLAN_STYLE_PATH_MAX - 1);
    }
    int blend = cli_read_blend(options->style_path != NULL, options->blend_path, options->blend_weight_text,
                               options->blend_weights_text, &options->blend_weights);
    if (blend != 0) {
        return blend;
    }
    if (options->seed_text) {
        if (!cli_parse_whole_number(options->seed_text, CLI_SEED_MAX, &options->seed)) {
            return cli_fail(CLI_EXIT_USAGE, "--seed must be a whole number from 0 to 4294967295, not '%s'",
                            options->seed_text);
        }
    } else {
        options->seed = cli_default_seed();
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

/* --- reading the plan ------------------------------------------------------- */

static const char *const BEGINNING_NAMES[] = {"presentation", "antecedent", "compound basic idea"};
static const char *const ENDING_NAMES[] = {"continuation", "consequent", "cadential"};
static const char *const VARIATION_NAMES[] = {"exact", "ornament", "reharmonize", "reregister"};

void cli_pairing_name(const PlanThemePairing *pairing, char *out, size_t size) {
    snprintf(out, size, "%s + %s", BEGINNING_NAMES[pairing->beginning], ENDING_NAMES[pairing->ending]);
}

const char *cli_return_variation_name(PlanReturnVariation variation) {
    return VARIATION_NAMES[variation];
}

static bool has_constraint(const PiecePlan *plan, NodeId node, PlanConstraintKind kind) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        if (plan_node_constraint_at(plan, node, i)->kind == kind) {
            return true;
        }
    }
    return false;
}

/* The bars in a length; 0 when the plan has no meter. */
size_t cli_bars_in(const PiecePlan *plan, PlanTime length) {
    LaneSpanId meters[1];
    PlanSpan whole = plan_node_span(plan, plan_root(plan));
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, whole, meters, 1) == 0) {
        return 0;
    }
    const PlanMeter *meter = &plan_lane_span_payload(plan, meters[0])->meter;
    long numerator = length.num * (long)meter->beat_unit;
    long denominator = (long)length.den * meter->beats_per_bar;
    return denominator > 0 ? (size_t)((numerator + denominator / 2) / denominator) : 0;
}

size_t cli_piece_sections(const PiecePlan *plan, CliSection *out, size_t capacity) {
    size_t count = 0;
    NodeId root = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId node = plan_node_child_at(plan, root, i);
        if (plan_node_kind(plan, node) != PLAN_NODE_SECTION || has_constraint(plan, node, PLAN_CONSTRAINT_MATERIAL)) {
            continue;
        }
        if (count < capacity) {
            CliSection *section = &out[count];
            memset(section, 0, sizeof(*section));
            section->node = node;
            const char *name = plan_node_name(plan, node);
            snprintf(section->name, sizeof(section->name), "%s", name ? name : "");
            snprintf(section->letter, sizeof(section->letter), "%.*s", (int)sizeof(section->letter) - 1, section->name);
            section->bars = cli_bars_in(plan, plan_node_span(plan, node).length);
            for (size_t c = 0; c < plan_node_constraint_count(plan, node); c++) {
                const PlanConstraint *constraint = plan_node_constraint_at(plan, node, c);
                switch (constraint->kind) {
                case PLAN_CONSTRAINT_SECTION_LETTER:
                    memcpy(section->letter, constraint->params.letter, sizeof(section->letter));
                    break;
                case PLAN_CONSTRAINT_KEY:
                    section->has_key = true;
                    section->key = constraint->params.key;
                    break;
                case PLAN_CONSTRAINT_CADENCE:
                    if (section->cadence_count < 2) {
                        section->cadences[section->cadence_count++] = constraint->params.cadence;
                    }
                    break;
                case PLAN_CONSTRAINT_THEME_PAIRING:
                    section->has_pairing = true;
                    section->pairing = constraint->params.theme_pairing;
                    break;
                case PLAN_CONSTRAINT_RETURN: {
                    section->returns = true;
                    section->variation = constraint->params.section_return.variation;
                    const char *source = plan_node_name(plan, constraint->params.section_return.section);
                    snprintf(section->returns_letter, sizeof(section->returns_letter), "%.*s", (int)sizeof(section->returns_letter) - 1, source ? source : "");
                    for (size_t c2 = 0; c2 < plan_node_constraint_count(plan, constraint->params.section_return.section);
                         c2++) {
                        const PlanConstraint *letter =
                            plan_node_constraint_at(plan, constraint->params.section_return.section, c2);
                        if (letter->kind == PLAN_CONSTRAINT_SECTION_LETTER) {
                            memcpy(section->returns_letter, letter->params.letter, sizeof(section->returns_letter));
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }
        count++;
    }
    return count;
}

/* --- showing the plan --------------------------------------------------------- */

static void print_section_text(const CliSection *section) {
    char key[24] = "";
    if (section->has_key) {
        cli_key_name(&section->key, key, sizeof(key));
    }
    printf("  %-3s %2zu bars, key %s, cadence", section->letter, section->bars, section->has_key ? key : "-");
    printf(" %s", section->cadence_count ? cli_cadence_name(section->cadences[0]) : "-");
    if (section->cadence_count > 1) {
        /* An antecedent's weak cadence comes second in the plan, first in the music. */
        printf(" (antecedent %s)", cli_cadence_name(section->cadences[1]));
    }
    if (section->has_pairing) {
        char pairing[64];
        cli_pairing_name(&section->pairing, pairing, sizeof(pairing));
        printf(", %s", pairing);
    }
    if (section->returns) {
        printf(", returns %s (%s)", section->returns_letter, cli_return_variation_name(section->variation));
    }
    printf("\n");
}

static void print_section_json(const CliSection *section) {
    char key[24] = "";
    printf("{\"letter\":");
    cli_print_json_string(section->letter);
    printf(",\"bars\":%zu,\"key\":", section->bars);
    if (section->has_key) {
        cli_key_name(&section->key, key, sizeof(key));
        cli_print_json_string(key);
    } else {
        printf("null");
    }
    printf(",\"cadences\":[");
    for (size_t c = 0; c < section->cadence_count; c++) {
        printf("%s\"%s\"", c ? "," : "", cli_cadence_name(section->cadences[c]));
    }
    printf("],\"pairing\":");
    if (section->has_pairing) {
        char pairing[64];
        cli_pairing_name(&section->pairing, pairing, sizeof(pairing));
        cli_print_json_string(pairing);
    } else {
        printf("null");
    }
    printf(",\"returns\":");
    if (section->returns) {
        printf("{\"section\":");
        cli_print_json_string(section->returns_letter);
        printf(",\"variation\":\"%s\"}", cli_return_variation_name(section->variation));
    } else {
        printf("null");
    }
    printf("}");
}

static const char *const GIVEN_KIND_NAMES[] = {"melody", "progression", "bass"};

static void print_given_json(const Options *options) {
    printf(",\"given\":[");
    for (size_t i = 0; i < options->given_count; i++) {
        const GivenOption *given = &options->given[i];
        printf("%s{\"kind\":\"%s\",\"source\":", i ? "," : "", GIVEN_KIND_NAMES[given->kind]);
        cli_print_json_string(given->source);
        printf(",\"section\":");
        cli_print_json_string(given->letter);
        printf(",\"bar\":%lu", given->bar);
        if (given->kind == GIVEN_PROGRESSION) {
            printf(",\"cells\":%lu", given->cells);
        }
        if (given->from_corpus) {
            printf(",\"from\":");
            cli_print_seed_json(&given->found);
        }
        printf("}");
    }
    printf("]");
}

static void print_given_text(const Options *options) {
    if (options->form_defaulted) {
        printf("no --form: a ternary form of %lu bars, the given material in %s\n", options->bars, DEFAULT_GIVEN_SECTION);
    }
    for (size_t i = 0; i < options->given_count; i++) {
        const GivenOption *given = &options->given[i];
        printf("given %s %s%s%s at %s:%lu", GIVEN_KIND_NAMES[given->kind], given->kind == GIVEN_PROGRESSION ? "\"" : "",
               given->source, given->kind == GIVEN_PROGRESSION ? "\"" : "", given->letter, given->bar);
        if (given->kind == GIVEN_PROGRESSION) {
            printf(", %lu %s a chord", given->cells, given->cells == 1 ? "cell" : "cells");
        }
        printf("\n");
        if (given->from_corpus) {
            cli_print_seed_line("progression", &given->found);
        }
    }
}

static void print_result(const Options *options, const PiecePlan *plan) {
    CliSection sections[CLI_MAX_SECTIONS];
    size_t count = cli_piece_sections(plan, sections, CLI_MAX_SECTIONS);
    if (count > CLI_MAX_SECTIONS) {
        count = CLI_MAX_SECTIONS;
    }
    size_t bars = 0;
    for (size_t i = 0; i < count; i++) {
        bars += sections[i].bars;
    }
    char accompaniment[96] = "";
    if (options->has_accompaniment) {
        snprintf(accompaniment, sizeof(accompaniment), "%s", cli_accompaniment_name(options->accompaniment.pattern));
    }
    if (options->json) {
        printf("{\"form\":\"%s\",\"bars\":%zu,\"key\":\"%s\",\"meter\":\"%d/%d\",\"seed\":%lu,\"modulate\":%s,\"motive\":",
               form_template_name(options->form), bars, options->key_name, options->meter.beats_per_bar,
               options->meter.beat_unit, options->seed, options->modulate ? "true" : "false");
        if (options->motive_path) {
            cli_print_json_string(options->motive_path);
        } else {
            printf("null");
        }
        if (options->seeded) {
            printf(",\"motive_source\":");
            cli_print_seed_json(&options->seed_found);
        }
        printf(",\"accompaniment\":");
        if (options->has_accompaniment) {
            cli_print_json_string(accompaniment);
        } else {
            printf("null");
        }
        printf(",\"sections\":[");
        for (size_t i = 0; i < count; i++) {
            printf("%s", i ? "," : "");
            print_section_json(&sections[i]);
        }
        printf("]");
        if (options->given_count > 0) {
            print_given_json(options);
        }
        if (options->style_path) {
            printf(",\"style\":{\"file\":");
            cli_print_json_string(options->style_path);
            printf(",\"weight\":\"%lu/%lu\"}", options->style_weight.denominator ? options->style_weight.numerator : 1UL,
                   options->style_weight.denominator ? options->style_weight.denominator : 1UL);
            if (options->blend_path) {
                printf(",\"blend\":{\"file\":");
                cli_print_json_string(options->blend_path);
                printf(",\"weights\":{");
                for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
                    printf("%s\"%s\":\"%lu/%lu\"", d ? "," : "", style_dimension_name((StyleDimension)d),
                           options->blend_weights.numerator[d], options->blend_weights.denominator[d]);
                }
                printf("}}");
            }
        }
        printf(",\"file\":");
        cli_print_json_string(options->out_path);
        printf("}\n");
        return;
    }
    printf("%s, %zu bars, key %s, meter %d/%d, seed %lu%s\n", form_template_name(options->form), bars, options->key_name,
           options->meter.beats_per_bar, options->meter.beat_unit, options->seed,
           options->modulate ? ", keys and cadences planned" : "");
    if (options->seeded) {
        cli_print_seed_line("motive", &options->seed_found);
    } else if (options->motive_path) {
        printf("motive %s\n", options->motive_path);
    } else {
        printf("motive invented at fill\n");
    }
    if (options->has_accompaniment) {
        printf("accompaniment %s", accompaniment);
        if (options->accompaniment.texture >= 0) {
            printf(" (%s)", standard_texture_name((StandardTexture)options->accompaniment.texture));
        }
        printf("\n");
    }
    if (options->style_path) {
        printf("style %s, weight %lu/%lu\n", options->style_path,
               options->style_weight.denominator ? options->style_weight.numerator : 1UL,
               options->style_weight.denominator ? options->style_weight.denominator : 1UL);
        if (options->blend_path) {
            printf("blend %s, second style's share", options->blend_path);
            for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
                printf("%s %s %lu/%lu", d ? "," : "", style_dimension_name((StyleDimension)d),
                       options->blend_weights.numerator[d], options->blend_weights.denominator[d]);
            }
            printf("\n");
        }
    }
    print_given_text(options);
    printf("sections\n");
    for (size_t i = 0; i < count; i++) {
        print_section_text(&sections[i]);
    }
    printf("wrote %s\n", options->out_path);
}

/* --- the command ---------------------------------------------------------------- */

static int write_plan(const Options *options, const PiecePlan *plan) {
    char temporary[1032];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", options->out_path) >= (int)sizeof(temporary)) {
        return cli_fail(CLI_EXIT_WRITE, "file name '%s' is too long", options->out_path);
    }
    PlanJsonError error;
    if (plan_write_json_file(plan, temporary, &error) != PLAN_OK) {
        remove(temporary);
        return cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", options->out_path, error.message);
    }
    if (rename(temporary, options->out_path) != 0) {
        remove(temporary);
        return cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", options->out_path);
    }
    return 0;
}

/* --- the given material ------------------------------------------------------- */

/* Reads the file of a given melody or bass line, any length (a motive reader with no bar limit). */
static int load_given(const Options *options, const GivenOption *given, Motive *out) {
    MotiveOptions motive_options;
    MotiveError error;
    memset(&motive_options, 0, sizeof(motive_options));
    motive_options.has_meter = true;
    motive_options.meter = options->meter;
    motive_options.has_key = true;
    motive_options.key = options->key;
    MotiveResult loaded = motive_load_file(given->source, MOTIVE_FORMAT_AUTO, &motive_options, out, &error);
    if (loaded != MOTIVE_OK) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot load '%s' (%s)%s%s: %s", given->source, motive_result_name(loaded),
                        error.location[0] ? " at " : "", error.location, error.message);
    }
    return 0;
}

/* The node of the section with the letter, or PLAN_NODE_NONE; the letters are listed into names when it has none. */
static NodeId section_named(const PiecePlan *plan, const char *letter, char *names, size_t size) {
    CliSection sections[CLI_MAX_SECTIONS];
    size_t count = cli_piece_sections(plan, sections, CLI_MAX_SECTIONS);
    names[0] = '\0';
    for (size_t i = 0; i < count && i < CLI_MAX_SECTIONS; i++) {
        if (cli_equals_ignore_case(sections[i].letter, letter)) {
            return sections[i].node;
        }
        size_t used = strlen(names);
        snprintf(names + used, size - used, i ? ", %s" : "%s", sections[i].letter);
    }
    return PLAN_NODE_NONE;
}

/* Places the given material in the plan; the status of the failure it printed, else 0. */
static int place_given(const Options *options, PiecePlan *plan) {
    for (size_t i = 0; i < options->given_count; i++) {
        const GivenOption *given = &options->given[i];
        char names[128];
        NodeId section = section_named(plan, given->letter, names, sizeof(names));
        if (section == PLAN_NODE_NONE) {
            return cli_fail(CLI_EXIT_USAGE, "--at '%s': the piece has no section '%s'; its sections are %s",
                            given->at_text ? given->at_text : given->letter, given->letter, names);
        }
        GivenResult placed;
        if (given->kind == GIVEN_PROGRESSION) {
            char text[1024];
            char *numerals[MAX_NUMERALS];
            size_t count = 0;
            if (snprintf(text, sizeof(text), "%s", given->source) >= (int)sizeof(text)) {
                return cli_fail(CLI_EXIT_USAGE, "the progression is too long");
            }
            for (char *token = strtok(text, " \t,|"); token; token = strtok(NULL, " \t,|")) {
                if (count == MAX_NUMERALS) {
                    return cli_fail(CLI_EXIT_USAGE, "a progression has %d chords at most", MAX_NUMERALS);
                }
                numerals[count++] = token;
            }
            placed = plan_give_progression(plan, section, given->bar - 1, (const char *const *)numerals, count,
                                           given->cells);
        } else {
            Motive *line = malloc(sizeof(*line));
            if (!line) {
                return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
            }
            int status = load_given(options, given, line);
            if (status != 0) {
                free(line);
                return status;
            }
            placed = given->kind == GIVEN_MELODY ? plan_give_melody(plan, section, given->bar - 1, line->notes, line->note_count)
                                                 : plan_give_bass(plan, section, given->bar - 1, line->notes, line->note_count);
            free(line);
        }
        if (placed != GIVEN_OK) {
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot place the given %s '%s' at %s:%lu: %s",
                            GIVEN_KIND_NAMES[given->kind], given->source, given->letter, given->bar,
                            given_result_name(placed));
        }
        if (given->from_corpus) {
            int recorded = cli_add_source(plan, section, &given->found);
            if (recorded != 0) {
                return recorded;
            }
        }
    }
    return 0;
}

/* Checks the style file (and the blend, if any) reads; the plan keeps its path and weight on the form node for fill (PieceRequest.style). */
static int check_style(const Options *options) {
    ComposerStyle *style = NULL;
    int status = options->blend_path
                     ? cli_load_blend(options->style_path, options->blend_path, &options->blend_weights, NULL, 0, &style)
                     : cli_load_style(options->style_path, NULL, 0, &style);
    composer_style_destroy(style);
    return status;
}

int run_plan(int argc, char **argv) {
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

    Motive motive;
    bool has_motive = false;
    if (options.like_path) {
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
        has_motive = true;
    }
    for (size_t i = 0; i < options.given_count; i++) {
        GivenOption *given = &options.given[i];
        if (!given->from_corpus) {
            continue;
        }
        status = cli_seed_progression(options.from_corpus, given->source, options.key.mode, &given->found);
        if (status != 0) {
            return status;
        }
        cli_seed_numerals_text(&given->found, given->found_numerals, sizeof(given->found_numerals));
        given->source = given->found_numerals;
    }
    if (options.motive_path) {
        MotiveOptions motive_options;
        memset(&motive_options, 0, sizeof(motive_options));
        motive_options.has_meter = true;
        motive_options.meter = options.meter;
        motive_options.has_key = true;
        motive_options.key = options.key;
        MotiveError motive_error;
        MotiveResult loaded =
            motive_load_file(options.motive_path, MOTIVE_FORMAT_AUTO, &motive_options, &motive, &motive_error);
        if (loaded != MOTIVE_OK) {
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot load '%s' (%s)%s%s: %s", options.motive_path,
                            motive_result_name(loaded), motive_error.location[0] ? " at " : "", motive_error.location,
                            motive_error.message);
        }
        has_motive = true;
    }

    if (options.style_path) {
        status = check_style(&options);
        if (status != 0) {
            return status;
        }
    }

    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = options.form;
    request.key = options.key;
    request.meter = options.meter;
    request.bars = options.bars;
    request.count = options.count;
    request.motive = has_motive ? &motive : NULL;
    request.source = options.seeded ? &options.source : NULL;
    request.has_accompaniment = options.has_accompaniment;
    request.accompaniment = options.accompaniment;
    request.modulate = options.modulate;
    request.seed = options.seed;
    request.given_melodies = options.given_melodies;
    if (options.style_path) {
        request.has_style = true;
        snprintf(request.style.path, PLAN_STYLE_PATH_MAX, "%s", options.style_path);
        request.style.weight_numerator = options.style_weight.numerator;
        request.style.weight_denominator = options.style_weight.denominator;
        if (options.blend_path) {
            snprintf(request.style.blend_path, PLAN_STYLE_PATH_MAX, "%s", options.blend_path);
            for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
                request.style.blend_numerator[d] = options.blend_weights.numerator[d];
                request.style.blend_denominator[d] = options.blend_weights.denominator[d];
            }
        }
    }

    PiecePlan *plan = NULL;
    PieceResult built = piece_build_plan(&request, &plan);
    if (built != PIECE_OK) {
        return built == PIECE_ERR_MOTIVE_TOO_LONG
                   ? cli_fail(CLI_EXIT_UNSATISFIABLE, "the motive does not fit in one bar of %s", options.meter_text)
               : built == PIECE_ERR_BAD_SIZE
                   ? cli_fail(CLI_EXIT_USAGE, "%s has no such size", form_template_name(options.form))
               : built == PIECE_ERR_BAD_REQUEST
                   ? cli_fail(CLI_EXIT_UNSATISFIABLE,
                              "this piece cannot be planned as asked: the motive needs at least two notes, and each "
                              "section must be one the harmony operators can write in %s",
                              options.meter_text)
                   : cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", piece_result_name(built));
    }
    status = place_given(&options, plan);
    if (status == 0) {
        status = write_plan(&options, plan);
    }
    if (status == 0) {
        print_result(&options, plan);
    }
    plan_destroy(plan);
    return status;
}
