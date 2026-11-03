#include "counterpoint.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_options.h"
#include "util/util.h"

#include <ursatz_composer/counterpoint.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/render.h>

#define MAX_SUGGESTIONS 16
#define MAX_FILES (3 * MAX_SUGGESTIONS)
#define MAX_NOTES 512
#define SIGNATURE_MAX 4096

typedef struct {
    bool help;
    const char *input;
    const char *species_text;
    bool above;
    bool below;
    const char *key_text;
    const char *mode_text;
    const char *meter_text;
    const char *seed_text;
    const char *suggest_text;
    const char *out_path;
    const char *midi_path;
    const char *musicxml_path;
    const char *tempo_text;
    bool plain_midi;
    PlanExportOptions export_options;
    const char *format_text;
    /* Read from the text above. */
    int species;
    PlanKey key;
    PlanMeter meter;
    unsigned long seed;
    unsigned long suggest;
    bool json;
} Options;

/* One counter-melody written: its plan, the target and the verdict of the library. */
typedef struct {
    PiecePlan *plan;
    LaneSpanId target;
    unsigned long seed;
    CounterpointReport report;
    char signature[SIGNATURE_MAX];
} Result;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", COUNTERPOINT_USAGE_LINE + 7);
    fprintf(out, "Writes a counter-melody against <line> (a MIDI file, a MusicXML file, a note list or a motive JSON file)\n");
    fprintf(out, "in the first, second, third or fourth species of strict counterpoint, and checks it with Ursatz's\n");
    fprintf(out, "standard-counterpoint rules.\n\n");
    fprintf(out, "Required:\n");
    fprintf(out, "  --species <1-4>      1 note against 1, 2 against 1, 4 against 1, or syncopated half notes tied over the\n");
    fprintf(out, "                       barline. Species 2 to 4 need a cantus firmus: one whole note a bar, three bars or more,\n");
    fprintf(out, "                       and species 3 a meter of four beats\n");
    fprintf(out, "  --key <key>          tonic and mode in one argument (\"D minor\"), or a tonic with --mode\n");
    fprintf(out, "  --meter <n/m>        the meter of the line, for example 4/4\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --below              the line is the melody and the counter-melody sings below it (default)\n");
    fprintf(out, "  --above              the line is the bass and the counter-melody sings above it\n");
    fprintf(out, "  --mode <major|minor> the mode, when --key gives only a tonic\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same seed gives the same counter-melody. Default: derived\n");
    fprintf(out, "                       from the clock; the seed used is always printed\n");
    fprintf(out, "  --suggest <n>        write n different counter-melodies (1 to %d); with files, number k goes to\n", MAX_SUGGESTIONS);
    fprintf(out, "                       <name>-k.<extension>\n");
    fprintf(out, "  --out <file.json>    write the plan as JSON\n");
    fprintf(out, "  --midi <file>        write the two parts as a MIDI file\n");
    fprintf(out, "  --musicxml <file>    write the two parts as MusicXML\n");
    fprintf(out, "  --tempo <bpm>        MIDI and MusicXML: write a tempo, quarter notes per minute from 20 to 400 (a whole number or\n");
    fprintf(out, "                       a fraction like 195/2). Default: none, the file default of 120 applies\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --format <text|json> how to print the result on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the request cannot be satisfied (the line cannot be\n");
    fprintf(out, "read or is not one the species can be written against, or no counter-melody holds the rules); 3 an output\n");
    fprintf(out, "file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer counterpoint cantus.txt --species 2 --above --key \"D minor\" --meter 4/4 --seed 3 --midi out.mid\n");
}

/* --- reading the options -------------------------------------------------- */

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--above") == 0) {
            options->above = true;
        } else if (strcmp(arg, "--below") == 0) {
            options->below = true;
        } else if (strcmp(arg, "--species") == 0) {
            target = &options->species_text;
        } else if (strcmp(arg, "--key") == 0) {
            target = &options->key_text;
        } else if (strcmp(arg, "--mode") == 0) {
            target = &options->mode_text;
        } else if (strcmp(arg, "--meter") == 0) {
            target = &options->meter_text;
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
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format_text;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer counterpoint --help)", arg);
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

