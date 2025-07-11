#include "regenerate.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_options.h"
#include "fill.h"
#include "plan.h"

#include <ursatz_composer/compose.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/regenerate.h>

typedef struct {
    bool help;
    const char *input;
    CliTargetOptions target;
    const char *seed_text;
    const char *out_path;
    const char *midi_path;
    const char *musicxml_path;
    const char *tempo_text;
    bool plain_midi;
    PlanExportOptions export_options;
    const char *format_text;
    unsigned long seed;
    bool json;
} Options;

static void print_lock_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", LOCK_USAGE_LINE + 7);
    fprintf(out, "Locks material of a plan as Given: a node and everything under it (a section, a phrase, an idea), or, with\n");
    fprintf(out, "--lane, one lane of it. Nothing composes over Given material, and it is never regenerated. Locking is for\n");
    fprintf(out, "good in the plan; to unlock, go back to an earlier plan file.\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --node <id>          the node's id in the plan file (composer.id)\n");
    fprintf(out, "  --section <letter>   the section with that form letter (\"A\", \"B\", \"A'\") instead of --node\n");
    fprintf(out, "  --lane <lane>        harmony, bass, melody or inner: only that lane of the node and of what is under it\n");
    fprintf(out, "  --out <plan.json>    where the plan is written\n");
    fprintf(out, "  --format <text|json> how to print the result on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Only written material can be locked: a node with a lane span still open is refused.\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the plan cannot be read, the node is not found or has\n");
    fprintf(out, "open spans; 3 the output file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer lock piece.json --section A --out piece-locked.json\n");
}

static void print_regenerate_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", REGENERATE_USAGE_LINE + 7);
    fprintf(out, "Writes one node of a filled plan again and nothing but what depends on it. A section is built again whole\n");
    fprintf(out, "(its form and its returns too); a phrase or an idea keeps its shape and has its lanes written again; with\n");
    fprintf(out, "--lane, that lane of the node and what is written from it (the bass and melody over new chords, the next\n");
    fprintf(out, "chord or idea that joins them). Given material is never changed.\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --node <id>          the node's id in the plan file (composer.id)\n");
    fprintf(out, "  --section <letter>   the section with that form letter (\"A\", \"B\", \"A'\") instead of --node\n");
    fprintf(out, "  --lane <lane>        harmony, bass, melody or inner: only that lane of the node and of what is under it\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same plan, node and seed give the same output. Default: from\n");
    fprintf(out, "                       the clock. The seed used is always printed (if a seed gets stuck, the next seeds are\n");
    fprintf(out, "                       tried, up to %d, and the one that worked is printed)\n", CLI_MAX_SEED_TRIES);
    fprintf(out, "  --out <plan.json>    where the plan is written\n");
    fprintf(out, "  --midi <file>        write the piece as a MIDI file\n");
    fprintf(out, "  --musicxml <file>    write the piece as MusicXML\n");
    fprintf(out, "  --tempo <bpm>        MIDI and MusicXML: write a tempo, quarter notes per minute from 20 to 400 (a whole number or\n");
    fprintf(out, "                       a fraction like 195/2). Default: none, the file default of 120 applies\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --format <text|json> how to print the result on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "What was reopened, the seed, the form and cadences heard in each section written again, and the returns are\n");
    fprintf(out, "printed. The ids in a plan file run from 1: a plan written after a section was built again numbers its nodes\n");
    fprintf(out, "again, so read them from the file you are using.\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the plan cannot be read, the node is not found, is\n");
    fprintf(out, "Given or cannot be regenerated, or no seed completes the plan; 3 an output file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer regenerate piece.json --section B --seed 3 --out piece-2.json --midi piece-2.mid\n");
}

static int read_options(int argc, char **argv, Options *options, bool regenerate) {
    memset(options, 0, sizeof(*options));
    const char *command = regenerate ? "regenerate" : "lock";
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--node") == 0) {
            target = &options->target.node_text;
        } else if (strcmp(arg, "--section") == 0) {
            target = &options->target.section_text;
        } else if (strcmp(arg, "--lane") == 0) {
            target = &options->target.lane_text;
        } else if (strcmp(arg, "--out") == 0) {
            target = &options->out_path;
        } else if (regenerate && strcmp(arg, "--seed") == 0) {
            target = &options->seed_text;
        } else if (regenerate && strcmp(arg, "--midi") == 0) {
            target = &options->midi_path;
        } else if (regenerate && strcmp(arg, "--musicxml") == 0) {
            target = &options->musicxml_path;
        } else if (regenerate && strcmp(arg, "--tempo") == 0) {
            target = &options->tempo_text;
        } else if (regenerate && strcmp(arg, "--plain-midi") == 0) {
            options->plain_midi = true;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format_text;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer %s --help)", arg, command);
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

