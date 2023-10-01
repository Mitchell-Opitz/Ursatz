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
#include <ursatz_composer/harmony_bass.h>
#include <ursatz_composer/harmony_embellish.h>
#include <ursatz_composer/harmony_phrase.h>
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
#define MAX_FILES 3

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
    bool bass;
    /* Read from the text above. */
    PlanKey key;
    PlanMeter meter;
    long bars;
    PlanCadenceType cadence;
    unsigned long seed;
    EmbellishLevel embellish;
    bool json;
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
    if (options->help) {
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
}

typedef enum { STAGE_HARMONY, STAGE_EMBELLISH, STAGE_BASS } Stage;

/* One run of the fill loop with the operators of a stage. False only if the loop itself fails. */
static bool run_stage(PiecePlan *plan, unsigned long seed, Stage stage, const Parameters *parameters) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    if (!registry) {
        return false;
    }
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = harmony_phrase_checker;
    FillResult registered = FILL_OK;
    switch (stage) {
    case STAGE_HARMONY:
        registered = harmony_phrase_register(registry);
        break;
    case STAGE_EMBELLISH:
        registered = harmony_embellish_register(registry, &parameters->embellish);
        break;
    case STAGE_BASS:
        registered = harmony_bass_register(registry, &parameters->bass);
        break;
    }
    bool ok = registered == FILL_OK && fill_run(plan, registry, &config, &outcome, NULL) == FILL_OK;
    if (ok) {
        fill_outcome_destroy(&outcome);
    }
    fill_registry_destroy(registry);
    return ok;
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