static int check_options(Options *options) {
    unsigned long number;
    if (!options->input) {
        return cli_fail(CLI_EXIT_USAGE, "counterpoint needs a line\n%s", COUNTERPOINT_USAGE_LINE);
    }
    if (!options->species_text || !options->key_text || !options->meter_text) {
        return cli_fail(CLI_EXIT_USAGE, "counterpoint needs --species, --key and --meter (see --help)");
    }
    if (!cli_parse_whole_number(options->species_text, 4, &number) || number < 1) {
        return cli_fail(CLI_EXIT_USAGE, "--species must be 1, 2, 3 or 4, not '%s' (fifth species is not written)",
                        options->species_text);
    }
    options->species = (int)number;
    if (options->above && options->below) {
        return cli_fail(CLI_EXIT_USAGE, "--above and --below cannot both be given");
    }
    if (options->mode_text) {
        char text[64];
        if (!cli_equals_ignore_case(options->mode_text, "major") && !cli_equals_ignore_case(options->mode_text, "minor")) {
            return cli_fail(CLI_EXIT_USAGE, "--mode must be major or minor, not '%s'", options->mode_text);
        }
        if (strchr(options->key_text, ' ')) {
            return cli_fail(CLI_EXIT_USAGE, "--key already names a mode; give a tonic alone with --mode");
        }
        snprintf(text, sizeof(text), "%s %s", options->key_text, options->mode_text);
        if (!cli_parse_key(text, &options->key)) {
            return cli_fail(CLI_EXIT_USAGE, "unknown key '%s'", options->key_text);
        }
    } else if (!cli_parse_key(options->key_text, &options->key)) {
        return cli_fail(CLI_EXIT_USAGE,
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, for "
                        "example \"D minor\"",
                        options->key_text);
    }
    if (!cli_parse_meter(options->meter_text, &options->meter)) {
        return cli_fail(CLI_EXIT_USAGE, "meter '%s' is not a meter; write beats over a beat unit, for example 4/4",
                        options->meter_text);
    }
    options->seed = cli_default_seed();
    if (options->seed_text && !cli_parse_whole_number(options->seed_text, CLI_SEED_MAX, &options->seed)) {
        return cli_fail(CLI_EXIT_USAGE, "--seed must be a whole number from 0 to %lu, not '%s'", CLI_SEED_MAX,
                        options->seed_text);
    }
    options->suggest = 1;
    if (options->suggest_text &&
        (!cli_parse_whole_number(options->suggest_text, MAX_SUGGESTIONS, &options->suggest) || options->suggest == 0)) {
        return cli_fail(CLI_EXIT_USAGE, "--suggest must be a whole number from 1 to %d, not '%s'", MAX_SUGGESTIONS,
                        options->suggest_text);
    }
    {
        int export_status = cli_read_export_options(options->tempo_text, options->plain_midi, &options->export_options);
        if (export_status != 0) {
            return export_status;
        }
    }
    if (options->format_text && strcmp(options->format_text, "text") != 0 && strcmp(options->format_text, "json") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", options->format_text);
    }
    options->json = options->format_text && strcmp(options->format_text, "json") == 0;
    return 0;
}

/* --- the plan -------------------------------------------------------------------- */

static PlanTime fraction(long num, unsigned long den) {
    PlanTime time = {0, 1};
    plan_time_make(num, den, &time);
    return time;
}

/*
 * The plan the operator works on: a form over the length of the line, the meter and the key given, the line as the melody
 * (below) or the bass (above), and an open inner voice span over the same time with the counterpoint constraint.
 */
