#include "progression.h"

#include "cli_options.h"
#include "util/util.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/fill_suggest.h>
#include <ursatz_composer/harmony_bass.h>
#include <ursatz_composer/harmony_embellish.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/harmony_schema.h>
#include <ursatz_composer/harmony_sequence.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/render.h>

#define EXIT_USAGE CLI_EXIT_USAGE
#define EXIT_UNSATISFIABLE CLI_EXIT_UNSATISFIABLE
#define EXIT_WRITE CLI_EXIT_WRITE

#define MAX_BARS 64
#define MAX_SEED_TRIES CLI_MAX_SEED_TRIES
#define MAX_CHORDS 64
#define MAX_SUGGESTIONS 8
#define MAX_FILES (3 * MAX_SUGGESTIONS)

typedef enum { EMBELLISH_NONE, EMBELLISH_LOW, EMBELLISH_MEDIUM, EMBELLISH_HIGH } EmbellishLevel;

typedef struct {
    bool help;
    const char *key_text;
    const char *meter_text;
    const char *bars_text;
    const char *cadence_text;
    const char *seed_text;
    const char *embellish_text;
    const char *format_text;
    const char *out_path;
    const char *midi_path;
    const char *musicxml_path;
    const char *tempo_text;
    bool plain_midi;
    PlanExportOptions export_options;
    const char *schema_text;
    const char *sequence_text;
    const char *suggest_text;
    bool list_schemas;
    bool bass;
    /* Read from the text above. */
    AnalysisKey key;
    PlanMeter meter;
    long bars;
    CadenceType cadence;
    unsigned long seed;
    EmbellishLevel embellish;
    bool json;
    GalantSchemaId schema;
    bool has_schema;
    ScalePatternId sequence;
    bool has_sequence;
    unsigned long suggest; /* 0 for none */
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage: ursatz-composer progression --key <key> --meter <n/m> --bars <n> --cadence <type> [options]\n");
    fprintf(out, "\n");
    fprintf(out, "Writes a chord progression for one phrase, with a bass line, ending in the cadence asked for.\n");
    fprintf(out, "\n");
    fprintf(out, "Required:\n");
    fprintf(out, "  --key <key>          tonic and mode: a letter A-G, an optional # or b, then major or minor,\n");
    fprintf(out, "                       in one argument (\"D minor\", \"F# major\", \"Bb minor\"); any case\n");
    fprintf(out, "  --meter <n/m>        2/4, 3/4, 4/4 or 6/8\n");
    fprintf(out, "  --bars <n>           length of the phrase, 1 to %d; a cadence needs at least 2 bars in 4/4 and\n", MAX_BARS);
    fprintf(out, "                       6/8, 3 in 2/4 and 3/4 (more for a plagal cadence)\n");
    fprintf(out, "  --cadence <type>     PAC, IAC, HC, DC or PC (perfect or imperfect authentic, half, deceptive,\n");
    fprintf(out, "                       plagal); any case\n");
    fprintf(out, "\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same seed gives the same output. Default: derived\n");
    fprintf(out, "                       from the clock. The seed used is always printed (if no bass line suits the\n");
    fprintf(out, "                       progression, the next seeds are tried and the one that worked is printed)\n");
    fprintf(out, "  --embellish <level>  none (default), low, medium or high: how often chords are prolonged with\n");
    fprintf(out, "                       neighbor, passing and six-four chords, given strong predominants and\n");
    fprintf(out, "                       sevenths, and led into by applied chords\n");
    fprintf(out, "  --bass, --no-bass    write a bass line (default) or only the chords\n");
    fprintf(out, "  --schema <name>      build the start of the phrase on a galant schema (see --list-schemas)\n");
    fprintf(out, "  --sequence <name>    build a continuation on a harmonic sequence (see --list-schemas)\n");
    fprintf(out, "  --list-schemas       list the schema and sequence names and exit\n");
    fprintf(out, "  --suggest <n>        write n alternatives (1 to %d) instead of one; with files, alternative k goes\n", MAX_SUGGESTIONS);
    fprintf(out, "                       to <name>-k.<extension>\n");
    fprintf(out, "  --out <file.json>    write the plan as JSON\n");
    fprintf(out, "  --midi <file>        write the bass line as a MIDI file (needs the bass)\n");
    fprintf(out, "  --musicxml <file>    write the bass line as MusicXML (needs the bass)\n");
    fprintf(out, "  --tempo <bpm>        MIDI and MusicXML: write a tempo, quarter notes per minute from 20 to 400 (a whole number or\n");
    fprintf(out, "                       a fraction like 195/2). Default: none, the file default of 120 applies\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --format <text|json> how to print the progression on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n");
    fprintf(out, "\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the request cannot be satisfied;\n");
    fprintf(out, "3 an output file could not be written.\n");
    fprintf(out, "\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer progression --key \"D minor\" --meter 4/4 --bars 8 --cadence PAC --seed 7 \\\n");
    fprintf(out, "      --embellish medium --out phrase.json --midi phrase.mid\n");
    fprintf(out, "  ursatz-composer progression --key \"G major\" --meter 4/4 --bars 8 --cadence PAC --schema romanesca\n");
    fprintf(out, "  ursatz-composer progression --key \"A minor\" --meter 3/4 --bars 6 --cadence HC --suggest 3\n");
}

