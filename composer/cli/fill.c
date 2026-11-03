#include "fill.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <semantics/cadence_type.h>

#include "cli_options.h"
#include "plan.h"

#include <ursatz_analyzer/staleness.h>
#include "util/util.h"
#include <ursatz_composer/compose.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>
#include <ursatz_composer/return_hear.h>

#define MAX_OUTPUTS 5
#define MAX_OPEN_SHOWN 24
#define MAX_CANDIDATES 32

typedef struct {
    bool help;
    const char *input;
    const char *seed_text;
    const char *out_path;
    const char *midi_path;
    const char *musicxml_path;
    const char *tempo_text;
    bool plain_midi;
    PlanExportOptions export_options;
    const char *record_path;
    const char *explain_path;
    const char *format_text;
    const char *candidates_text;
    const char *style_path;
    CliStyleWithout without;
    const char *style_weight_text;
    const char *blend_path;
    const char *blend_weight_text;
    const char *blend_weights_text;
    unsigned long seed;
    unsigned long candidates; /* 0 for the single run */
    ComposerRatio style_weight; /* 0/0 unless --style-weight was given */
    bool check;
    bool json;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", FILL_USAGE_LINE + 7);
    fprintf(out, "Realizes a plan: composes every open theme section of <plan.json> (a plan from 'ursatz-composer plan' or\n");
    fprintf(out, "'grow --out') in stages, structure, motives, harmony, bass, melody, texture and the returns, and writes the\n");
    fprintf(out, "piece. A plan with no open theme section (one from 'grow --out' is written already) is only exported.\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same plan and seed give the same output. Default: from the\n");
    fprintf(out, "                       clock. The seed used is always printed (if a seed gets stuck, the next seeds are\n");
    fprintf(out, "                       tried, up to %d, and the one that worked is printed)\n", CLI_MAX_SEED_TRIES);
    fprintf(out, "  --check              analyze every theme section with the Ursatz Analyzer and reject a run whose analysis\n");
    fprintf(out, "                       contradicts the plan (a planned cadence or the key not heard, a hard voice-leading fault)\n");
    fprintf(out, "  --candidates <n>     compose n candidates (1 to %d, seeds from --seed) and write the best, ranked by agreement\n", MAX_CANDIDATES);
    fprintf(out, "                       with the Analyzer, then score, then the lower seed; every candidate is listed\n");
    fprintf(out, "  --style <file>       prefer the harmony and melody closest to a style: a style model file from the Ursatz\n");
    fprintf(out, "                       Analyzer (--style-profile). Overrides the plan's own style. The Analyzer's distance from the\n");
    fprintf(out, "                       style is printed with the result\n");
    fprintf(out, "  --style-weight <n/d> how strongly the style is preferred (default 1/1; overrides the plan's)\n");
    fprintf(out, "  --style-without <c>  read the style without the category c (repeatable, at most 8; overrides the plan's list)\n");
    fprintf(out, "  --blend <file>       a second style model file, blended with the style (--style, or the plan's). Overrides the\n");
    fprintf(out, "                       plan's blend; --style without --blend drops the plan's blend\n");
    fprintf(out, "  --blend-weight <n/d> the second style's share in every dimension, 0 to 1 (default 1/2; overrides the plan's)\n");
    fprintf(out, "  --blend-weights harmony=<n/d>,rhythm=<n/d>,melody=<n/d>,form=<n/d>\n");
    fprintf(out, "                       the share per dimension; one not named takes --blend-weight\n");
    fprintf(out, "  --out <file.json>    write the realized plan as JSON\n");
    fprintf(out, "  --midi <file>        write the piece as a MIDI file\n");
    fprintf(out, "  --musicxml <file>    write the piece as MusicXML\n");
    fprintf(out, "  --tempo <bpm>        MIDI and MusicXML: write a tempo, quarter notes per minute from 20 to 400 (a whole number or\n");
    fprintf(out, "                       a fraction like 195/2). Default: none, the file default of 120 applies\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --record <file.json> write the records of the stages (what each choice was and why) as one JSON array\n");
    fprintf(out, "  --explain <file>     write why each candidate was chosen or rejected: the chosen candidate, its runners-up and the\n");
    fprintf(out, "                       rejected ones with their reasons per target, the analyzer check and the form heard per section, and\n");
    fprintf(out, "                       the runs thrown away. JSON when the file ends in .json, else text\n");
    fprintf(out, "  --format <text|json> how to print the result on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "The form and cadences heard in each section, and the thematic returns heard, are printed with the result; a\n");
    fprintf(out, "section need not be heard as the form it was planned as, and that is reported, not an error. When no seed\n");
    fprintf(out, "completes the plan, the targets left open are printed with the reason each is stuck, and nothing is written.\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the plan cannot be read or cannot be completed;\n");
    fprintf(out, "3 an output file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer fill piece.json --seed 7 --midi piece.mid --musicxml piece.xml\n");
}

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed_text;
        } else if (strcmp(arg, "--check") == 0) {
            options->check = true;
        } else if (strcmp(arg, "--candidates") == 0) {
            target = &options->candidates_text;
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
        } else if (strcmp(arg, "--record") == 0) {
            target = &options->record_path;
        } else if (strcmp(arg, "--explain") == 0) {
            target = &options->explain_path;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format_text;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer fill --help)", arg);
        } else if (!options->input) {
            options->input = arg;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unexpected argument '%s'; the input is one plan file", arg);
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

static int check_options(Options *options) {
    if (!options->input) {
        return cli_fail(CLI_EXIT_USAGE, "fill needs a plan file\n%s", FILL_USAGE_LINE);
    }
    if (options->seed_text) {
        if (!request_parse_whole_number(options->seed_text, CLI_SEED_MAX, &options->seed)) {
            return cli_fail(CLI_EXIT_USAGE, "--seed must be a whole number from 0 to 4294967295, not '%s'",
                            options->seed_text);
        }
    } else {
        options->seed = cli_default_seed();
    }
    if (options->candidates_text) {
        if (!request_parse_whole_number(options->candidates_text, MAX_CANDIDATES, &options->candidates) ||
            options->candidates == 0) {
            return cli_fail(CLI_EXIT_USAGE, "--candidates must be a whole number from 1 to %d, not '%s'", MAX_CANDIDATES,
                            options->candidates_text);
        }
    }
    if (options->style_weight_text && !request_parse_ratio(options->style_weight_text, &options->style_weight)) {
        return cli_fail(CLI_EXIT_USAGE, "--style-weight must be a fraction like 1/2 or a whole number, not '%s'",
                        options->style_weight_text);
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

/* --- reading the plan --------------------------------------------------------- */

/* The plan in the file as JSON text, so that each seed can start from a new copy of it. */
int cli_load_plan_text(const char *path, char **text) {
    PiecePlan *plan = NULL;
    PlanJsonError error;
    PlanResult read = plan_read_json_file(path, &plan, &error);
    if (read != PLAN_OK) {
        /* A document of the Analyzer is not a plan: say where an analysis is loaded. */
        char *document = NULL;
        if (read == PLAN_ERR_FORMAT && util_read_file(path, &document, NULL)) {
            bool analysis = analyzer_document_status(document) != ANALYZER_DOCUMENT_UNREADABLE;
            free(document);
            if (analysis) {
                return cli_fail(CLI_EXIT_UNSATISFIABLE,
                                "'%s' is an Ursatz-Analyzer analysis document, not a plan; it describes a piece that is "
                                "already written (load an analysis with plan_from_analyzer_result)",
                                path);
            }
        }
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot read plan '%s': %s%s%s", path, error.message,
                        error.path[0] ? " at " : "", error.path);
    }
    *text = plan_to_json(plan, &error);
    plan_destroy(plan);
    if (!*text) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot read plan '%s': %s", path, error.message);
    }
    return 0;
}

/* --- showing the result --------------------------------------------------------- */

static const char *letter_of(const CliSection *sections, size_t count, NodeId node) {
    for (size_t i = 0; i < count; i++) {
        if (sections[i].node == node) {
            return sections[i].letter;
        }
    }
    return "?";
}

static void form_label(int form, char *out, size_t size) {
    snprintf(out, size, "%s", form >= 0 ? form_hear_form_name(form) : "none");
}

/* What was heard in a section, in words: "heard as period" or "heard as sentence, not period". */
static void heard_text(const ComposeHeard *heard, char *out, size_t size) {
    char requested[96];
    char found[96];
    form_label(heard->requested_form, requested, sizeof(requested));
    if (!heard->in_scope) {
        snprintf(out, size, "kept");
    } else if (heard->heard_result != FORM_HEAR_OK) {
        snprintf(out, size, "not heard (%s)", form_hear_result_name(heard->heard_result));
    } else if (heard->heard.count == 0 || heard->heard_form < 0) {
        snprintf(out, size, "no theme heard");
    } else {
        form_label(heard->heard_form, found, sizeof(found));
        if (heard->heard_as_requested) {
            snprintf(out, size, "heard as %s", found);
        } else {
            snprintf(out, size, "heard as %s, not %s", found, requested);
        }
    }
}

/* --- the given material ------------------------------------------------------------ */

#define MAX_GIVEN_RUNS 64

typedef struct {
    GivenReport runs[MAX_GIVEN_RUNS];
    size_t count;
    bool unchanged; /* every run is as it was given */
} GivenRuns;

static const char *given_lane_name(const GivenReport *run) {
    switch (run->lane) {
    case PLAN_LANE_HARMONY:
        return "progression";
    case PLAN_LANE_BASS:
        return "bass line";
    case PLAN_LANE_MELODY:
        return "melody";
    default:
        return "inner voice";
    }
}

/* The bar of its section a run starts in, from 1. */
static size_t given_bar(const PiecePlan *plan, const GivenReport *run) {
    Rational offset;
    PlanSpan section = plan_node_span(plan, run->owner);
    if (rational_sub(run->span.start, section.start, &offset) != RATIONAL_OK) {
        return 1;
    }
    return cli_bars_in(plan, offset) + 1;
}

static void print_given_text(const PiecePlan *plan, const CliSection *sections, size_t section_count,
                             const GivenRuns *given) {
    if (given->count == 0) {
        return;
    }
    printf("\ngiven material, %s\n", given->unchanged ? "unchanged" : "CHANGED");
    for (size_t i = 0; i < given->count && i < MAX_GIVEN_RUNS; i++) {
        const GivenReport *run = &given->runs[i];
        printf("  %s in %s from bar %zu, ", given_lane_name(run), letter_of(sections, section_count, run->owner),
               given_bar(plan, run));
        if (run->lane == PLAN_LANE_HARMONY) {
            printf("%zu %s", run->spans, run->spans == 1 ? "chord" : "chords");
        } else {
            printf("%zu %s", cli_bars_in(plan, run->span.length), cli_bars_in(plan, run->span.length) == 1 ? "bar" : "bars");
        }
        printf("%s\n", run->unchanged ? "" : ": changed");
    }
}

static void print_given_json(const PiecePlan *plan, const CliSection *sections, size_t section_count,
                             const GivenRuns *given) {
    printf(",\"given\":{\"unchanged\":%s,\"runs\":[", given->unchanged ? "true" : "false");
    for (size_t i = 0; i < given->count && i < MAX_GIVEN_RUNS; i++) {
        const GivenReport *run = &given->runs[i];
        printf("%s{\"kind\":\"%s\",\"section\":", i ? "," : "", given_lane_name(run));
        cli_print_json_string(letter_of(sections, section_count, run->owner));
        printf(",\"bar\":%zu,\"%s\":%zu,\"unchanged\":%s}", given_bar(plan, run),
               run->lane == PLAN_LANE_HARMONY ? "chords" : "bars",
               run->lane == PLAN_LANE_HARMONY ? run->spans : cli_bars_in(plan, run->span.length),
               run->unchanged ? "true" : "false");
    }
    printf("]}");
}

void cli_print_heard_json(const PiecePlan *plan, const ComposeOutcome *outcome) {
    CliSection sections[CLI_MAX_SECTIONS];
    size_t count = cli_piece_sections(plan, sections, CLI_MAX_SECTIONS);
    if (count > CLI_MAX_SECTIONS) {
        count = CLI_MAX_SECTIONS;
    }
    printf("[");
    for (size_t i = 0; i < outcome->section_count; i++) {
        const ComposeHeard *heard = &outcome->sections[i];
        char text[256];
        heard_text(heard, text, sizeof(text));
        printf("%s{\"letter\":", i ? "," : "");
        cli_print_json_string(letter_of(sections, count, heard->section));
        printf(",\"heard\":");
        if (heard->in_scope) {
            cli_print_json_string(text);
        } else {
            printf("null,\"in_scope\":false");
        }
        printf(",\"heard_as_requested\":%s,\"cadences\":[", heard->heard_as_requested ? "true" : "false");
        for (size_t c = 0; heard->heard_result == FORM_HEAR_OK && c < heard->heard.cadence_count; c++) {
            printf("%s\"%s\"", c ? "," : "", cadence_type_name((CadenceType)heard->heard.cadences[c].type));
        }
        printf("]}");
    }
    printf("]");
}

void cli_print_heard_text(const PiecePlan *plan, const ComposeOutcome *outcome) {
    CliSection sections[CLI_MAX_SECTIONS];
    size_t count = cli_piece_sections(plan, sections, CLI_MAX_SECTIONS);
    if (count > CLI_MAX_SECTIONS) {
        count = CLI_MAX_SECTIONS;
    }
    for (size_t i = 0; i < outcome->section_count; i++) {
        const ComposeHeard *heard = &outcome->sections[i];
        char text[256];
        heard_text(heard, text, sizeof(text));
        printf("%s%s %s", i ? "; " : "", letter_of(sections, count, heard->section), text);
        if (heard->heard_result == FORM_HEAR_OK && heard->heard.cadence_count > 0) {
            printf(", cadences");
            for (size_t c = 0; c < heard->heard.cadence_count; c++) {
                printf(c ? ", %s" : " %s", cadence_type_name((CadenceType)heard->heard.cadences[c].type));
            }
        }
    }
}


static void print_result_text(unsigned long seed, bool realized, const PiecePlan *plan, const ComposeOutcome *outcome,
                              const GivenRuns *given, const char *const *files, size_t file_count) {
    CliSection sections[CLI_MAX_SECTIONS];
    size_t count = cli_piece_sections(plan, sections, CLI_MAX_SECTIONS);
    if (count > CLI_MAX_SECTIONS) {
        count = CLI_MAX_SECTIONS;
    }
    if (realized) {
        printf("nothing to fill: the plan is already realized\n");
        for (size_t i = 0; i < file_count; i++) {
            printf("wrote %s\n", files[i]);
        }
        return;
    }
    printf("seed %lu\n", seed);
    printf("\nsections (form and cadences heard)\n");
    for (size_t i = 0; i < outcome->section_count; i++) {
        const ComposeHeard *heard = &outcome->sections[i];
        char text[256];
        heard_text(heard, text, sizeof(text));
        printf("  %-3s %s", letter_of(sections, count, heard->section), text);
        if (heard->heard_result == FORM_HEAR_OK && heard->heard.cadence_count > 0) {
            printf(", cadences");
            for (size_t c = 0; c < heard->heard.cadence_count; c++) {
                printf(c ? ", %s" : " %s", cadence_type_name((CadenceType)heard->heard.cadences[c].type));
            }
        }
        printf("\n");
    }
    if (outcome->analyzer_check_count > 0) {
        printf("\nanalyzer check: %zu of %zu planned cadences and keys heard as planned\n", outcome->analyzer_agreed,
               outcome->analyzer_planned);
    }
    if (outcome->style_measured) {
        cli_print_style_distance(&outcome->style_distance);
    }
    printf("\nreturns heard %zu of %zu planned\n", outcome->returns_heard, outcome->returns_planned);
    for (size_t i = 0; i < outcome->returns.count; i++) {
        const ReturnHeardPair *pair = &outcome->returns.pairs[i];
        printf("  %s resembles %s (similarity %.2f)\n", letter_of(sections, count, pair->later),
               letter_of(sections, count, pair->earlier), pair->similarity);
    }
    print_given_text(plan, sections, count, given);
    for (size_t i = 0; i < file_count; i++) {
        printf("wrote %s\n", files[i]);
    }
}

static void print_result_json(unsigned long seed, bool realized, const PiecePlan *plan, const ComposeOutcome *outcome,
                              const GivenRuns *given, const char *const *files, size_t file_count, const char *extra) {
    CliSection sections[CLI_MAX_SECTIONS];
    size_t count = cli_piece_sections(plan, sections, CLI_MAX_SECTIONS);
    if (count > CLI_MAX_SECTIONS) {
        count = CLI_MAX_SECTIONS;
    }
    printf("{\"seed\":%lu,\"realized\":%s%s%s,\"sections\":", seed, realized ? "true" : "false", extra ? "," : "",
           extra ? extra : "");
    cli_print_heard_json(plan, outcome);
    printf(",");
    if (outcome->analyzer_check_count > 0) {
        printf("\"analyzer\":{\"planned\":%zu,\"agreed\":%zu},", outcome->analyzer_planned, outcome->analyzer_agreed);
    }
    if (outcome->style_measured) {
        cli_print_style_distance_json(&outcome->style_distance);
        printf(",");
    }
    printf("\"returns\":{\"planned\":%zu,\"heard\":%zu,\"pairs\":[", outcome->returns_planned, outcome->returns_heard);
    for (size_t i = 0; i < outcome->returns.count; i++) {
        const ReturnHeardPair *pair = &outcome->returns.pairs[i];
        printf("%s{\"later\":", i ? "," : "");
        cli_print_json_string(letter_of(sections, count, pair->later));
        printf(",\"earlier\":");
        cli_print_json_string(letter_of(sections, count, pair->earlier));
        printf(",\"similarity\":%.4f}", pair->similarity);
    }
    printf("]}");
    if (given->count > 0) {
        print_given_json(plan, sections, count, given);
    }
    printf(",\"files\":[");
    for (size_t i = 0; i < file_count; i++) {
        printf("%s", i ? "," : "");
        cli_print_json_string(files[i]);
    }
    printf("]}\n");
}

/* --- a plan that is stuck ------------------------------------------------------- */

static const char *const NODE_KINDS[] = {"form", "section", "phrase", "subphrase", "idea", "motive"};
static const char *const LANES[] = {"meter", "key", "harmony", "bass", "melody", "inner voice"};

/* Why the stage left a target open: the last step the record has for it. */
static void stuck_reason(const ComposeOutcome *outcome, FillTarget target, char *out, size_t size) {
    snprintf(out, size, "not reached");
    const FillRecord *record = outcome->records[outcome->stuck_stage];
    for (size_t s = 0; record && s < record->step_count; s++) {
        const FillStep *step = &record->steps[s];
        if (!fill_target_equals(step->target, target)) {
            continue;
        }
        if (step->outcome == FILL_STEP_NO_OPERATOR) {
            snprintf(out, size, "no operator applies");
        } else if (step->outcome == FILL_STEP_ALL_REJECTED) {
            const char *why = "";
            for (size_t c = 0; c < step->considered_count; c++) {
                why = step->considered[c].reason[0] ? step->considered[c].reason : why;
            }
            snprintf(out, size, "every candidate was rejected%s%s", why[0] ? ": " : "", why);
        }
    }
}

void cli_report_stuck(const PiecePlan *plan, const ComposeOutcome *outcome, unsigned long first, unsigned long last) {
    fprintf(stderr, "Error: the plan could not be completed from seeds %lu to %lu: stuck at the %s stage%s\n", first, last,
            compose_stage_name(outcome->stuck_stage),
            outcome->key_rejected ? " (the keys or cadences heard did not fit the plan)" : "");
    size_t shown = 0;
    size_t open = 0;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (!plan_has_node(plan, id) || plan_node_state(plan, id) != PLAN_STATE_CONSTRAINED) {
            continue;
        }
        open++;
        if (shown < MAX_OPEN_SHOWN) {
            char reason[160];
            FillTarget target = {FILL_TARGET_NODE, id};
            stuck_reason(outcome, target, reason, sizeof(reason));
            const char *name = plan_node_name(plan, id);
            fprintf(stderr, "  open: %s %s: %s\n", NODE_KINDS[plan_node_kind(plan, id)], name ? name : "?", reason);
            shown++;
        }
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (!plan_has_lane_span(plan, id) || plan_lane_span_state(plan, id) != PLAN_STATE_CONSTRAINED) {
            continue;
        }
        open++;
        if (shown < MAX_OPEN_SHOWN) {
            char reason[160];
            FillTarget target = {FILL_TARGET_LANE_SPAN, id};
            stuck_reason(outcome, target, reason, sizeof(reason));
            const char *owner = plan_node_name(plan, plan_lane_span_owner(plan, id));
            fprintf(stderr, "  open: %s lane of %s: %s\n", LANES[plan_lane_span_lane(plan, id)], owner ? owner : "?", reason);
            shown++;
        }
    }
    if (open > shown) {
        fprintf(stderr, "  and %zu more\n", open - shown);
    }
}