static PiecePlan *build_plan(const Options *options, const Motive *motive, LaneSpanId *target) {
    PiecePlan *plan = plan_create();
    PlanSpan all = {fraction(0, 1), motive->length};
    PlanLanePayload payload;
    PlanConstraint constraint;
    PlanNote placeholder;
    NodeId owner;
    LaneSpanId id;
    bool ok = plan != NULL;
    ok = ok && plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, all, "counterpoint", &owner) == PLAN_OK;
    memset(&payload, 0, sizeof(payload));
    payload.meter = options->meter;
    ok = ok && plan_add_lane_span(plan, owner, PLAN_LANE_METER, 0, all, &payload, &id) == PLAN_OK &&
         plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN) == PLAN_OK;
    memset(&payload, 0, sizeof(payload));
    payload.key = options->key;
    ok = ok && plan_add_lane_span(plan, owner, PLAN_LANE_KEY, 0, all, &payload, &id) == PLAN_OK &&
         plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN) == PLAN_OK;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = motive->notes;
    payload.notes.note_count = motive->note_count;
    ok = ok && plan_add_lane_span(plan, owner, options->above ? PLAN_LANE_BASS : PLAN_LANE_MELODY, 0, all, &payload, &id) ==
                   PLAN_OK &&
         plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN) == PLAN_OK;
    ok = ok && plan_node_set_state(plan, owner, PLAN_STATE_GIVEN) == PLAN_OK;
    placeholder.midi_pitch = 60;
    placeholder.offset = fraction(0, 1);
    placeholder.duration = motive->length;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = &placeholder;
    payload.notes.note_count = 1;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_COUNTERPOINT;
    constraint.params.counterpoint.species = options->species;
    constraint.params.counterpoint.above = options->above;
    ok = ok && plan_add_lane_span(plan, owner, PLAN_LANE_INNER_VOICE, 1, all, &payload, target) == PLAN_OK &&
         plan_lane_span_add_constraint(plan, *target, &constraint) == PLAN_OK;
    if (!ok) {
        plan_destroy(plan);
        return NULL;
    }
    return plan;
}

static void result_destroy(Result *result) {
    plan_destroy(result->plan);
    result->plan = NULL;
}

/* The notes of the counter-melody as text, to tell two results apart. */
static void sign(const PiecePlan *plan, LaneSpanId target, char *out, size_t size) {
    const PlanNotes *notes = &plan_lane_span_payload(plan, target)->notes;
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < notes->note_count && used + 24 < size; i++) {
        used += (size_t)snprintf(out + used, size - used, "%d@%ld/%lu,", notes->notes[i].midi_pitch,
                                 notes->notes[i].offset.num, notes->notes[i].offset.den);
    }
}

/* One run from one seed; true when the counter-melody was written. */
static bool write_one(const Options *options, const Motive *motive, unsigned long seed, Result *result) {
    FillOperatorRegistry *registry = fill_registry_create();
    CounterpointParams params = counterpoint_params_default();
    FillConfig config;
    FillOutcome outcome;
    bool ok = false;
    memset(result, 0, sizeof(*result));
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = counterpoint_checker;
    result->seed = seed;
    result->plan = build_plan(options, motive, &result->target);
    if (registry && result->plan && counterpoint_register(registry, &params) == FILL_OK &&
        fill_run(result->plan, registry, &config, &outcome, NULL) == FILL_OK) {
        ok = outcome.status == FILL_STATUS_COMPLETE && counterpoint_report(result->plan, result->target, &result->report);
        fill_outcome_destroy(&outcome);
    }
    fill_registry_destroy(registry);
    if (ok) {
        sign(result->plan, result->target, result->signature, sizeof(result->signature));
    } else {
        result_destroy(result);
    }
    return ok;
}

/* --- printing ----------------------------------------------------------------------- */

static void pitch_name(int pitch, char *out, size_t size) {
    util_pitch_name(pitch, false, out, size);
}