/* --- reading the options -------------------------------------------------- */

/*
 * A sequence continues something, and so do the schemas that are not opening gambits: for those the first bar
 * is a tonic chord, written before the operators run, and the skeleton follows it.
 */
static bool needs_lead(const Options *options) {
    return options->has_sequence ||
           (options->has_schema &&
            galant_prototype(options->schema)->function != SCHEMA_FUNCTION_OPENING_GAMBIT);
}

/* Splits argv into options and checks each value; returns 0, or the exit status. */
static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    options->bass = true;
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--bass") == 0) {
            options->bass = true;
        } else if (strcmp(arg, "--no-bass") == 0) {
            options->bass = false;
        } else if (strcmp(arg, "--key") == 0) {
            target = &options->key_text;
        } else if (strcmp(arg, "--meter") == 0) {
            target = &options->meter_text;
        } else if (strcmp(arg, "--bars") == 0) {
            target = &options->bars_text;
        } else if (strcmp(arg, "--cadence") == 0) {
            target = &options->cadence_text;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed_text;
        } else if (strcmp(arg, "--embellish") == 0) {
            target = &options->embellish_text;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format_text;
        } else if (strcmp(arg, "--list-schemas") == 0) {
            options->list_schemas = true;
        } else if (strcmp(arg, "--schema") == 0) {
            target = &options->schema_text;
        } else if (strcmp(arg, "--sequence") == 0) {
            target = &options->sequence_text;
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
        } else {
            return cli_fail(EXIT_USAGE, "unexpected argument '%s' (see ursatz-composer progression --help)", arg);
        }
        if (target) {
            if (i + 1 >= argc) {
                return cli_fail(EXIT_USAGE, "%s needs a value", arg);
            }
            if (*target) {
                return cli_fail(EXIT_USAGE, "%s given twice", arg);
            }
            *target = argv[++i];
        }
    }
    if (options->help || options->list_schemas) {
        return 0;
    }
    if (!options->key_text || !options->meter_text || !options->bars_text || !options->cadence_text) {
        return cli_fail(EXIT_USAGE, "progression needs --key, --meter, --bars and --cadence (see --help)");
    }
    if (!cli_parse_key(options->key_text, &options->key)) {
        return cli_fail(EXIT_USAGE,
                    "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                    "for example \"D minor\"",
                    options->key_text);
    }
    if (!cli_parse_meter(options->meter_text, &options->meter) || !harmony_meter_supported(&options->meter)) {
        return cli_fail(EXIT_USAGE, "meter '%s' is not supported; use 2/4, 3/4, 4/4 or 6/8", options->meter_text);
    }
    unsigned long bars;
    if (!cli_parse_whole_number(options->bars_text, MAX_BARS, &bars) || bars == 0) {
        return cli_fail(EXIT_USAGE, "--bars must be a whole number from 1 to %d, not '%s'", MAX_BARS, options->bars_text);
    }
    options->bars = (long)bars;
    if (!cli_parse_cadence(options->cadence_text, &options->cadence)) {
        return cli_fail(EXIT_USAGE, "unknown cadence '%s'; use PAC, IAC, HC, DC or PC", options->cadence_text);
    }
    if (options->seed_text) {
        if (!cli_parse_whole_number(options->seed_text, 4294967295UL, &options->seed)) {
            return cli_fail(EXIT_USAGE, "--seed must be a whole number from 0 to 4294967295, not '%s'", options->seed_text);
        }
    } else {
        options->seed = cli_default_seed();
    }
    options->embellish = EMBELLISH_NONE;
    if (options->embellish_text) {
        static const char *const LEVELS[4] = {"none", "low", "medium", "high"};
        bool found = false;
        for (int i = 0; i < 4; i++) {
            if (cli_equals_ignore_case(options->embellish_text, LEVELS[i])) {
                options->embellish = (EmbellishLevel)i;
                found = true;
            }
        }
        if (!found) {
            return cli_fail(EXIT_USAGE, "--embellish must be none, low, medium or high, not '%s'", options->embellish_text);
        }
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
            return cli_fail(EXIT_USAGE, "--format must be text or json, not '%s'", options->format_text);
        }
    }
    if (options->schema_text) {
        if (!harmony_schema_find(options->schema_text, &options->schema)) {
            return cli_fail(EXIT_USAGE, "unknown schema '%s' (see --list-schemas)", options->schema_text);
        }
        options->has_schema = true;
        if (!harmony_schema_available(options->schema, options->key.mode)) {
            return cli_fail(EXIT_UNSATISFIABLE, "the schema '%s' cannot be written in a %s key", options->schema_text,
                        options->key.mode == KEY_STRUCTURE_MODE_MINOR ? "minor" : "major");
        }
    }
    if (options->sequence_text) {
        if (!harmony_sequence_find(options->sequence_text, &options->sequence)) {
            return cli_fail(EXIT_USAGE, "unknown sequence '%s' (see --list-schemas)", options->sequence_text);
        }
        options->has_sequence = true;
    }
    if (options->suggest_text &&
        (!cli_parse_whole_number(options->suggest_text, MAX_SUGGESTIONS, &options->suggest) || options->suggest == 0)) {
        return cli_fail(EXIT_USAGE, "--suggest must be a whole number from 1 to %d, not '%s'", MAX_SUGGESTIONS,
                    options->suggest_text);
    }
    if (!options->bass && (options->has_schema || options->has_sequence)) {
        return cli_fail(EXIT_USAGE, "--schema and --sequence write their own bass, so they cannot be used with --no-bass");
    }
    if (!options->bass && (options->midi_path || options->musicxml_path)) {
        return cli_fail(EXIT_USAGE, "--midi and --musicxml write the bass line (harmony makes no sound yet), "
                                "so they cannot be used with --no-bass");
    }
    size_t minimum = harmony_phrase_min_bars(options->cadence, &options->meter) + (needs_lead(options) ? 1 : 0);
    if ((size_t)options->bars < minimum) {
        return cli_fail(EXIT_UNSATISFIABLE, "%ld bar%s is too short for a %s in %s; at least %zu bars are needed",
                    options->bars, options->bars == 1 ? "" : "s", cli_cadence_name(options->cadence), options->meter_text,
                    minimum);
    }
    return 0;
}

