#include "plan.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <texture_rules.h>

#include "cli_options.h"

#include <ursatz_composer/form_template.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_json.h>

#define SIZE_SEARCH_MAX 96

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
    const char *accompaniment_text;
    const char *texture_text;
    const char *seed_text;
    const char *out_path;
    const char *format_text;
    /* Read from the text above. */
    FormTemplateKind form;
    PlanKey key;
    char key_name[24];
    PlanMeter meter;
    unsigned long bars;
    unsigned long count;
    bool has_accompaniment;
    PlanAccompaniment accompaniment;
    unsigned long seed;
    bool json;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", PLAN_USAGE_LINE + 7);
    fprintf(out, "Writes the plan of a whole short piece from a form template: its sections, their keys and cadences, the\n");
    fprintf(out, "sections that return an earlier one, and the motive. Nothing is composed yet; 'ursatz-composer fill'\n");
    fprintf(out, "realizes the plan.\n\n");
    fprintf(out, "Required:\n");
    fprintf(out, "  --form <name>        binary, rounded-binary, ternary, variations, loop or layered\n");
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
    fprintf(out, "  --accompaniment <p>  block, arpeggiated, alberti (not in 6/8), oom-pah or any (a layered form has one in any\n");
    fprintf(out, "                       case)\n");
    fprintf(out, "  --texture <name>     the texture the accompaniment is to be heard as (see grow --help); implies\n");
    fprintf(out, "                       --accompaniment any\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same seed gives the same plan. Default: from the clock\n");
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
        } else if (strcmp(arg, "--accompaniment") == 0) {
            target = &options->accompaniment_text;
        } else if (strcmp(arg, "--texture") == 0) {
            target = &options->texture_text;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed_text;
        } else if (strcmp(arg, "--out") == 0) {
            target = &options->out_path;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format_text;
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

static int check_options(Options *options) {
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
    if (options->accompaniment_text || options->texture_text) {
        options->has_accompaniment = true;
        int read = cli_read_accompaniment(options->accompaniment_text, options->texture_text, options->meter_text,
                                          &options->meter, &options->accompaniment);
        if (read != 0) {
            return read;
        }
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
static size_t bars_in(const PiecePlan *plan, PlanTime length) {
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
            section->bars = bars_in(plan, plan_node_span(plan, node).length);
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
        printf("],\"file\":");
        cli_print_json_string(options->out_path);
        printf("}\n");
        return;
    }
    printf("%s, %zu bars, key %s, meter %d/%d, seed %lu%s\n", form_template_name(options->form), bars, options->key_name,
           options->meter.beats_per_bar, options->meter.beat_unit, options->seed,
           options->modulate ? ", keys and cadences planned" : "");
    if (options->motive_path) {
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

    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = options.form;
    request.key = options.key;
    request.meter = options.meter;
    request.bars = options.bars;
    request.count = options.count;
    request.motive = has_motive ? &motive : NULL;
    request.has_accompaniment = options.has_accompaniment;
    request.accompaniment = options.accompaniment;
    request.modulate = options.modulate;
    request.seed = options.seed;

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
    status = write_plan(&options, plan);
    if (status == 0) {
        print_result(&options, plan);
    }
    plan_destroy(plan);
    return status;
}