/* A length as w, h, q, e, s or t with up to two dots, else as a fraction of a whole note. */
static void duration_name(PlanTime length, char *out, size_t size) {
    static const struct {
        long ticks;
        const char *name;
    } NAMES[] = {{128, "w"}, {192, "w."}, {64, "h"}, {96, "h."}, {112, "h.."}, {32, "q"}, {48, "q."}, {56, "q.."},
                 {16, "e"},  {24, "e."},  {28, "e.."}, {8, "s"}, {12, "s."}, {4, "t"}};
    long ticks = (length.num * 128) % (long)length.den == 0 ? length.num * 128 / (long)length.den : -1;
    for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
        if (NAMES[i].ticks == ticks) {
            snprintf(out, size, "%s", NAMES[i].name);
            return;
        }
    }
    snprintf(out, size, "%ld/%lu", length.num, length.den);
}

/* The notes of a lane voice as a note list, gaps as rests, a bar line between bars. */
static void print_line(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan span, PlanTime bar) {
    LaneSpanId ids[MAX_NOTES];
    size_t found = plan_lane_find(plan, lane, voice, span, ids, MAX_NOTES);
    PlanTime at = fraction(0, 1);
    for (size_t i = 0; i < found; i++) {
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        PlanTime base = plan_lane_span_span(plan, ids[i]).start;
        for (size_t n = 0; n < notes->note_count; n++) {
            PlanTime start;
            PlanTime gap;
            char pitch[16];
            char length[24];
            plan_time_add(base, notes->notes[n].offset, &start);
            if (plan_time_compare(start, at) > 0) {
                PlanTime negative = {-at.num, at.den};
                plan_time_add(start, negative, &gap);
                duration_name(gap, length, sizeof(length));
                printf(" r:%s", length);
            }
            if (bar.num > 0 && start.num * (long)bar.den % (bar.num * (long)start.den) == 0 && start.num > 0) {
                printf(" |");
            }
            pitch_name(notes->notes[n].midi_pitch, pitch, sizeof(pitch));
            duration_name(notes->notes[n].duration, length, sizeof(length));
            printf(" %s:%s", pitch, length);
            plan_time_add(start, notes->notes[n].duration, &at);
        }
    }
    printf("\n");
}

static void print_json_notes(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan span) {
    LaneSpanId ids[MAX_NOTES];
    size_t found = plan_lane_find(plan, lane, voice, span, ids, MAX_NOTES);
    bool first = true;
    printf("[");
    for (size_t i = 0; i < found; i++) {
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        PlanTime base = plan_lane_span_span(plan, ids[i]).start;
        for (size_t n = 0; n < notes->note_count; n++) {
            PlanTime start;
            plan_time_add(base, notes->notes[n].offset, &start);
            printf("%s{\"midi_pitch\":%d,\"offset\":{\"num\":%ld,\"den\":%lu},\"duration\":{\"num\":%ld,\"den\":%lu}}",
                   first ? "" : ",", notes->notes[n].midi_pitch, start.num, start.den, notes->notes[n].duration.num,
                   notes->notes[n].duration.den);
            first = false;
        }
    }
    printf("]");
}

static void print_json_string(const char *text) {
    putchar('"');
    for (; *text; text++) {
        if (*text == '"' || *text == '\\') {
            putchar('\\');
        }
        putchar(*text);
    }
    putchar('"');
}

static void print_result_text(const Options *options, const Result *result, PlanSpan span, PlanTime bar, size_t k,
                              size_t count) {
    if (options->suggest > 1 || options->suggest_text) {
        printf("\ncounter-melody %zu of %zu, seed %lu\n", k + 1, count, result->seed);
    } else {
        printf("\nseed %lu\n", result->seed);
    }
    printf("line:          ");
    print_line(result->plan, options->above ? PLAN_LANE_BASS : PLAN_LANE_MELODY, 0, span, bar);
    printf("counter-melody:");
    print_line(result->plan, PLAN_LANE_INNER_VOICE, 1, span, bar);
    printf("rules (standard-counterpoint, species %d):\n", result->report.species);
    for (size_t g = 0; g < result->report.count; g++) {
        printf("  %-18s %s%s\n", result->report.name[g], result->report.hard_ok[g] ? "held" : "BROKEN",
               result->report.hard_ok[g] && !result->report.soft_ok[g] ? " (some advice not followed)" : "");
    }
}