/* --- building the plan ------------------------------------------------------ */

typedef struct {
    PiecePlan *plan;
    PlanSpan span;
    Rational bar_length;
} Built;

static Rational time_of(long num, unsigned long den) {
    Rational time;
    rational_make(num, den, &time);
    return time;
}

/*
 * A form, a section and a phrase of the length asked for, with the meter and the key Given and the harmony
 * (and the bass, if wanted) left open for the operators. The nodes and the lanes the user has fixed are Given.
 */
static bool build_plan(const Options *options, Built *built) {
    memset(built, 0, sizeof(*built));
    built->plan = plan_create();
    if (!built->plan) {
        return false;
    }
    built->bar_length = time_of(options->meter.beats_per_bar, (unsigned long)options->meter.beat_unit);
    built->span.start = time_of(0, 1);
    built->span.length = time_of(options->bars * (long)built->bar_length.num, built->bar_length.den);

    NodeId form;
    NodeId section;
    NodeId phrase;
    PlanLanePayload payload;
    LaneSpanId id;
    bool ok = plan_add_node(built->plan, PLAN_NODE_NONE, PLAN_NODE_FORM, built->span, "form", &form) == PLAN_OK &&
              plan_add_node(built->plan, form, PLAN_NODE_SECTION, built->span, "section", &section) == PLAN_OK &&
              plan_add_node(built->plan, section, PLAN_NODE_PHRASE, built->span, "phrase", &phrase) == PLAN_OK;
    PlanConstraint length = {PLAN_CONSTRAINT_LENGTH_BARS, {0}};
    PlanConstraint cadence = {PLAN_CONSTRAINT_CADENCE, {0}};
    length.params.bars = (int)options->bars;
    cadence.params.cadence = options->cadence;
    ok = ok && plan_node_add_constraint(built->plan, phrase, &length) == PLAN_OK &&
         plan_node_add_constraint(built->plan, phrase, &cadence) == PLAN_OK;

    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = options->meter.beats_per_bar;
    payload.meter.beat_unit = options->meter.beat_unit;
    ok = ok && plan_add_lane_span(built->plan, phrase, PLAN_LANE_METER, 0, built->span, &payload, &id) == PLAN_OK &&
         plan_lane_span_set_state(built->plan, id, PLAN_STATE_GIVEN) == PLAN_OK;
    memset(&payload, 0, sizeof(payload));
    payload.key = options->key;
    ok = ok && plan_add_lane_span(built->plan, phrase, PLAN_LANE_KEY, 0, built->span, &payload, &id) == PLAN_OK &&
         plan_lane_span_set_state(built->plan, id, PLAN_STATE_GIVEN) == PLAN_OK;

    /* The part of the phrase the operators fill: all of it, or what follows a tonic first bar. */
    PlanSpan region = built->span;
    const char *tonic = options->key.mode == KEY_STRUCTURE_MODE_MINOR ? "i" : "I";
    if (needs_lead(options)) {
        PlanSpan lead = {built->span.start, built->bar_length};
        region.start = built->bar_length;
        if (rational_sub(built->span.length, built->bar_length, &region.length) != RATIONAL_OK) {
            return false;
        }
        memset(&payload, 0, sizeof(payload));
        strcpy(payload.harmony.roman_numeral, tonic);
        payload.harmony.function = HARMONIC_FUNCTION_TONIC;
        payload.harmony.has_function = true;
        ok = ok && plan_add_lane_span(built->plan, phrase, PLAN_LANE_HARMONY, 0, lead, &payload, &id) == PLAN_OK &&
             plan_lane_span_set_state(built->plan, id, PLAN_STATE_REALIZED) == PLAN_OK;
        /* Its bass is the tonic in the octave of the range nearest the middle of it. */
        PlanNote tonic_note = plan_note(48, time_of(0, 1), lead.length);
        for (int pitch = 40; pitch <= 60; pitch++) {
            int distance = pitch > 50 ? pitch - 50 : 50 - pitch;
            int best = tonic_note.midi > 50 ? tonic_note.midi - 50 : 50 - tonic_note.midi;
            if (pitch % 12 == analysis_key_tonic_pitch_class(&options->key) && (tonic_note.midi % 12 != analysis_key_tonic_pitch_class(&options->key) || distance < best)) {
                tonic_note.midi = pitch;
                tonic_note.spelled = false;
            }
        }
        memset(&payload, 0, sizeof(payload));
        payload.notes.notes = &tonic_note;
        payload.notes.note_count = 1;
        ok = ok && plan_add_lane_span(built->plan, phrase, PLAN_LANE_BASS, 0, lead, &payload, &id) == PLAN_OK &&
             plan_lane_span_set_state(built->plan, id, PLAN_STATE_REALIZED) == PLAN_OK;
    }

    /* The open harmony span asks for the cadence; its placeholder chord is replaced by the operators. */
    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, tonic);
    payload.harmony.has_function = false;
    ok = ok && plan_add_lane_span(built->plan, phrase, PLAN_LANE_HARMONY, 0, region, &payload, &id) == PLAN_OK &&
         plan_lane_span_add_constraint(built->plan, id, &cadence) == PLAN_OK;
    if (options->bass) {
        PlanNote note = plan_note(40, time_of(0, 1), region.length);
        memset(&payload, 0, sizeof(payload));
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        ok = ok && plan_add_lane_span(built->plan, phrase, PLAN_LANE_BASS, 0, region, &payload, &id) == PLAN_OK;
    }
    ok = ok && plan_node_set_state(built->plan, form, PLAN_STATE_GIVEN) == PLAN_OK &&
         plan_node_set_state(built->plan, section, PLAN_STATE_GIVEN) == PLAN_OK &&
         plan_node_set_state(built->plan, phrase, PLAN_STATE_GIVEN) == PLAN_OK;
    if (!ok) {
        plan_destroy(built->plan);
        built->plan = NULL;
    }
    return ok;
}