static int check_options(Options *options, bool regenerate) {
    const char *usage = regenerate ? REGENERATE_USAGE_LINE : LOCK_USAGE_LINE;
    const char *command = regenerate ? "regenerate" : "lock";
    if (!options->input) {
        return cli_fail(CLI_EXIT_USAGE, "%s needs a plan file\n%s", command, usage);
    }
    int status = cli_check_target(&options->target, command, usage);
    if (status != 0) {
        return status;
    }
    if (!options->out_path) {
        return cli_fail(CLI_EXIT_USAGE, "%s needs --out <plan.json>\n%s", command, usage);
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

/* --- the targets ---------------------------------------------------------------- */

static const char *const LANE_LABELS[] = {"meter", "key", "harmony", "bass", "melody", "inner voice"};

/* --- lock ------------------------------------------------------------------------ */

static size_t count_given(const PiecePlan *plan, size_t *nodes) {
    size_t spans = 0;
    *nodes = 0;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        *nodes += plan_has_node(plan, id) && plan_node_state(plan, id) == PLAN_STATE_GIVEN;
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        spans += plan_has_lane_span(plan, id) && plan_lane_span_state(plan, id) == PLAN_STATE_GIVEN;
    }
    return spans;
}

int run_lock(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options, false);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_lock_help(stdout);
        return 0;
    }
    status = check_options(&options, false);
    if (status != 0) {
        return status;
    }
    PiecePlan *plan = NULL;
    PlanJsonError error;
    if (plan_read_json_file(options.input, &plan, &error) != PLAN_OK) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot read plan '%s': %s%s%s", options.input, error.message,
                        error.path[0] ? " at " : "", error.path);
    }
    FillTarget *targets = NULL;
    size_t target_count = 0;
    status = cli_find_targets(plan, &options.target, &targets, &target_count);
    char what[96];
    cli_describe_target(plan, &options.target, what, sizeof(what));
    size_t nodes_before;
    size_t spans_before = count_given(plan, &nodes_before);
    for (size_t t = 0; status == 0 && t < target_count; t++) {
        PlanResult locked = plan_lock(plan, targets[t]);
        if (locked == PLAN_ERR_BAD_PAYLOAD) {
            status = cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot lock %s: some of it is not written yet (run fill first)", what);
        } else if (locked != PLAN_OK) {
            status = cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot lock %s", what);
        }
    }
    free(targets);
    if (status == 0) {
        PlanJsonError write_error;
        if (plan_write_json_file(plan, options.out_path, &write_error) != PLAN_OK) {
            status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", options.out_path, write_error.message);
        } else {
            size_t nodes_after;
            size_t spans_after = count_given(plan, &nodes_after);
            if (options.json) {
                printf("{\"locked\":{\"nodes\":%zu,\"spans\":%zu},\"plan\":", nodes_after - nodes_before,
                       spans_after - spans_before);
                cli_print_json_string(options.out_path);
                printf("}\n");
            } else {
                printf("locked %s: %zu %s and %zu lane %s now Given\n", what, nodes_after - nodes_before,
                       nodes_after - nodes_before == 1 ? "node" : "nodes", spans_after - spans_before,
                       spans_after - spans_before == 1 ? "span" : "spans");
                printf("wrote %s\n", options.out_path);
            }
        }
    }
    plan_destroy(plan);
    return status;
}

/* --- regenerate -------------------------------------------------------------------- */

typedef struct {
    size_t sections;
    char letters[CLI_MAX_SECTIONS][PLAN_SECTION_LETTER_MAX];
    size_t spans[PLAN_LANE_INNER_VOICE + 1];
} Reopened;

static void read_reopened(const PiecePlan *plan, const FillTarget *scope, size_t count, Reopened *out) {
    CliSection sections[CLI_MAX_SECTIONS];
    size_t section_count = cli_piece_sections(plan, sections, CLI_MAX_SECTIONS);
    section_count = section_count > CLI_MAX_SECTIONS ? CLI_MAX_SECTIONS : section_count;
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < count; i++) {
        if (scope[i].kind == FILL_TARGET_LANE_SPAN) {
            out->spans[plan_lane_span_lane(plan, (LaneSpanId)scope[i].id)]++;
            continue;
        }
        for (size_t s = 0; s < section_count && out->sections < CLI_MAX_SECTIONS; s++) {
            if (sections[s].node == (NodeId)scope[i].id) {
                snprintf(out->letters[out->sections++], PLAN_SECTION_LETTER_MAX, "%s", sections[s].letter);
            }
        }
    }
}

static void print_reopened_text(const Reopened *reopened) {
    if (reopened->sections > 0) {
        printf("reopened sections");
        for (size_t i = 0; i < reopened->sections; i++) {
            printf(i ? ", %s" : " %s", reopened->letters[i]);
        }
        printf("\n");
    }
    bool any = false;
    for (int lane = PLAN_LANE_HARMONY; lane <= PLAN_LANE_INNER_VOICE; lane++) {
        if (reopened->spans[lane] > 0) {
            printf("%s %s %zu", any ? "," : "reopened lane spans:", LANE_LABELS[lane], reopened->spans[lane]);
            any = true;
        }
    }
    if (any) {
        printf("\n");
    }
}

