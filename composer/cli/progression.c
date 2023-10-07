#include "progression.h"

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

#define EXIT_USAGE 1
#define EXIT_UNSATISFIABLE 2
#define EXIT_WRITE 3

#define MAX_BARS 64
/* The seeds tried, one after another from the one given, before a request is given up on. */
#define MAX_SEED_TRIES 16
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
    const char *schema_text;
    const char *sequence_text;
    const char *suggest_text;
    bool list_schemas;
    bool bass;
    /* Read from the text above. */
    PlanKey key;
    PlanMeter meter;
    long bars;
    PlanCadenceType cadence;
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

static int fail(int status, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    return status;
}

/* --- reading the options -------------------------------------------------- */

static bool equals_ignore_case(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
    }
    return *a == *b;
}

/* A tonic letter, an optional # or b, whitespace, then major or minor. */
static bool parse_key(const char *text, PlanKey *key) {
    static const int NATURAL[7] = {9, 11, 0, 2, 4, 5, 7}; /* A B C D E F G */
    while (isspace((unsigned char)*text)) {
        text++;
    }
    char letter = (char)toupper((unsigned char)*text);
    if (letter < 'A' || letter > 'G') {
        return false;
    }
    int pc = NATURAL[letter - 'A'];
    text++;
    if (*text == '#') {
        pc++;
        text++;
    } else if (*text == 'b') {
        pc--;
        text++;
    }
    if (!isspace((unsigned char)*text)) {
        return false;
    }
    while (isspace((unsigned char)*text)) {
        text++;
    }
    char mode[8];
    size_t length = 0;
    while (text[length] && !isspace((unsigned char)text[length]) && length + 1 < sizeof(mode)) {
        mode[length] = text[length];
        length++;
    }
    mode[length] = '\0';
    const char *rest = text + length;
    while (isspace((unsigned char)*rest)) {
        rest++;
    }
    if (*rest != '\0') {
        return false;
    }
    if (equals_ignore_case(mode, "major")) {
        key->mode = PLAN_MODE_MAJOR;
    } else if (equals_ignore_case(mode, "minor")) {
        key->mode = PLAN_MODE_MINOR;
    } else {
        return false;
    }
    key->tonic_pitch_class = (pc % 12 + 12) % 12;
    return true;
}

static bool parse_whole_number(const char *text, unsigned long maximum, unsigned long *out) {
    char *end = NULL;
    if (!text || !isdigit((unsigned char)text[0])) {
        return false;
    }
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 || *end != '\0' || value > maximum) {
        return false;
    }
    *out = value;
    return true;
}

static bool parse_meter(const char *text, PlanMeter *meter) {
    unsigned long beats;
    unsigned long unit;
    char first[16];
    const char *slash = strchr(text, '/');
    if (!slash || (size_t)(slash - text) >= sizeof(first)) {
        return false;
    }
    memcpy(first, text, (size_t)(slash - text));
    first[slash - text] = '\0';
    if (!parse_whole_number(first, 64, &beats) || !parse_whole_number(slash + 1, 64, &unit) || beats == 0 ||
        unit == 0) {
        return false;
    }
    meter->beats_per_bar = (int)beats;
    meter->beat_unit = (int)unit;
    meter->bars_per_hyperbar = 0;
    return true;
}

static bool parse_cadence(const char *text, PlanCadenceType *cadence) {
    static const char *const NAMES[5] = {"PAC", "IAC", "HC", "DC", "PC"};
    for (int i = 0; i < 5; i++) {
        if (equals_ignore_case(text, NAMES[i])) {
            *cadence = (PlanCadenceType)i;
            return true;
        }
    }
    return false;
}

