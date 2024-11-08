#include "fill.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <semantics/cadence_type.h>

#include "cli_options.h"
#include "plan.h"

#include <ursatz_composer/analysis_load.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>
#include <ursatz_composer/return_hear.h>

#define MAX_OUTPUTS 4
#define MAX_OPEN_SHOWN 24

typedef struct {
    bool help;
    const char *input;
    const char *seed_text;
    const char *out_path;
    const char *midi_path;
    const char *musicxml_path;
    const char *record_path;
    const char *format_text;
    unsigned long seed;
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
    fprintf(out, "  --out <file.json>    write the realized plan as JSON\n");
    fprintf(out, "  --midi <file>        write the piece as a MIDI file\n");
    fprintf(out, "  --musicxml <file>    write the piece as MusicXML\n");
    fprintf(out, "  --record <file.json> write the records of the stages (what each choice was and why) as one JSON array\n");
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
        } else if (strcmp(arg, "--out") == 0) {
            target = &options->out_path;
        } else if (strcmp(arg, "--midi") == 0) {
            target = &options->midi_path;
        } else if (strcmp(arg, "--musicxml") == 0) {
            target = &options->musicxml_path;
        } else if (strcmp(arg, "--record") == 0) {
            target = &options->record_path;
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

/* --- reading the plan --------------------------------------------------------- */

/* The plan in the file as JSON text, so that each seed can start from a new copy of it. */
static int load_plan_text(const char *path, char **text) {
    PiecePlan *plan = NULL;
    PlanJsonError error;
    PlanResult read = plan_read_json_file(path, &plan, &error);
    if (read != PLAN_OK) {
        /* A document of the Analyzer is not a plan: say where it is loaded. */
        PiecePlan *analysis = NULL;
        AnalysisLoadReport report = {0};
        PlanJsonError ignored;
        if (read == PLAN_ERR_FORMAT && plan_read_analysis_document_file(path, &analysis, &report, &ignored) == PLAN_OK) {
            plan_destroy(analysis);
            analysis_load_report_destroy(&report);
            return cli_fail(CLI_EXIT_UNSATISFIABLE,
                            "'%s' is an Ursatz-Analyzer analysis document, not a plan; it describes a piece that is already "
                            "written (load it with plan_read_analysis_document_file)",
                            path);
        }
        analysis_load_report_destroy(&report);
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
    if (heard->heard_result != FORM_HEAR_OK) {
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

static void print_result_text(unsigned long seed, bool realized, const PiecePlan *plan, const ComposeOutcome *outcome,
                              const char *const *files, size_t file_count) {
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
    printf("\nreturns heard %zu of %zu planned\n", outcome->returns_heard, outcome->returns_planned);
    for (size_t i = 0; i < outcome->returns.count; i++) {
        const ReturnHeardPair *pair = &outcome->returns.pairs[i];
        printf("  %s resembles %s (similarity %.2f)\n", letter_of(sections, count, pair->later),
               letter_of(sections, count, pair->earlier), pair->similarity);
    }
    for (size_t i = 0; i < file_count; i++) {
        printf("wrote %s\n", files[i]);
    }
}

static void print_result_json(unsigned long seed, bool realized, const PiecePlan *plan, const ComposeOutcome *outcome,
                              const char *const *files, size_t file_count) {
    CliSection sections[CLI_MAX_SECTIONS];
    size_t count = cli_piece_sections(plan, sections, CLI_MAX_SECTIONS);
    if (count > CLI_MAX_SECTIONS) {
        count = CLI_MAX_SECTIONS;
    }
    printf("{\"seed\":%lu,\"realized\":%s,\"sections\":[", seed, realized ? "true" : "false");
    for (size_t i = 0; i < outcome->section_count; i++) {
        const ComposeHeard *heard = &outcome->sections[i];
        char text[256];
        heard_text(heard, text, sizeof(text));
        printf("%s{\"letter\":", i ? "," : "");
        cli_print_json_string(letter_of(sections, count, heard->section));
        printf(",\"heard\":");
        cli_print_json_string(text);
        printf(",\"heard_as_requested\":%s,\"cadences\":[", heard->heard_as_requested ? "true" : "false");
        for (size_t c = 0; heard->heard_result == FORM_HEAR_OK && c < heard->heard.cadence_count; c++) {
            printf("%s\"%s\"", c ? "," : "", cadence_type_name((CadenceType)heard->heard.cadences[c].type));
        }
        printf("]}");
    }
    printf("],\"returns\":{\"planned\":%zu,\"heard\":%zu,\"pairs\":[", outcome->returns_planned, outcome->returns_heard);
    for (size_t i = 0; i < outcome->returns.count; i++) {
        const ReturnHeardPair *pair = &outcome->returns.pairs[i];
        printf("%s{\"later\":", i ? "," : "");
        cli_print_json_string(letter_of(sections, count, pair->later));
        printf(",\"earlier\":");
        cli_print_json_string(letter_of(sections, count, pair->earlier));
        printf(",\"similarity\":%.4f}", pair->similarity);
    }
    printf("]},\"files\":[");
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

static void report_stuck(const PiecePlan *plan, const ComposeOutcome *outcome, unsigned long first, unsigned long last) {
    fprintf(stderr, "Error: the plan could not be completed from seeds %lu to %lu: stuck at the %s stage%s\n", first, last,
            compose_stage_name(outcome->stuck_stage),
            outcome->key_rejected ? " (the keys or cadences heard did not fit the plan)" : "");
    size_t shown = 0;
    size_t open = 0;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (plan_node_state(plan, id) != PLAN_STATE_CONSTRAINED) {
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

typedef struct {
    const char *final_path;
    char temporary[1032];
} Pending;

static void discard(Pending *pending, size_t count) {
    for (size_t i = 0; i < count; i++) {
        remove(pending[i].temporary);
    }
}

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
static int write_files(const Options *options, const PiecePlan *plan, const ComposeOutcome *outcome, Pending *pending,
                       size_t *pending_count) {
    const char *paths[MAX_OUTPUTS] = {options->out_path, options->midi_path, options->musicxml_path, options->record_path};
    int status = 0;
    RenderedPlan rendered;
    bool have_rendering = false;
    *pending_count = 0;
    for (int kind = 0; kind < MAX_OUTPUTS && status == 0; kind++) {
        if (!paths[kind]) {
            continue;
        }
        Pending *entry = &pending[*pending_count];
        entry->final_path = paths[kind];
        if (snprintf(entry->temporary, sizeof(entry->temporary), "%s.tmp", paths[kind]) >= (int)sizeof(entry->temporary)) {
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
        } else {
            if (!have_rendering) {
                if (plan_render(plan, &rendered) != PLAN_RENDER_OK) {
                    status = cli_fail(CLI_EXIT_WRITE, "cannot render the plan");
                    break;
                }
                have_rendering = true;
            }
            PlanExportError error;
            PlanExportResult result = kind == 1 ? rendered_plan_export_midi(&rendered, entry->temporary, &error)
                                                : rendered_plan_export_musicxml(&rendered, entry->temporary, &error);
            if (result != PLAN_EXPORT_OK) {
                status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", paths[kind], error.message);
            }
        }
    }
    if (have_rendering) {
        rendered_plan_destroy(&rendered);
    }
    if (status != 0) {
        discard(pending, *pending_count);
        return status;
    }
    for (size_t i = 0; i < *pending_count; i++) {
        if (rename(pending[i].temporary, pending[i].final_path) != 0) {
            status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", pending[i].final_path);
            break;
        }
    }
    if (status != 0) {
        discard(pending, *pending_count);
    }
    return status;
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
    status = load_plan_text(options.input, &text);
    if (status != 0) {
        return status;
    }

    /* The seeds are tried one after another, each from a new copy of the plan; only a stuck run can be mended by the next. */
    PiecePlan *plan = NULL;
    ComposeOutcome outcome;
    memset(&outcome, 0, sizeof(outcome));
    bool done = false;
    bool realized = false;
    unsigned long used = options.seed;
    for (unsigned long attempt = 0; attempt < CLI_MAX_SEED_TRIES && !done && status == 0; attempt++) {
        PlanJsonError error;
        PiecePlan *fresh = NULL;
        if (plan_from_json(text, &fresh, &error) != PLAN_OK) {
            status = cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot read plan '%s': %s", options.input, error.message);
            break;
        }
        ComposeOutcome result;
        memset(&result, 0, sizeof(result));
        ComposeConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = (options.seed + attempt) & CLI_SEED_MAX;
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
        report_stuck(plan, &outcome, options.seed, used);
        status = CLI_EXIT_UNSATISFIABLE;
    }
    if (status == 0) {
        Pending pending[MAX_OUTPUTS];
        const char *files[MAX_OUTPUTS];
        size_t pending_count = 0;
        status = write_files(&options, plan, &outcome, pending, &pending_count);
        if (status == 0) {
            for (size_t i = 0; i < pending_count; i++) {
                files[i] = pending[i].final_path;
            }
            if (options.json) {
                print_result_json(used, realized, plan, &outcome, files, pending_count);
            } else {
                print_result_text(used, realized, plan, &outcome, files, pending_count);
            }
        }
    }
    if (plan) {
        plan_destroy(plan);
        compose_outcome_destroy(&outcome);
    }
    return status;
}