/* --- running the operators ---------------------------------------------------- */

typedef struct {
    HarmonyEmbellishParams embellish;
    HarmonyBassParams bass;
    HarmonySchemaParams schema;
    HarmonySequenceParams sequence;
} Parameters;

static void parameters_for(const Options *options, Parameters *parameters) {
    parameters->embellish = harmony_embellish_params_default();
    switch (options->embellish) {
    case EMBELLISH_LOW:
        parameters->embellish.density = 250;
        parameters->embellish.families = HARMONY_EMBELLISH_PROLONGATION;
        break;
    case EMBELLISH_MEDIUM:
        parameters->embellish.density = 500;
        parameters->embellish.families = HARMONY_EMBELLISH_PROLONGATION | HARMONY_EMBELLISH_PREDOMINANT;
        break;
    case EMBELLISH_HIGH:
        parameters->embellish.density = 750;
        parameters->embellish.families = HARMONY_EMBELLISH_ALL;
        break;
    default:
        parameters->embellish.density = 0;
        break;
    }
    parameters->bass = harmony_bass_params_default();
    /* A schema or sequence that was asked for by name is the only one offered, and is preferred to the plain
       operators' candidates by a whole point. */
    parameters->schema = harmony_schema_params_default();
    parameters->schema.schemas = options->has_schema ? HARMONY_SCHEMA_BIT(options->schema) : 0;
    parameters->schema.preference = 1000;
    parameters->sequence = harmony_sequence_params_default();
    parameters->sequence.patterns = options->has_sequence ? HARMONY_SEQUENCE_BIT(options->sequence) : 0;
    parameters->sequence.preference = 1000;
}