/* The JSON members "reopened":{...}, into out. */
static void reopened_json(const Reopened *reopened, char *out, size_t size) {
    size_t used = (size_t)snprintf(out, size, "\"reopened\":{\"sections\":[");
    for (size_t i = 0; i < reopened->sections && used < size; i++) {
        used += (size_t)snprintf(out + used, size - used, "%s\"%s\"", i ? "," : "", reopened->letters[i]);
    }
    used += (size_t)snprintf(out + used, size - used, "],\"spans\":{\"harmony\":%zu,\"bass\":%zu,\"melody\":%zu,\"inner_voice\":%zu}}",
                             reopened->spans[PLAN_LANE_HARMONY], reopened->spans[PLAN_LANE_BASS],
                             reopened->spans[PLAN_LANE_MELODY], reopened->spans[PLAN_LANE_INNER_VOICE]);
}

int run_regenerate(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options, true);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_regenerate_help(stdout);
        return 0;
    }
    status = check_options(&options, true);
    if (status != 0) {
        return status;
    }
    char *text = NULL;
    status = cli_load_plan_text(options.input, &text);
    if (status != 0) {
        return status;
    }

    /* What will be reopened, read from the plan as it is now. */
    PiecePlan *before = NULL;
    PlanJsonError error;
    Reopened reopened;
    char what[96];
    if (plan_from_json(text, &before, &error) != PLAN_OK) {
        free(text);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot read plan '%s': %s", options.input, error.message);
    }
    FillTarget *targets = NULL;
    size_t target_count = 0;
    status = cli_find_targets(before, &options.target, &targets, &target_count);
    cli_describe_target(before, &options.target, what, sizeof(what));
    if (status == 0) {
        FillTarget *scope = NULL;
        size_t scope_count = 0;
        RegenerateResult scoped = plan_regenerate_scope_all(before, targets, target_count, &scope, &scope_count);
        if (scoped != REGENERATE_OK) {
            status = cli_report_refused(scoped, what);
        } else {
            read_reopened(before, scope, scope_count, &reopened);
        }
        free(scope);
    }
    free(targets);
    plan_destroy(before);
    if (status != 0) {
        free(text);
        return status;
    }

    /* The seeds are tried one after another, each from a new copy of the plan; only a stuck run can be mended by the next. */
    PiecePlan *plan = NULL;
    ComposeOutcome outcome;
    memset(&outcome, 0, sizeof(outcome));
    bool done = false;
    unsigned long used = options.seed;
    for (unsigned long attempt = 0; attempt < CLI_MAX_SEED_TRIES && !done && status == 0; attempt++) {
        PiecePlan *fresh = NULL;
        if (plan_from_json(text, &fresh, &error) != PLAN_OK) {
            status = cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot read plan '%s': %s", options.input, error.message);
            break;
        }
        FillTarget *again = NULL;
        size_t again_count = 0;
        status = cli_find_targets(fresh, &options.target, &again, &again_count);
        if (status != 0) {
            plan_destroy(fresh);
            break;
        }
        ComposeOutcome result;
        memset(&result, 0, sizeof(result));
        ComposeConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = (options.seed + attempt) & CLI_SEED_MAX;
        RegenerateResult ran = plan_regenerate_all(fresh, again, again_count, &config, &result);
        free(again);
        if (plan) {
            plan_destroy(plan);
            compose_outcome_destroy(&outcome);
        }
        plan = fresh;
        outcome = result;
        used = config.seed;
        if (ran == REGENERATE_OK) {
            done = true;
        } else if (ran != REGENERATE_ERR_STUCK) {
            status = cli_report_refused(ran, what);
        }
    }
    free(text);
    if (status == 0 && !done) {
        cli_report_stuck(plan, &outcome, options.seed, used);
        status = CLI_EXIT_UNSATISFIABLE;
    }
    if (status == 0) {
        CliOutputPaths paths = {options.out_path, options.midi_path, options.musicxml_path, NULL, NULL,
                                &options.export_options};
        const char *files[CLI_MAX_OUTPUTS];
        size_t file_count = 0;
        status = cli_write_outputs(&paths, plan, &outcome, files, &file_count);
        if (status == 0) {
            if (options.json) {
                char extra[512];
                reopened_json(&reopened, extra, sizeof(extra));
                cli_print_composed(true, used, plan, &outcome, extra, files, file_count);
            } else {
                print_reopened_text(&reopened);
                cli_print_composed(false, used, plan, &outcome, NULL, files, file_count);
            }
        }
    }
    if (plan) {
        plan_destroy(plan);
        compose_outcome_destroy(&outcome);
    }
    return status;
}