static const char *cadence_name(PlanCadenceType cadence) {
    static const char *const NAMES[5] = {"PAC", "IAC", "HC", "DC", "PC"};
    return NAMES[cadence];
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
        } else {
            return fail(EXIT_USAGE, "unexpected argument '%s' (see ursatz-composer progression --help)", arg);
        }
        if (target) {
            if (i + 1 >= argc) {
                return fail(EXIT_USAGE, "%s needs a value", arg);
            }
            if (*target) {
                return fail(EXIT_USAGE, "%s given twice", arg);
            }
            *target = argv[++i];
        }
    }
    if (options->help || options->list_schemas) {
        return 0;
    }
    if (!options->key_text || !options->meter_text || !options->bars_text || !options->cadence_text) {
        return fail(EXIT_USAGE, "progression needs --key, --meter, --bars and --cadence (see --help)");
    }
    if (!parse_key(options->key_text, &options->key)) {
        return fail(EXIT_USAGE,
                    "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                    "for example \"D minor\"",
                    options->key_text);
    }
    if (!parse_meter(options->meter_text, &options->meter) || !harmony_meter_supported(&options->meter)) {
        return fail(EXIT_USAGE, "meter '%s' is not supported; use 2/4, 3/4, 4/4 or 6/8", options->meter_text);
    }
    unsigned long bars;
    if (!parse_whole_number(options->bars_text, MAX_BARS, &bars) || bars == 0) {
        return fail(EXIT_USAGE, "--bars must be a whole number from 1 to %d, not '%s'", MAX_BARS, options->bars_text);
    }
    options->bars = (long)bars;
    if (!parse_cadence(options->cadence_text, &options->cadence)) {
        return fail(EXIT_USAGE, "unknown cadence '%s'; use PAC, IAC, HC, DC or PC", options->cadence_text);
    }
    if (options->seed_text) {
        if (!parse_whole_number(options->seed_text, 4294967295UL, &options->seed)) {
            return fail(EXIT_USAGE, "--seed must be a whole number from 0 to 4294967295, not '%s'", options->seed_text);
        }
    } else {
        options->seed = ((unsigned long)time(NULL) * 2654435761UL + (unsigned long)clock()) & 0xFFFFFFFFUL;
    }
    options->embellish = EMBELLISH_NONE;
    if (options->embellish_text) {
        static const char *const LEVELS[4] = {"none", "low", "medium", "high"};
        bool found = false;
        for (int i = 0; i < 4; i++) {
            if (equals_ignore_case(options->embellish_text, LEVELS[i])) {
                options->embellish = (EmbellishLevel)i;
                found = true;
            }
        }
        if (!found) {
            return fail(EXIT_USAGE, "--embellish must be none, low, medium or high, not '%s'", options->embellish_text);
        }
    }
    if (options->format_text) {
        if (equals_ignore_case(options->format_text, "json")) {
            options->json = true;
        } else if (!equals_ignore_case(options->format_text, "text")) {
            return fail(EXIT_USAGE, "--format must be text or json, not '%s'", options->format_text);
        }
    }
    if (options->schema_text) {
        if (!harmony_schema_find(options->schema_text, &options->schema)) {
            return fail(EXIT_USAGE, "unknown schema '%s' (see --list-schemas)", options->schema_text);
        }
        options->has_schema = true;
        if (!harmony_schema_available(options->schema, options->key.mode)) {
            return fail(EXIT_UNSATISFIABLE, "the schema '%s' cannot be written in a %s key", options->schema_text,
                        options->key.mode == PLAN_MODE_MINOR ? "minor" : "major");
        }
    }
    if (options->sequence_text) {
        if (!harmony_sequence_find(options->sequence_text, &options->sequence)) {
            return fail(EXIT_USAGE, "unknown sequence '%s' (see --list-schemas)", options->sequence_text);
        }
        options->has_sequence = true;
    }
    if (options->suggest_text &&
        (!parse_whole_number(options->suggest_text, MAX_SUGGESTIONS, &options->suggest) || options->suggest == 0)) {
        return fail(EXIT_USAGE, "--suggest must be a whole number from 1 to %d, not '%s'", MAX_SUGGESTIONS,
                    options->suggest_text);
    }
    if (!options->bass && (options->has_schema || options->has_sequence)) {
        return fail(EXIT_USAGE, "--schema and --sequence write their own bass, so they cannot be used with --no-bass");
    }
    if (!options->bass && (options->midi_path || options->musicxml_path)) {
        return fail(EXIT_USAGE, "--midi and --musicxml write the bass line (harmony makes no sound yet), "
                                "so they cannot be used with --no-bass");
    }
    size_t minimum = harmony_phrase_min_bars(options->cadence, &options->meter);
    if ((size_t)options->bars < minimum) {
        return fail(EXIT_UNSATISFIABLE, "%ld bar%s is too short for a %s in %s; at least %zu bars are needed",
                    options->bars, options->bars == 1 ? "" : "s", cadence_name(options->cadence), options->meter_text,
                    minimum);
    }
    return 0;
}

/* --- building the plan ------------------------------------------------------ */

typedef struct {
    PiecePlan *plan;
    PlanSpan span;
    PlanTime bar_length;
} Built;