typedef enum { STAGE_HARMONY, STAGE_EMBELLISH, STAGE_BASS } Stage;

static FillOperatorRegistry *registry_for(const Options *options, const Parameters *parameters, Stage stage) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillResult registered = FILL_OK;
    if (!registry) {
        return NULL;
    }
    switch (stage) {
    case STAGE_HARMONY:
        if (options->has_schema) {
            registered = harmony_schema_register(registry, &parameters->schema);
        }
        if (registered == FILL_OK && options->has_sequence) {
            registered = harmony_sequence_register(registry, &parameters->sequence);
        }
        if (registered == FILL_OK) {
            registered = harmony_phrase_register(registry);
        }
        break;
    case STAGE_EMBELLISH:
        registered = harmony_embellish_register(registry, &parameters->embellish);
        break;
    case STAGE_BASS:
        registered = harmony_bass_register(registry, &parameters->bass);
        break;
    }
    if (registered != FILL_OK) {
        fill_registry_destroy(registry);
        return NULL;
    }
    return registry;
}

static FillConfig config_for(unsigned long seed) {
    FillConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = harmony_phrase_checker;
    return config;
}

/* One run of the fill loop with the operators of a stage. False only if the loop itself fails. */
static bool run_stage(PiecePlan *plan, const Options *options, unsigned long seed, Stage stage,
                      const Parameters *parameters, FillRecord **record) {
    FillOperatorRegistry *registry = registry_for(options, parameters, stage);
    FillConfig config = config_for(seed);
    FillOutcome outcome;
    if (!registry) {
        return false;
    }
    bool ok = fill_run(plan, registry, &config, &outcome, record) == FILL_OK;
    if (ok) {
        fill_outcome_destroy(&outcome);
    }
    fill_registry_destroy(registry);
    return ok;
}

/* Whether the loop chose a candidate with this label (or, with a prefix, one that begins with it). */
static bool chose(const FillRecord *record, const char *label_prefix) {
    for (size_t s = 0; record && s < record->step_count; s++) {
        const FillStep *step = &record->steps[s];
        if (step->outcome == FILL_STEP_APPLIED && step->chosen >= 0 &&
            strncmp(step->considered[step->chosen].label, label_prefix, strlen(label_prefix)) == 0) {
            return true;
        }
    }
    return false;
}

static bool any_open(const PiecePlan *plan, PlanLaneKind lane, PlanSpan span) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(plan, lane, 0, span, ids, MAX_CHORDS);
    for (size_t i = 0; i < count && i < MAX_CHORDS; i++) {
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED) {
            return true;
        }
    }
    return count == 0;
}

/* Reopens, for embellishment, the chords that have no bass written under them. */
static bool open_for_embellishment(PiecePlan *plan, PlanSpan span) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, MAX_CHORDS);
    for (size_t i = 0; i < count && i < MAX_CHORDS; i++) {
        PlanSpan chord = plan_lane_span_span(plan, ids[i]);
        LaneSpanId basses[MAX_CHORDS];
        size_t bass_count = plan_lane_find(plan, PLAN_LANE_BASS, 0, chord, basses, MAX_CHORDS);
        bool written = false;
        for (size_t b = 0; b < bass_count && b < MAX_CHORDS; b++) {
            written = written || plan_lane_span_state(plan, basses[b]) != PLAN_STATE_CONSTRAINED;
        }
        if (!written && harmony_embellish_open(plan, chord) != PLAN_OK) {
            return false;
        }
    }
    return true;
}

typedef enum { MADE, FAILED, SKELETON_UNUSED } Made;

/*
 * Builds the plan and runs the stages for one seed. With suggestion >= 0 the harmony stage starts from that one of
 * the candidates it proposes first. FAILED if the seed gives no progression or no bass for it; SKELETON_UNUSED if
 * the progression was made without the schema or sequence that was asked for by name.
 */