static void print_result(const Options *options, const Result *results, size_t count, PlanSpan span, PlanTime bar,
                         const char *const *files, size_t file_count) {
    char key[24];
    cli_key_name(&options->key, key, sizeof(key));
    if (options->json) {
        printf("{\"species\":%d,\"position\":\"%s\",\"key\":\"%s\",\"meter\":\"%d/%d\",\"results\":[", options->species,
               options->above ? "above" : "below", key, options->meter.beats_per_bar, options->meter.beat_unit);
        for (size_t k = 0; k < count; k++) {
            const Result *result = &results[k];
            printf("%s{\"seed\":%lu,\"line\":", k ? "," : "", result->seed);
            print_json_notes(result->plan, options->above ? PLAN_LANE_BASS : PLAN_LANE_MELODY, 0, span);
            printf(",\"counter_melody\":");
            print_json_notes(result->plan, PLAN_LANE_INNER_VOICE, 1, span);
            printf(",\"groups\":[");
            for (size_t g = 0; g < result->report.count; g++) {
                printf("%s{\"name\":\"%s\",\"hard_held\":%s,\"soft_held\":%s}", g ? "," : "", result->report.name[g],
                       result->report.hard_ok[g] ? "true" : "false", result->report.soft_ok[g] ? "true" : "false");
            }
            printf("]}");
        }
        printf("],\"files\":[");
        for (size_t i = 0; i < file_count; i++) {
            printf("%s", i ? "," : "");
            print_json_string(files[i]);
        }
        printf("]}\n");
        return;
    }
    printf("species %d counterpoint %s the line, key %s, meter %d/%d\n", options->species,
           options->above ? "above" : "below", key, options->meter.beats_per_bar, options->meter.beat_unit);
    for (size_t k = 0; k < count; k++) {
        print_result_text(options, &results[k], span, bar, k, count);
    }
    for (size_t i = 0; i < file_count; i++) {
        printf("wrote %s\n", files[i]);
    }
}

/* --- writing the files ---------------------------------------------------------- */

/* Every output goes to a temporary file first and is renamed only when every one was written. */
static int write_files(const Options *options, const Result *results, size_t count, CliPendingFile *pending,
                       size_t *pending_count) {
    const char *paths[3] = {options->out_path, options->midi_path, options->musicxml_path};
    int status = 0;
    *pending_count = 0;
    for (size_t k = 0; k < count && status == 0; k++) {
        RenderedPlan rendered;
        bool have_rendering = false;
        for (int kind = 0; kind < 3 && status == 0; kind++) {
            CliPendingFile *entry = &pending[*pending_count];
            if (!paths[kind]) {
                continue;
            }
            if (!cli_alternative_path(paths[kind], k + 1, count > 1, entry->final_path, sizeof(entry->final_path)) ||
                snprintf(entry->temporary, sizeof(entry->temporary), "%s.tmp", entry->final_path) >=
                    (int)sizeof(entry->temporary)) {
                status = cli_fail(CLI_EXIT_WRITE, "file name '%s' is too long", paths[kind]);
                break;
            }
            (*pending_count)++;
            if (kind == 0) {
                PlanJsonError error;
                if (plan_write_json_file(results[k].plan, entry->temporary, &error) != PLAN_OK) {
                    status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", entry->final_path, error.message);
                }
                continue;
            }
            PlanExportError error;
            if (!have_rendering) {
                if (plan_render(results[k].plan, &rendered) != PLAN_RENDER_OK) {
                    status = cli_fail(CLI_EXIT_WRITE, "cannot render the plan");
                    break;
                }
                have_rendering = true;
            }
            PlanExportResult written = kind == 1 ? rendered_plan_export_midi(&rendered, &options->export_options, entry->temporary, &error)
                                                 : rendered_plan_export_musicxml(&rendered, &options->export_options, entry->temporary, &error);
            if (written != PLAN_EXPORT_OK) {
                status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", entry->final_path, error.message);
            }
        }
        if (have_rendering) {
            rendered_plan_destroy(&rendered);
        }
    }
    if (status == 0) {
        for (size_t i = 0; i < *pending_count; i++) {
            if (rename(pending[i].temporary, pending[i].final_path) != 0) {
                status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", pending[i].final_path);
                break;
            }
        }
    }
    if (status != 0) {
        cli_discard_pending(pending, *pending_count);
    }
    return status;
}