static PlanTime time_of(long num, unsigned long den) {
    PlanTime time;
    plan_time_make(num, den, &time);
    return time;
}

static PlanTime time_plus(PlanTime a, PlanTime b) {
    PlanTime sum;
    plan_time_add(a, b, &sum);
    return sum;
}

static PlanTime time_minus(PlanTime a, PlanTime b) {
    return time_of(a.num * (long)b.den - b.num * (long)a.den, a.den * b.den);
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

    /* The open harmony span asks for the cadence; its placeholder chord is replaced by the operators. */
    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, options->key.mode == PLAN_MODE_MINOR ? "i" : "I");
    payload.harmony.function = PLAN_FUNCTION_UNKNOWN;
    ok = ok && plan_add_lane_span(built->plan, phrase, PLAN_LANE_HARMONY, 0, built->span, &payload, &id) == PLAN_OK &&
         plan_lane_span_add_constraint(built->plan, id, &cadence) == PLAN_OK;
    if (options->bass) {
        PlanNote note = {40, time_of(0, 1), built->span.length};
        memset(&payload, 0, sizeof(payload));
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        ok = ok && plan_add_lane_span(built->plan, phrase, PLAN_LANE_BASS, 0, built->span, &payload, &id) == PLAN_OK;
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
        ok = run_stage(built->plan, options, seed, STAGE_BASS, parameters, NULL) &&
             !any_open(built->plan, PLAN_LANE_BASS, built->span);
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

static const char *function_name(PlanHarmonicFunction function) {
    switch (function) {
    case PLAN_FUNCTION_TONIC:
        return "tonic";
    case PLAN_FUNCTION_PREDOMINANT:
        return "predominant";
    case PLAN_FUNCTION_DOMINANT:
        return "dominant";
    default:
        return "unknown";
    }
}

static void pitch_name(int midi, char *out, size_t size) {
    static const char *const NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    snprintf(out, size, "%s%d", NAMES[((midi % 12) + 12) % 12], midi / 12 - 1);
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
        PlanTime bar_start = built->span.start;
        PlanTime next = time_plus(bar_start, built->bar_length);
        int bar = 1;
        while (plan_time_compare(next, span.start) <= 0) {
            bar_start = next;
            next = time_plus(bar_start, built->bar_length);
            bar++;
        }
        PlanTime offset = time_minus(span.start, bar_start);
        rows[i].bar = bar;
        rows[i].beat = (int)(offset.num * options->meter.beat_unit / (long)offset.den) + 1;
        memcpy(rows[i].roman, harmony->roman_numeral, PLAN_ROMAN_NUMERAL_MAX);
        rows[i].function = function_name(harmony->function);
        rows[i].bass[0] = '\0';

        LaneSpanId basses[2];
        if (options->bass && plan_lane_find(built->plan, PLAN_LANE_BASS, 0, span, basses, 2) == 1) {
            const PlanNotes *notes = &plan_lane_span_payload(built->plan, basses[0])->notes;
            size_t used = 0;
            for (size_t n = 0; n < notes->note_count && used + 8 < sizeof(rows[i].bass); n++) {
                char name[8];
                pitch_name(notes->notes[n].midi_pitch, name, sizeof(name));
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

static const char *key_name(const Options *options, char *out, size_t size) {
    static const char *const MAJOR[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    static const char *const MINOR[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};
    int pc = options->key.tonic_pitch_class;
    snprintf(out, size, "%s %s", options->key.mode == PLAN_MODE_MINOR ? MINOR[pc] : MAJOR[pc],
             options->key.mode == PLAN_MODE_MINOR ? "minor" : "major");
    return out;
}

static void print_table(const Options *options, const Built *built) {
    Row rows[MAX_CHORDS];
    size_t count = collect_rows(options, built, rows);
    printf("%-4s %-4s %-10s %-12s %s\n", "bar", "beat", "chord", "function", options->bass ? "bass" : "");
    for (size_t i = 0; i < count; i++) {
        printf("%-4d %-4d %-10s %-12s %s\n", rows[i].bar, rows[i].beat, rows[i].roman, rows[i].function, rows[i].bass);
    }
    printf("\ncadence %s (heard: %s)\n", cadence_name(options->cadence), heard_name(options, built));
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
    key_name(options, key, sizeof(key));
    if (options->json) {
        printf("{\"key\":\"%s\",\"meter\":\"%d/%d\",\"bars\":%ld,\"cadence\":\"%s\",\"seed\":%lu,", key,
               options->meter.beats_per_bar, options->meter.beat_unit, options->bars, cadence_name(options->cadence),
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
           options->bars, cadence_name(options->cadence));
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

typedef struct {
    char final_path[1024];
    char temporary[1032];
} Pending;

static void discard(Pending *pending, size_t count) {
    for (size_t i = 0; i < count; i++) {
        remove(pending[i].temporary);
    }
}

/* The name of alternative k (from 1) of a file: before the extension, "-k". With one result the name as given. */
static bool alternative_path(const char *path, size_t k, bool several, char *out, size_t size) {
    if (!several) {
        return snprintf(out, size, "%s", path) < (int)size;
    }
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (!dot || (slash && dot < slash)) {
        return snprintf(out, size, "%s-%zu", path, k) < (int)size;
    }
    return snprintf(out, size, "%.*s-%zu%s", (int)(dot - path), path, k, dot) < (int)size;
}

/*
 * Writes every output to a temporary file next to its final name, and renames them all only when every one was
 * written, so that an error leaves no output file behind.
 */
static int write_files(const Options *options, const Built *alternatives, size_t count, Pending *pending,
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
            Pending *entry = &pending[*pending_count];
            if (!alternative_path(paths[kind], k + 1, count > 1, entry->final_path, sizeof(entry->final_path)) ||
                snprintf(entry->temporary, sizeof(entry->temporary), "%s.tmp", entry->final_path) >=
                    (int)sizeof(entry->temporary)) {
                status = fail(EXIT_WRITE, "file name '%s' is too long", paths[kind]);
                break;
            }
            (*pending_count)++;
            if (kind == 0) {
                PlanJsonError error;
                if (plan_write_json_file(alternatives[k].plan, entry->temporary, &error) != PLAN_OK) {
                    status = fail(EXIT_WRITE, "cannot write '%s': %s", entry->final_path, error.message);
                }
                continue;
            }
            PlanExportError error;
            if (!have_rendering) {
                if (plan_render(alternatives[k].plan, &rendered) != PLAN_RENDER_OK) {
                    status = fail(EXIT_WRITE, "cannot render the plan");
                    break;
                }
                have_rendering = true;
            }
            PlanExportResult result = kind == 1 ? rendered_plan_export_midi(&rendered, entry->temporary, &error)
                                                : rendered_plan_export_musicxml(&rendered, entry->temporary, &error);
            if (result != PLAN_EXPORT_OK) {
                status = fail(EXIT_WRITE, "cannot write '%s': %s", entry->final_path, error.message);
            }
        }
        if (have_rendering) {
            rendered_plan_destroy(&rendered);
        }
    }
    if (status != 0) {
        discard(pending, *pending_count);
        return status;
    }
    for (size_t i = 0; i < *pending_count; i++) {
        if (rename(pending[i].temporary, pending[i].final_path) != 0) {
            status = fail(EXIT_WRITE, "cannot write '%s'", pending[i].final_path);
            break;
        }
    }
    if (status != 0) {
        discard(pending, *pending_count);
    }
    return status;
}

/* --- the command ---------------------------------------------------------------- */

static void list_names(void) {
    printf("Schemas (--schema <name>), as Ursatz's galant-schemas framework names them:\n");
    for (int id = 0; id < GALANT_SCHEMA_COUNT; id++) {
        bool major = harmony_schema_available((GalantSchemaId)id, PLAN_MODE_MAJOR);
        bool minor = harmony_schema_available((GalantSchemaId)id, PLAN_MODE_MINOR);
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
            return fail(EXIT_UNSATISFIABLE, "%s could not be used for %s, %s, %ld bars, %s from seeds %lu to %lu",
                        options.has_schema ? "the schema" : "the sequence", options.key_text, options.meter_text,
                        options.bars, cadence_name(options.cadence), options.seed,
                        (options.seed + MAX_SEED_TRIES - 1) & 0xFFFFFFFFUL);
        }
        return fail(EXIT_UNSATISFIABLE, "no valid progression for %s, %s, %ld bars, %s from seeds %lu to %lu",
                    options.key_text, options.meter_text, options.bars, cadence_name(options.cadence), options.seed,
                    (options.seed + MAX_SEED_TRIES - 1) & 0xFFFFFFFFUL);
    }

    Pending pending[MAX_FILES];
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