static Made make_progression(const Options *options, const Parameters *parameters, unsigned long seed,
                             long suggestion, Built *built) {
    FillRecord *record = NULL;
    if (!build_plan(options, built)) {
        return FAILED;
    }
    bool ok = true;
    if (suggestion >= 0) {
        FillOperatorRegistry *registry = registry_for(options, parameters, STAGE_HARMONY);
        FillConfig config = config_for(seed);
        FillSuggestions *suggestions = NULL;
        LaneSpanId target = PLAN_LANE_SPAN_NONE;
        LaneSpanId found[2];
        if (plan_lane_find(built->plan, PLAN_LANE_HARMONY, 0, built->span, found, 2) == 1) {
            target = found[0];
        }
        FillTarget fill_target = {FILL_TARGET_LANE_SPAN, target};
        ok = registry && target != PLAN_LANE_SPAN_NONE &&
             fill_suggest(built->plan, registry, &config, fill_target, MAX_SUGGESTIONS, &suggestions) == FILL_OK &&
             (size_t)suggestion < fill_suggestions_count(suggestions) &&
             fill_apply_suggestion(built->plan, suggestions, (size_t)suggestion) == FILL_OK;
        fill_suggestions_destroy(suggestions);
        if (registry) {
            fill_registry_destroy(registry);
        }
    }
    ok = ok && run_stage(built->plan, options, seed, STAGE_HARMONY, parameters, &record) &&
         !any_open(built->plan, PLAN_LANE_HARMONY, built->span);
    bool unused = false;
    if (ok && suggestion < 0) {
        char label[FILL_LABEL_MAX];
        if (options->has_schema) {
            snprintf(label, sizeof(label), "schema:%s", options->schema_text);
            unused = unused || !chose(record, label);
        }
        if (options->has_sequence) {
            snprintf(label, sizeof(label), "sequence:%s", options->sequence_text);
            unused = unused || !chose(record, label);
        }
    }
    fill_record_destroy(record);
    if (ok && !unused && options->embellish != EMBELLISH_NONE) {
        ok = open_for_embellishment(built->plan, built->span) &&
             run_stage(built->plan, options, seed, STAGE_EMBELLISH, parameters, NULL) &&
             !any_open(built->plan, PLAN_LANE_HARMONY, built->span);
    }
    if (ok && !unused && options->bass) {
        /* The bass is built in pieces (before and after a skeleton, say), so the whole line is checked once more
           against the melodic rules; a seed whose line breaks one is not used. */
        const char *violation = "unread";
        ok = run_stage(built->plan, options, seed, STAGE_BASS, parameters, NULL) &&
             !any_open(built->plan, PLAN_LANE_BASS, built->span) &&
             harmony_check_bass(built->plan, built->span, options->key, &parameters->bass, &violation) && !violation;
    }
    if (!ok || unused) {
        plan_destroy(built->plan);
        built->plan = NULL;
    }
    return !ok ? FAILED : (unused ? SKELETON_UNUSED : MADE);
}

/* How many candidates the harmony stage proposes for the phrase, at most MAX_SUGGESTIONS. */
static size_t count_suggestions(const Options *options, const Parameters *parameters, unsigned long seed) {
    Built built;
    size_t count = 0;
    if (!build_plan(options, &built)) {
        return 0;
    }
    FillOperatorRegistry *registry = registry_for(options, parameters, STAGE_HARMONY);
    FillConfig config = config_for(seed);
    FillSuggestions *suggestions = NULL;
    LaneSpanId found[2];
    if (registry && plan_lane_find(built.plan, PLAN_LANE_HARMONY, 0, built.span, found, 2) == 1) {
        FillTarget target = {FILL_TARGET_LANE_SPAN, found[0]};
        if (fill_suggest(built.plan, registry, &config, target, MAX_SUGGESTIONS, &suggestions) == FILL_OK) {
            count = fill_suggestions_count(suggestions);
        }
    }
    fill_suggestions_destroy(suggestions);
    if (registry) {
        fill_registry_destroy(registry);
    }
    plan_destroy(built.plan);
    return count;
}

/* --- showing the result --------------------------------------------------------- */

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

/* The chords of the phrase, one row each, with the bass under them. */
static size_t collect_rows(const Options *options, const Built *built, Row *rows) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(built->plan, PLAN_LANE_HARMONY, 0, built->span, ids, MAX_CHORDS);
    if (count > MAX_CHORDS) {
        count = MAX_CHORDS;
    }
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(built->plan, ids[i]);
        const PlanHarmony *harmony = &plan_lane_span_payload(built->plan, ids[i])->harmony;
        Rational bar_start = built->span.start;
        Rational next;
        Rational offset;
        int bar = 1;
        if (rational_add(bar_start, built->bar_length, &next) != RATIONAL_OK) {
            return i;
        }
        while (rational_cmp(next, span.start) <= 0) {
            bar_start = next;
            if (rational_add(bar_start, built->bar_length, &next) != RATIONAL_OK) {
                return i;
            }
            bar++;
        }
        if (rational_sub(span.start, bar_start, &offset) != RATIONAL_OK) {
            return i;
        }
        rows[i].bar = bar;
        rows[i].beat = (int)(offset.num * options->meter.beat_unit / (long)offset.den) + 1;
        memcpy(rows[i].roman, harmony->roman_numeral, PLAN_ROMAN_NUMERAL_MAX);
        rows[i].function = function_name(harmony);
        rows[i].bass[0] = '\0';

        LaneSpanId basses[2];
        if (options->bass && plan_lane_find(built->plan, PLAN_LANE_BASS, 0, span, basses, 2) == 1) {
            const PlanNotes *notes = &plan_lane_span_payload(built->plan, basses[0])->notes;
            size_t used = 0;
            for (size_t n = 0; n < notes->note_count && used + 8 < sizeof(rows[i].bass); n++) {
                char name[8];
                pitch_name(notes->notes[n].midi, name, sizeof(name));
                used += (size_t)snprintf(rows[i].bass + used, sizeof(rows[i].bass) - used, n ? " %s" : "%s", name);
            }
        }
    }
    return count;
}