/* Builds the plan and runs the stages for one seed. NULL if the seed gives no progression (or no bass for it). */
static PiecePlan *make_progression(const Options *options, const Parameters *parameters, unsigned long seed,
                                   Built *built) {
    if (!build_plan(options, built)) {
        return NULL;
    }
    bool ok = run_stage(built->plan, seed, STAGE_HARMONY, parameters) && !any_open(built->plan, PLAN_LANE_HARMONY, built->span);
    if (ok && options->embellish != EMBELLISH_NONE) {
        ok = open_for_embellishment(built->plan, built->span) &&
             run_stage(built->plan, seed, STAGE_EMBELLISH, parameters) &&
             !any_open(built->plan, PLAN_LANE_HARMONY, built->span);
    }
    if (ok && options->bass) {
        ok = run_stage(built->plan, seed, STAGE_BASS, parameters) && !any_open(built->plan, PLAN_LANE_BASS, built->span);
    }
    if (!ok) {
        plan_destroy(built->plan);
        built->plan = NULL;
    }
    return built->plan;
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

static void print_text(const Options *options, const Built *built, unsigned long seed, const char *const *files,
                       size_t file_count) {
    Row rows[MAX_CHORDS];
    char key[24];
    size_t count = collect_rows(options, built, rows);
    printf("key %s, meter %d/%d, %ld bars, cadence %s\n", key_name(options, key, sizeof(key)),
           options->meter.beats_per_bar, options->meter.beat_unit, options->bars, cadence_name(options->cadence));
    printf("seed %lu\n\n", seed);
    printf("%-4s %-4s %-10s %-12s %s\n", "bar", "beat", "chord", "function", options->bass ? "bass" : "");
    for (size_t i = 0; i < count; i++) {
        printf("%-4d %-4d %-10s %-12s %s\n", rows[i].bar, rows[i].beat, rows[i].roman, rows[i].function, rows[i].bass);
    }
    printf("\ncadence %s (heard: %s)\n", cadence_name(options->cadence), heard_name(options, built));
    for (size_t i = 0; i < file_count; i++) {
        printf("wrote %s\n", files[i]);
    }
}

static void print_json(const Options *options, const Built *built, unsigned long seed, const char *const *files,
                       size_t file_count) {
    Row rows[MAX_CHORDS];
    char key[24];
    size_t count = collect_rows(options, built, rows);
    printf("{\"key\":\"%s\",\"meter\":\"%d/%d\",\"bars\":%ld,\"cadence\":\"%s\",\"heard\":\"%s\",\"seed\":%lu,",
           key_name(options, key, sizeof(key)), options->meter.beats_per_bar, options->meter.beat_unit, options->bars,
           cadence_name(options->cadence), heard_name(options, built), seed);
    printf("\"chords\":[");
    for (size_t i = 0; i < count; i++) {
        printf("%s{\"bar\":%d,\"beat\":%d,\"chord\":\"%s\",\"function\":\"%s\",\"bass\":\"%s\"}", i ? "," : "", rows[i].bar,
               rows[i].beat, rows[i].roman, rows[i].function, rows[i].bass);
    }
    printf("],\"files\":[");
    for (size_t i = 0; i < file_count; i++) {
        printf("%s\"%s\"", i ? "," : "", files[i]);
    }
    printf("]}\n");
}

/* --- writing the files ---------------------------------------------------------- */

typedef struct {
    char temporary[1024];
    const char *final_path;
} Pending;

static void discard(Pending *pending, size_t count) {
    for (size_t i = 0; i < count; i++) {
        remove(pending[i].temporary);
    }
}

/*
 * Writes every output to a temporary file next to its final name, and renames them all only when every one was
 * written, so that an error leaves no output file behind.
 */
static int write_files(const Options *options, const Built *built, const char **written, size_t *written_count) {
    Pending pending[MAX_FILES];
    size_t count = 0;
    const char *paths[MAX_FILES] = {options->out_path, options->midi_path, options->musicxml_path};
    RenderedPlan rendered;
    bool have_rendering = false;
    int status = 0;
    for (int kind = 0; kind < MAX_FILES && status == 0; kind++) {
        if (!paths[kind]) {
            continue;
        }
        if (snprintf(pending[count].temporary, sizeof(pending[count].temporary), "%s.tmp", paths[kind]) >=
            (int)sizeof(pending[count].temporary)) {
            status = fail(EXIT_WRITE, "file name '%s' is too long", paths[kind]);
            break;
        }
        pending[count].final_path = paths[kind];
        count++;
        if (kind == 0) {
            PlanJsonError error;
            if (plan_write_json_file(built->plan, pending[count - 1].temporary, &error) != PLAN_OK) {
                status = fail(EXIT_WRITE, "cannot write '%s': %s", paths[kind], error.message);
            }
        } else {
            PlanExportError error;
            if (!have_rendering) {
                if (plan_render(built->plan, &rendered) != PLAN_RENDER_OK) {
                    status = fail(EXIT_WRITE, "cannot render the plan");
                    break;
                }
                have_rendering = true;
            }
            PlanExportResult result = kind == 1
                                          ? rendered_plan_export_midi(&rendered, pending[count - 1].temporary, &error)
                                          : rendered_plan_export_musicxml(&rendered, pending[count - 1].temporary, &error);
            if (result != PLAN_EXPORT_OK) {
                status = fail(EXIT_WRITE, "cannot write '%s': %s", paths[kind], error.message);
            }
        }
    }
    if (have_rendering) {
        rendered_plan_destroy(&rendered);
    }
    if (status != 0) {
        discard(pending, count);
        return status;
    }
    for (size_t i = 0; i < count; i++) {
        if (rename(pending[i].temporary, pending[i].final_path) != 0) {
            status = fail(EXIT_WRITE, "cannot write '%s'", pending[i].final_path);
            break;
        }
        written[(*written_count)++] = pending[i].final_path;
    }
    if (status != 0) {
        discard(pending, count);
    }
    return status;
}

/* --- the command ---------------------------------------------------------------- */

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

    Parameters parameters;
    Built built;
    unsigned long seed = options.seed;
    parameters_for(&options, &parameters);
    PiecePlan *plan = NULL;
    for (int attempt = 0; attempt < MAX_SEED_TRIES && !plan; attempt++) {
        seed = (options.seed + (unsigned long)attempt) & 0xFFFFFFFFUL;
        plan = make_progression(&options, &parameters, seed, &built);
    }
    if (!plan) {
        return fail(EXIT_UNSATISFIABLE, "no valid progression for %s, %s, %ld bars, %s from seeds %lu to %lu",
                    options.key_text, options.meter_text, options.bars, cadence_name(options.cadence), options.seed,
                    (options.seed + MAX_SEED_TRIES - 1) & 0xFFFFFFFFUL);
    }

    const char *written[MAX_FILES];
    size_t written_count = 0;
    status = write_files(&options, &built, written, &written_count);
    if (status == 0) {
        if (options.json) {
            print_json(&options, &built, seed, written, written_count);
        } else {
            print_text(&options, &built, seed, written, written_count);
        }
    }
    plan_destroy(built.plan);
    return status;
}