/* --- the command ---------------------------------------------------------------- */

int run_counterpoint(int argc, char **argv) {
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

    MotiveOptions motive_options;
    Motive motive;
    MotiveError motive_error;
    memset(&motive_options, 0, sizeof(motive_options));
    motive_options.has_meter = true;
    motive_options.meter = options.meter;
    motive_options.has_key = true;
    motive_options.key = options.key;
    /* A line of any length is read here: the one-bar limit belongs to a motive, not to a line to write against. */
    motive_options.max_bars = 0;
    MotiveResult loaded = motive_load_file(options.input, MOTIVE_FORMAT_AUTO, &motive_options, &motive, &motive_error);
    if (loaded != MOTIVE_OK) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot load '%s' (%s)%s%s: %s", options.input,
                        motive_result_name(loaded), motive_error.location[0] ? " at " : "", motive_error.location,
                        motive_error.message);
    }

    /* Whether the line is one the species can be written against, before any seed is tried. */
    {
        LaneSpanId target;
        PiecePlan *plan = build_plan(&options, &motive, &target);
        const char *why = plan ? counterpoint_why_not(plan, target) : "the plan could not be built";
        if (why) {
            status = cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot be written as asked: %s", why);
        }
        plan_destroy(plan);
        if (status != 0) {
            return status;
        }
    }

    Result results[MAX_SUGGESTIONS];
    size_t made = 0;
    unsigned long tries = (unsigned long)CLI_MAX_SEED_TRIES * options.suggest;
    unsigned long last_seed = options.seed;
    memset(results, 0, sizeof(results));
    for (unsigned long attempt = 0; attempt < tries && made < options.suggest; attempt++) {
        Result result;
        last_seed = (options.seed + attempt) & CLI_SEED_MAX;
        if (write_one(&options, &motive, last_seed, &result)) {
            bool repeated = false;
            for (size_t k = 0; k < made; k++) {
                repeated = repeated || strcmp(results[k].signature, result.signature) == 0;
            }
            if (repeated) {
                result_destroy(&result);
            } else {
                results[made++] = result;
            }
        }
    }
    if (made == 0) {
        status = cli_fail(CLI_EXIT_UNSATISFIABLE,
                          "cannot be written as asked: no species %d counter-melody %s the line holds the rules in %s, "
                          "from seeds %lu to %lu",
                          options.species, options.above ? "above" : "below", options.key_text, options.seed, last_seed);
    } else if (made < options.suggest && options.suggest_text) {
        fprintf(stderr, "Note: only %zu different counter-melod%s found for this request\n", made, made == 1 ? "y" : "ies");
    }
    if (status == 0) {
        CliPendingFile pending[MAX_FILES];
        const char *files[MAX_FILES];
        size_t pending_count = 0;
        PlanSpan span = {fraction(0, 1), motive.length};
        PlanTime bar = fraction(options.meter.beats_per_bar, (unsigned long)options.meter.beat_unit);
        status = write_files(&options, results, made, pending, &pending_count);
        if (status == 0) {
            for (size_t i = 0; i < pending_count; i++) {
                files[i] = pending[i].final_path;
            }
            print_result(&options, results, made, span, bar, files, pending_count);
        }
    }
    for (size_t k = 0; k < made; k++) {
        result_destroy(&results[k]);
    }
    return status;
}