/* --- writing the files ---------------------------------------------------------- */

static int write_record(const ComposeOutcome *outcome, const char *path) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        return -1;
    }
    bool ok = fputs("[", file) >= 0;
    bool first = true;
    for (int stage = 0; stage < COMPOSE_STAGE_COUNT && ok; stage++) {
        if (!outcome->records[stage]) {
            continue;
        }
        char *json = fill_record_to_json(outcome->records[stage]);
        if (!json) {
            ok = false;
            break;
        }
        ok = fprintf(file, "%s{\"stage\":\"%s\",\"record\":%s}", first ? "\n" : ",\n",
                     compose_stage_name((ComposeStage)stage), json) >= 0;
        first = false;
        free(json);
    }
    ok = ok && fputs("\n]\n", file) >= 0;
    return fclose(file) == 0 && ok ? 0 : -1;
}

/*
 * Writes every output to a temporary file next to its final name, and renames them all only when every one was
 * written, so that an error leaves no output file behind.
 */
static int write_files(const CliOutputPaths *output, const PiecePlan *plan, const ComposeOutcome *outcome, CliPendingFile *pending,
                       size_t *pending_count) {
    const char *paths[MAX_OUTPUTS] = {output->plan, output->midi, output->musicxml, output->record, output->explain};
    int status = 0;
    RenderedPlan rendered;
    bool have_rendering = false;
    *pending_count = 0;
    for (int kind = 0; kind < MAX_OUTPUTS && status == 0; kind++) {
        if (!paths[kind]) {
            continue;
        }
        CliPendingFile *entry = &pending[*pending_count];
        if (snprintf(entry->final_path, sizeof(entry->final_path), "%s", paths[kind]) >= (int)sizeof(entry->final_path) ||
            snprintf(entry->temporary, sizeof(entry->temporary), "%s.tmp", paths[kind]) >= (int)sizeof(entry->temporary)) {
            status = cli_fail(CLI_EXIT_WRITE, "file name '%s' is too long", paths[kind]);
            break;
        }
        (*pending_count)++;
        if (kind == 0) {
            PlanJsonError error;
            if (plan_write_json_file(plan, entry->temporary, &error) != PLAN_OK) {
                status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", paths[kind], error.message);
            }
        } else if (kind == 3) {
            if (write_record(outcome, entry->temporary) != 0) {
                status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", paths[kind]);
            }
        } else if (kind == 4) {
            Explanation explanation;
            ExplainResult built = explain_build(plan, outcome, &explanation);
            if (built != EXPLAIN_OK || cli_write_explanation(paths[kind], entry->temporary, &explanation) != 0) {
                status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", paths[kind]);
            }
            if (built == EXPLAIN_OK) {
                explain_destroy(&explanation);
            }
        } else {
            if (!have_rendering) {
                if (plan_export_render(plan, &rendered, NULL) != PLAN_EXPORT_OK) {
                    status = cli_fail(CLI_EXIT_WRITE, "cannot render the plan");
                    break;
                }
                have_rendering = true;
            }
            PlanExportError error;
            PlanExportResult result = kind == 1 ? rendered_plan_export_midi(&rendered, output->export_options, entry->temporary, &error)
                                                : rendered_plan_export_musicxml(&rendered, output->export_options, entry->temporary, &error);
            if (result != PLAN_EXPORT_OK) {
                status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", paths[kind], error.message);
            }
        }
    }
    if (have_rendering) {
        rendered_plan_destroy(&rendered);
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

/* --- shared with the commands that compose a plan again ------------------------ */

int cli_write_outputs(const CliOutputPaths *paths, const PiecePlan *plan, const ComposeOutcome *outcome,
                      const char **files, size_t *file_count) {
    CliPendingFile pending[MAX_OUTPUTS];
    size_t count = 0;
    int status = write_files(paths, plan, outcome, pending, &count);
    /* The paths are the caller's, in the order the files were written. */
    const char *given[MAX_OUTPUTS] = {paths->plan, paths->midi, paths->musicxml, paths->record, paths->explain};
    size_t next = 0;
    for (size_t kind = 0; status == 0 && kind < MAX_OUTPUTS; kind++) {
        if (given[kind]) {
            files[next++] = given[kind];
        }
    }
    *file_count = status == 0 ? count : 0;
    return status;
}

void cli_print_composed(bool json, unsigned long seed, const PiecePlan *plan, const ComposeOutcome *outcome,
                        const char *json_extra, const char *const *files, size_t file_count) {
    GivenRuns given;
    memset(&given, 0, sizeof(given));
    given.unchanged = true;
    if (json) {
        print_result_json(seed, false, plan, outcome, &given, files, file_count, json_extra);
    } else {
        print_result_text(seed, false, plan, outcome, &given, files, file_count);
    }
}

/* --- the candidates -------------------------------------------------------------- */

/* The written candidate's seed is `written`; the best is first, but the written one is named so a reader need not guess. */
static void print_candidates_text(const ComposeCandidates *candidates, unsigned long written) {
    if (candidates->count == 0) {
        return;
    }
    printf("\ncandidates (%zu of %zu completed, best first)\n", candidates->count, candidates->tried);
    for (size_t i = 0; i < candidates->count; i++) {
        const ComposeCandidate *candidate = &candidates->items[i];
        printf("  seed %lu  agreement %zu/%zu  score %lu/%lu%s\n", candidate->seed, candidate->agreed, candidate->planned,
               candidate->score.numerator, candidate->score.denominator, candidate->seed == written ? "  written" : "");
    }
    printf("\n");
}

#define CANDIDATE_JSON_SIZE (MAX_CANDIDATES * 120 + 64)

/* The members "candidates" of the result, as JSON text for print_result_json's extra. */
static void candidates_json(const ComposeCandidates *candidates, unsigned long written, char *out, size_t size) {
    size_t length = (size_t)snprintf(out, size, "\"candidates\":[");
    for (size_t i = 0; i < candidates->count && length < size; i++) {
        const ComposeCandidate *candidate = &candidates->items[i];
        length += (size_t)snprintf(out + length, size - length,
                                   "%s{\"seed\":%lu,\"agreed\":%zu,\"planned\":%zu,\"score\":\"%lu/%lu\",\"written\":%s}",
                                   i ? "," : "", candidate->seed, candidate->agreed, candidate->planned,
                                   candidate->score.numerator, candidate->score.denominator,
                                   candidate->seed == written ? "true" : "false");
    }
    if (length < size) {
        snprintf(out + length, size - length, "]");
    }
}

/* --- the style ------------------------------------------------------------------ */

/*
 * The style the run prefers: --style, else the STYLE constraint on the plan's form node, and the weight from
 * --style-weight, else the constraint's. *style stays NULL when there is none. Returns 0 or the usage status.
 */
static int resolve_style(const Options *options, const PiecePlan *plan, ComposerStyle **style, ComposerRatio *weight) {
    const PlanStyle *planned = NULL;
    NodeId root = plan_root(plan);
    for (size_t i = 0; root != PLAN_NODE_NONE && i < plan_node_constraint_count(plan, root); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, root, i);
        if (constraint->kind == PLAN_CONSTRAINT_STYLE) {
            planned = &constraint->params.style;
        }
    }
    const char *path = options->style_path ? options->style_path : (planned ? planned->path : NULL);
    /* --style-without is the list when given; else the plan keeps the categories its style was read without, and a --style
       of its own starts with none. */
    const char *without[PLAN_STYLE_WITHOUT_MAX];
    size_t without_count = 0;
    if (options->without.count > 0) {
        for (; without_count < options->without.count; without_count++) {
            without[without_count] = options->without.names[without_count];
        }
    } else if (planned && !options->style_path) {
        for (; without_count < planned->without_count; without_count++) {
            without[without_count] = planned->without[without_count];
        }
    }
    /* The plan's blend goes with the plan's style: a --style of its own is not blended unless --blend says so. */
    const char *blend_path = options->blend_path;
    if (!blend_path && !options->style_path && planned && planned->blend_path[0]) {
        blend_path = planned->blend_path;
    }
    ComposerStyleWeights blend_weights;
    bool weights_given = options->blend_path || options->blend_weight_text || options->blend_weights_text;
    if (!path) {
        if (options->style_weight_text) {
            return cli_fail(CLI_EXIT_USAGE, "--style-weight needs a style: give --style or a plan that has one");
        }
        if (options->without.count > 0) {
            return cli_fail(CLI_EXIT_USAGE, "--style-without needs a style: give --style or a plan that has one");
        }
        return cli_read_blend(false, blend_path, options->blend_weight_text, options->blend_weights_text, &blend_weights);
    }
    weight->numerator = planned ? planned->weight_numerator : 0;
    weight->denominator = planned ? planned->weight_denominator : 0;
    if (options->style_weight_text) {
        *weight = options->style_weight;
    }
    if (!blend_path) {
        int checked = cli_read_blend(true, NULL, options->blend_weight_text, options->blend_weights_text, &blend_weights);
        return checked != 0 ? checked : cli_load_style(path, without, without_count, style);
    }
    if (weights_given) {
        int read = cli_read_blend(true, blend_path, options->blend_weight_text, options->blend_weights_text,
                                  &blend_weights);
        if (read != 0) {
            return read;
        }
    } else {
        for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
            blend_weights.numerator[d] = planned->blend_numerator[d];
            blend_weights.denominator[d] = planned->blend_denominator[d];
        }
    }
    return cli_load_blend(path, blend_path, &blend_weights, without, without_count, style);
}