static const char *heard_name(const Options *options, const Built *built) {
    CadenceType heard;
    bool at_end;
    if (harmony_hear_cadence(built->plan, built->span, options->key.mode, options->cadence, &heard, &at_end) && at_end) {
        return cadence_type_name(heard);
    }
    return "none";
}

static void print_table(const Options *options, const Built *built) {
    Row rows[MAX_CHORDS];
    size_t count = collect_rows(options, built, rows);
    printf("%-4s %-4s %-10s %-12s %s\n", "bar", "beat", "chord", "function", options->bass ? "bass" : "");
    for (size_t i = 0; i < count; i++) {
        printf("%-4d %-4d %-10s %-12s %s\n", rows[i].bar, rows[i].beat, rows[i].roman, rows[i].function, rows[i].bass);
    }
    printf("\ncadence %s (heard: %s)\n", cli_cadence_name(options->cadence), heard_name(options, built));
}

static void print_json_chords(const Options *options, const Built *built) {
    Row rows[MAX_CHORDS];
    size_t count = collect_rows(options, built, rows);
    printf("\"heard\":\"%s\",\"chords\":[", heard_name(options, built));
    for (size_t i = 0; i < count; i++) {
        printf("%s{\"bar\":%d,\"beat\":%d,\"chord\":\"%s\",\"function\":\"%s\",\"bass\":\"%s\"}", i ? "," : "",
               rows[i].bar, rows[i].beat, rows[i].roman, rows[i].function, rows[i].bass);
    }
    printf("]");
}

static void print_result(const Options *options, const Built *alternatives, size_t count, unsigned long seed,
                         const char *const *files, size_t file_count) {
    char key[24];
    cli_key_name(&options->key, key, sizeof(key));
    if (options->json) {
        printf("{\"key\":\"%s\",\"meter\":\"%d/%d\",\"bars\":%ld,\"cadence\":\"%s\",\"seed\":%lu,", key,
               options->meter.beats_per_bar, options->meter.beat_unit, options->bars, cli_cadence_name(options->cadence),
               seed);
        if (options->suggest > 0) {
            printf("\"alternatives\":[");
            for (size_t k = 0; k < count; k++) {
                printf("%s{", k ? "," : "");
                print_json_chords(options, &alternatives[k]);
                printf("}");
            }
            printf("],");
        } else {
            print_json_chords(options, &alternatives[0]);
            printf(",");
        }
        printf("\"files\":[");
        for (size_t i = 0; i < file_count; i++) {
            printf("%s\"%s\"", i ? "," : "", files[i]);
        }
        printf("]}\n");
        return;
    }
    printf("key %s, meter %d/%d, %ld bars, cadence %s\n", key, options->meter.beats_per_bar, options->meter.beat_unit,
           options->bars, cli_cadence_name(options->cadence));
    printf("seed %lu\n", seed);
    for (size_t k = 0; k < count; k++) {
        if (options->suggest > 0) {
            printf("\nalternative %zu of %zu\n", k + 1, count);
        } else {
            printf("\n");
        }
        print_table(options, &alternatives[k]);
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
static int write_files(const Options *options, const Built *alternatives, size_t count, CliPendingFile *pending,
                       size_t *pending_count) {
    const char *paths[3] = {options->out_path, options->midi_path, options->musicxml_path};
    int status = 0;
    *pending_count = 0;
    for (size_t k = 0; k < count && status == 0; k++) {
        RenderedPlan rendered;
        bool have_rendering = false;
        for (int kind = 0; kind < 3 && status == 0; kind++) {
            if (!paths[kind]) {
                continue;
            }
            CliPendingFile *entry = &pending[*pending_count];
            if (!cli_alternative_path(paths[kind], k + 1, count > 1, entry->final_path, sizeof(entry->final_path)) ||
                snprintf(entry->temporary, sizeof(entry->temporary), "%s.tmp", entry->final_path) >=
                    (int)sizeof(entry->temporary)) {
                status = cli_fail(EXIT_WRITE, "file name '%s' is too long", paths[kind]);
                break;
            }
            (*pending_count)++;
            if (kind == 0) {
                PlanJsonError error;
                if (plan_write_json_file(alternatives[k].plan, entry->temporary, &error) != PLAN_OK) {
                    status = cli_fail(EXIT_WRITE, "cannot write '%s': %s", entry->final_path, error.message);
                }
                continue;
            }
            PlanExportError error;
            if (!have_rendering) {
                if (plan_render(alternatives[k].plan, &rendered) != PLAN_RENDER_OK) {
                    status = cli_fail(EXIT_WRITE, "cannot render the plan");
                    break;
                }
                have_rendering = true;
            }
            PlanExportResult result = kind == 1 ? rendered_plan_export_midi(&rendered, &options->export_options, entry->temporary, &error)
                                                : rendered_plan_export_musicxml(&rendered, &options->export_options, entry->temporary, &error);
            if (result != PLAN_EXPORT_OK) {
                status = cli_fail(EXIT_WRITE, "cannot write '%s': %s", entry->final_path, error.message);
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
            status = cli_fail(EXIT_WRITE, "cannot write '%s'", pending[i].final_path);
            break;
        }
    }
    if (status != 0) {
        cli_discard_pending(pending, *pending_count);
    }
    return status;
}

/* --- the command ---------------------------------------------------------------- */

static void list_names(void) {
    printf("Schemas (--schema <name>), as Ursatz's galant-schemas framework names them:\n");
    for (int id = 0; id < GALANT_SCHEMA_COUNT; id++) {
        bool major = harmony_schema_available((GalantSchemaId)id, KEY_STRUCTURE_MODE_MAJOR);
        bool minor = harmony_schema_available((GalantSchemaId)id, KEY_STRUCTURE_MODE_MINOR);
        if (major || minor) {
            printf("  %-24s %-15s %s\n", galant_schema_name((GalantSchemaId)id),
                   schema_function_name(galant_prototype((GalantSchemaId)id)->function),
                   major && minor ? "major, minor" : (major ? "major" : "minor"));
        }
    }
    printf("\nSequences (--sequence <name>), as scale-harmonization names them:\n");
    for (int id = 0; id < SCALE_PATTERN_COUNT; id++) {
        printf("  %s\n", scale_pattern_name((ScalePatternId)id));
    }
    printf("\nSchemas with weak stages need 4/4 or 6/8; sequences work in every meter.\n");
}

int run_progression(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    if (options.list_schemas) {
        list_names();
        return 0;
    }

    Parameters parameters;
    Built built[MAX_SUGGESTIONS];
    size_t made = 0;
    unsigned long seed = options.seed;
    bool skeleton_unused = false;
    parameters_for(&options, &parameters);
    memset(built, 0, sizeof(built));

    if (options.suggest > 0) {
        /* The alternatives are the candidates the harmony stage proposes, each taken through the other stages. */
        size_t proposed = count_suggestions(&options, &parameters, seed);
        for (size_t i = 0; i < proposed && made < options.suggest; i++) {
            if (make_progression(&options, &parameters, seed, (long)i, &built[made]) == MADE) {
                made++;
            }
        }
        if (made < options.suggest && made > 0) {
            fprintf(stderr, "Note: only %zu alternative%s found for this request\n", made, made == 1 ? "" : "s");
        }
    } else {
        for (int attempt = 0; attempt < MAX_SEED_TRIES && made == 0; attempt++) {
            seed = (options.seed + (unsigned long)attempt) & 0xFFFFFFFFUL;
            Made result = make_progression(&options, &parameters, seed, -1, &built[0]);
            skeleton_unused = skeleton_unused || result == SKELETON_UNUSED;
            made += result == MADE;
        }
    }
    if (made == 0) {
        if (skeleton_unused) {
            return cli_fail(EXIT_UNSATISFIABLE, "%s could not be used for %s, %s, %ld bars, %s from seeds %lu to %lu",
                        options.has_schema ? "the schema" : "the sequence", options.key_text, options.meter_text,
                        options.bars, cli_cadence_name(options.cadence), options.seed,
                        (options.seed + MAX_SEED_TRIES - 1) & 0xFFFFFFFFUL);
        }
        return cli_fail(EXIT_UNSATISFIABLE, "no valid progression for %s, %s, %ld bars, %s from seeds %lu to %lu",
                    options.key_text, options.meter_text, options.bars, cli_cadence_name(options.cadence), options.seed,
                    (options.seed + MAX_SEED_TRIES - 1) & 0xFFFFFFFFUL);
    }

    CliPendingFile pending[MAX_FILES];
    const char *files[MAX_FILES];
    size_t pending_count = 0;
    status = write_files(&options, built, made, pending, &pending_count);
    if (status == 0) {
        for (size_t i = 0; i < pending_count; i++) {
            files[i] = pending[i].final_path;
        }
        print_result(&options, built, made, seed, files, pending_count);
    }
    for (size_t i = 0; i < made; i++) {
        plan_destroy(built[i].plan);
    }
    return status;
}