/* --- the command ---------------------------------------------------------------- */

int run_fill(int argc, char **argv) {
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
    char *text = NULL;
    status = cli_load_plan_text(options.input, &text);
    if (status != 0) {
        return status;
    }

    /* The seeds are tried one after another, each from a new copy of the plan; only a stuck run can be mended by the next. */
    PiecePlan *plan = NULL;
    ComposeOutcome outcome;
    memset(&outcome, 0, sizeof(outcome));
    GivenSnapshot *snapshot = NULL;
    bool done = false;
    bool realized = false;
    unsigned long used = options.seed;
    ComposeCandidates candidates;
    memset(&candidates, 0, sizeof(candidates));
    ComposerStyle *style = NULL;
    ComposerRatio style_weight = {0, 0};
    for (unsigned long attempt = 0; attempt < CLI_MAX_SEED_TRIES && !done && status == 0; attempt++) {
        PlanJsonError error;
        PiecePlan *fresh = NULL;
        if (plan_from_json(text, &fresh, &error) != PLAN_OK) {
            status = cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot read plan '%s': %s", options.input, error.message);
            break;
        }
        if (!snapshot) {
            /* What the user gave, kept to show after the run that none of it was changed. */
            snapshot = given_snapshot(fresh);
            status = resolve_style(&options, fresh, &style, &style_weight);
            if (status != 0) {
                plan_destroy(fresh);
                break;
            }
        }
        ComposeConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = (options.seed + attempt) & CLI_SEED_MAX;
        config.analyzer_check = options.check;
        config.style = style;
        config.style_weight = style_weight;
        if (options.candidates > 0) {
            /* The candidates run from the seed and the next ones; the best is written. */
            PieceResult ran = compose_candidates(fresh, &config, options.candidates, &candidates);
            if (ran == PIECE_ERR_BAD_REQUEST) {
                plan = fresh;
                realized = true;
                done = true;
            } else if (ran != PIECE_OK) {
                plan_destroy(fresh);
                status = cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", piece_result_name(ran));
            } else if (candidates.count == 0) {
                plan_destroy(fresh);
                status = cli_fail(CLI_EXIT_UNSATISFIABLE, "no candidate could be completed from seeds %lu to %lu",
                                  config.seed, (config.seed + candidates.tried - 1) & CLI_SEED_MAX);
            } else {
                plan_destroy(fresh);
                plan = candidates.items[0].plan;
                outcome = candidates.items[0].outcome;
                used = candidates.items[0].seed & CLI_SEED_MAX;
                candidates.items[0].plan = NULL;
                memset(&candidates.items[0].outcome, 0, sizeof(candidates.items[0].outcome));
                done = true;
            }
            break;
        }
        ComposeOutcome result;
        memset(&result, 0, sizeof(result));
        PieceResult ran = compose_run(fresh, &config, &result);
        if (plan) {
            plan_destroy(plan);
            compose_outcome_destroy(&outcome);
        }
        plan = fresh;
        outcome = result;
        used = config.seed;
        if (ran == PIECE_OK) {
            done = true;
        } else if (ran == PIECE_ERR_BAD_REQUEST) {
            /* No open theme section: the plan is written already (one from grow --out is), so it is only exported. */
            realized = true;
            done = true;
        } else if (ran != PIECE_ERR_STUCK) {
            status = cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", piece_result_name(ran));
        }
    }
    free(text);
    if (status == 0 && !done) {
        cli_report_stuck(plan, &outcome, options.seed, used);
        status = CLI_EXIT_UNSATISFIABLE;
    }
    GivenRuns given;
    memset(&given, 0, sizeof(given));
    given.unchanged = true;
    if (status == 0 && done && !realized && snapshot) {
        given.count = given_snapshot_report(snapshot, plan, given.runs, MAX_GIVEN_RUNS);
        given.count = given.count > MAX_GIVEN_RUNS ? MAX_GIVEN_RUNS : given.count;
        for (size_t i = 0; i < given.count; i++) {
            given.unchanged = given.unchanged && given.runs[i].unchanged;
        }
        if (!given.unchanged) {
            status = cli_fail(CLI_EXIT_UNSATISFIABLE, "the run changed the given material; nothing is written");
        }
    }
    given_snapshot_destroy(snapshot);
    if (status == 0) {
        CliPendingFile pending[MAX_OUTPUTS];
        const char *files[MAX_OUTPUTS];
        size_t pending_count = 0;
        CliOutputPaths output = {options.out_path,     options.midi_path,    options.musicxml_path,
                                 options.record_path,  options.explain_path, &options.export_options};
        status = write_files(&output, plan, &outcome, pending, &pending_count);
        if (status == 0) {
            const char *written[MAX_OUTPUTS] = {output.plan, output.midi, output.musicxml, output.record, output.explain};
            size_t next = 0;
            for (size_t kind = 0; kind < MAX_OUTPUTS; kind++) {
                if (written[kind]) {
                    files[next++] = written[kind];
                }
            }
            if (options.json) {
                char extra[CANDIDATE_JSON_SIZE];
                candidates_json(&candidates, used, extra, sizeof(extra));
                print_result_json(used, realized, plan, &outcome, &given, files, pending_count,
                                  candidates.count > 0 ? extra : NULL);
            } else {
                print_candidates_text(&candidates, used);
                print_result_text(used, realized, plan, &outcome, &given, files, pending_count);
            }
        }
    }
    compose_candidates_destroy(&candidates);
    if (plan) {
        plan_destroy(plan);
        compose_outcome_destroy(&outcome);
    }
    composer_style_destroy(style);
    return status;
}
