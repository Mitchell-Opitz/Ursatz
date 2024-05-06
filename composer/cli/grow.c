#include "grow.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <phrase_beginning.h>
#include <phrase_ending.h>
#include <phrase_level_form.h>

#include "cli_options.h"

#include <ursatz_composer/export.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>

#define MAX_SUGGESTIONS 16
#define MAX_FILES (3 * MAX_SUGGESTIONS)
#define MAX_CHORDS 96
#define MAX_LANE_SPANS 128

typedef enum { INTO_SENTENCE, INTO_PERIOD, INTO_HYBRID } Into;

typedef struct {
    bool help;
    const char *input;
    const char *into_text;
    const char *hybrid_text;
    const char *key_text;
    const char *meter_text;
    const char *bars_text;
    const char *cadence_text;
    const char *first_cadence_text;
    const char *seed_text;
    const char *suggest_text;
    const char *out_path;
    const char *midi_path;
    const char *musicxml_path;
    const char *format_text;
    /* Read from the text above. */
    Into into;
    PlanThemePairing pairing;
    PlanKey key;
    PlanMeter meter;
    size_t bars;
    PlanCadenceType cadence;
    PlanCadenceType first_cadence;
    unsigned long seed;
    unsigned long suggest;
    bool json;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", GROW_USAGE_LINE + 7);
    fprintf(out, "Grows a theme from the motive in <motive-input> (a MIDI file, a MusicXML file, a note list or a motive\n");
    fprintf(out, "JSON file): its structure, a harmony, a bass line and a melody, ending in the cadence asked for.\n\n");
    fprintf(out, "Required:\n");
    fprintf(out, "  --into <form>        sentence (presentation + continuation), period (antecedent + consequent) or\n");
    fprintf(out, "                       hybrid (any other pairing, named with --hybrid)\n");
    fprintf(out, "  --key <key>          tonic and mode in one argument (\"D minor\", \"F# major\"); any case\n");
    fprintf(out, "  --meter <n/m>        2/4, 3/4, 4/4 or 6/8; the motive must fit in one bar of it\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --hybrid <b>+<e>     with --into hybrid only: the beginning (presentation, antecedent or cbi, a compound\n");
    fprintf(out, "                       basic idea) and the ending (continuation, consequent or cadential), for example\n");
    fprintf(out, "                       antecedent+continuation\n");
    fprintf(out, "  --bars <4|8>         length of the theme (default 8; 4 uses a one-bar basic idea)\n");
    fprintf(out, "  --cadence <type>     PAC (default), IAC or HC: the cadence the theme ends in\n");
    fprintf(out, "  --first-cadence <t>  HC (default) or IAC: the weaker cadence an antecedent ends in (a period or a hybrid\n");
    fprintf(out, "                       that begins with an antecedent only)\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same seed gives the same output. Default: derived from\n");
    fprintf(out, "                       the clock. The seed used is always printed (if a seed gets stuck, the next seeds are\n");
    fprintf(out, "                       tried, up to %d, and the one that worked is printed)\n", CLI_MAX_SEED_TRIES);
    fprintf(out, "  --suggest <n>        write n different themes (1 to %d) instead of one; with files, theme k goes to\n", MAX_SUGGESTIONS);
    fprintf(out, "                       <name>-k.<extension>\n");
    fprintf(out, "  --out <file.json>    write the plan as JSON\n");
    fprintf(out, "  --midi <file>        write the melody and the bass as a MIDI file\n");
    fprintf(out, "  --musicxml <file>    write the melody and the bass as MusicXML\n");
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
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--into") == 0) {
            target = &options->into_text;
        } else if (strcmp(arg, "--hybrid") == 0) {
            target = &options->hybrid_text;
        } else if (strcmp(arg, "--key") == 0) {
            target = &options->key_text;
        } else if (strcmp(arg, "--meter") == 0) {
            target = &options->meter_text;
        } else if (strcmp(arg, "--bars") == 0) {
            target = &options->bars_text;
        } else if (strcmp(arg, "--cadence") == 0) {
            target = &options->cadence_text;
        } else if (strcmp(arg, "--first-cadence") == 0) {
            target = &options->first_cadence_text;
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
    if (!options->input) {
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
    if (!cli_parse_key(options->key_text, &options->key)) {
        return cli_fail(CLI_EXIT_USAGE,
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                        "for example \"D minor\"",
                        options->key_text);
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
    options->cadence = PLAN_CADENCE_PAC;
    if (options->cadence_text &&
        (!cli_parse_cadence(options->cadence_text, &options->cadence) || options->cadence == PLAN_CADENCE_DC ||
         options->cadence == PLAN_CADENCE_PC)) {
        return cli_fail(CLI_EXIT_USAGE, "--cadence must be PAC, IAC or HC, not '%s'", options->cadence_text);
    }
    options->first_cadence = PLAN_CADENCE_HC;
    if (options->first_cadence_text) {
        if (options->pairing.beginning != PLAN_THEME_BEGINNING_ANTECEDENT) {
            return cli_fail(CLI_EXIT_USAGE, "--first-cadence applies only to a period or a hybrid that begins with an "
                                            "antecedent");
        }
        if (!cli_parse_cadence(options->first_cadence_text, &options->first_cadence) ||
            (options->first_cadence != PLAN_CADENCE_HC && options->first_cadence != PLAN_CADENCE_IAC)) {
            return cli_fail(CLI_EXIT_USAGE, "--first-cadence must be HC or IAC, not '%s'", options->first_cadence_text);
        }
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
} Theme;

static void theme_destroy(Theme *theme) {
    plan_destroy(theme->plan);
    grow_outcome_destroy(&theme->outcome);
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
            if (!text_append(&text, "|%d@%ld/%lu+%ld/%lu:", lane, at.start.num, at.start.den, at.length.num,
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
                if (!text_append(&text, "%d@%ld/%lu+%ld/%lu,", note->midi_pitch, note->offset.num, note->offset.den,
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
static GrowResult grow_one(const Options *options, const Motive *motive, unsigned long seed, Theme *theme) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key = options->key;
    request.meter = options->meter;
    request.bars = options->bars;
    request.pairing = options->pairing;
    request.cadence = options->cadence;
    request.first_cadence = options->first_cadence;
    memset(theme, 0, sizeof(*theme));
    theme->seed = seed;
    GrowResult built = grow_build_plan(&request, &theme->plan);
    if (built != GROW_OK) {
        return built;
    }
    GrowConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    GrowResult ran = grow_run(theme->plan, &config, &theme->outcome);
    if (ran == GROW_OK) {
        NodeId theme_node = find_node(theme->plan, "theme");
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

static double in_bars(PlanTime time, PlanTime bar) {
    return ((double)time.num / (double)time.den) / ((double)bar.num / (double)bar.den);
}

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

/* The chords of the theme, one row each, with the bass under them. */
static size_t collect_rows(const Options *options, const PiecePlan *plan, NodeId theme, Row *rows) {
    PlanSpan span = plan_node_span(plan, theme);
    PlanTime bar;
    plan_time_make(options->meter.beats_per_bar, (unsigned long)options->meter.beat_unit, &bar);
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
        rows[i].function = function_name(harmony->function);
        rows[i].bass[0] = '\0';
        LaneSpanId basses[2];
        if (plan_lane_find(plan, PLAN_LANE_BASS, 0, chord, basses, 2) == 1) {
            const PlanNotes *notes = &plan_lane_span_payload(plan, basses[0])->notes;
            size_t used = 0;
            for (size_t n = 0; n < notes->note_count && used + 8 < sizeof(rows[i].bass); n++) {
                char name[16];
                pitch_name(notes->notes[n].midi_pitch, name, sizeof(name));
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
static double offset_in_bars(PlanTime time, PlanSpan theme, PlanTime bar) {
    PlanTime offset;
    offset.num = time.num * (long)theme.start.den - theme.start.num * (long)time.den;
    offset.den = time.den * theme.start.den;
    return in_bars(offset, bar);
}

static void print_node(const PiecePlan *plan, NodeId node, PlanSpan theme, PlanTime bar, int depth) {
    PlanSpan span = plan_node_span(plan, node);
    PlanTime end;
    plan_time_add(span.start, span.length, &end);
    const char *name = plan_node_name(plan, node);
    printf("%*s%s (%s) bars %g-%g\n", 2 + 2 * depth, "", name ? name : "?", KIND_NAMES[plan_node_kind(plan, node)],
           offset_in_bars(span.start, theme, bar), offset_in_bars(end, theme, bar));
    for (size_t i = 0; i < plan_node_child_count(plan, node); i++) {
        print_node(plan, plan_node_child_at(plan, node, i), theme, bar, depth + 1);
    }
}

static void print_json_string(const char *text) {
    putchar('"');
    for (; *text; text++) {
        if (*text == '"' || *text == '\\') {
            putchar('\\');
        }
        if ((unsigned char)*text >= 0x20) {
            putchar(*text);
        }
    }
    putchar('"');
}

static void print_json_node(const PiecePlan *plan, NodeId node, PlanSpan theme, PlanTime bar, bool *first) {
    PlanSpan span = plan_node_span(plan, node);
    PlanTime end;
    plan_time_add(span.start, span.length, &end);
    const char *name = plan_node_name(plan, node);
    printf("%s{\"name\":", *first ? "" : ",");
    print_json_string(name ? name : "");
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

static void print_theme_text(const Options *options, const Theme *theme, PlanTime bar) {
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

    char heard_form[96];
    char requested[96];
    char heard[256];
    heard_text(&theme->outcome, heard_form, sizeof(heard_form), requested, sizeof(requested), heard, sizeof(heard));
    printf("\nstandard-form rule score %lu/%lu (%lu%%)\n", theme->outcome.score.numerator,
           theme->outcome.score.denominator, percent(&theme->outcome.score));
    printf("%s\n", heard);
}

static void print_theme_json(const Options *options, const Theme *theme, PlanTime bar) {
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
        print_json_string(developments[i].idea);
        printf(",\"development\":");
        print_json_string(developments[i].how);
        printf("}");
    }
    char heard_form[96];
    char requested[96];
    char heard[256];
    heard_text(&theme->outcome, heard_form, sizeof(heard_form), requested, sizeof(requested), heard, sizeof(heard));
    printf("],\"score\":{\"numerator\":%lu,\"denominator\":%lu},\"requested\":", theme->outcome.score.numerator,
           theme->outcome.score.denominator);
    print_json_string(requested);
    printf(",\"heard\":");
    if (heard_form[0]) {
        print_json_string(heard_form);
    } else {
        printf("null");
    }
    printf(",\"heard_as_requested\":%s,\"heard_text\":", theme->outcome.heard_as_requested ? "true" : "false");
    print_json_string(heard);
    printf("}");
}

static void print_result(const Options *options, const Theme *themes, size_t count, const char *const *files,
                         size_t file_count) {
    char key[24];
    cli_key_name(&options->key, key, sizeof(key));
    char pairing[64];
    snprintf(pairing, sizeof(pairing), "%s + %s", BEGINNING_NAMES[options->pairing.beginning],
             ENDING_NAMES[options->pairing.ending]);
    PlanTime bar;
    plan_time_make(options->meter.beats_per_bar, (unsigned long)options->meter.beat_unit, &bar);
    if (options->json) {
        printf("{\"key\":\"%s\",\"meter\":\"%d/%d\",\"bars\":%zu,\"into\":\"%s\",\"pairing\":\"%s\",\"cadence\":\"%s\"",
               key, options->meter.beats_per_bar, options->meter.beat_unit, options->bars, options->into_text, pairing,
               cli_cadence_name(options->cadence));
        if (options->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT) {
            printf(",\"first_cadence\":\"%s\"", cli_cadence_name(options->first_cadence));
        }
        printf(",\"themes\":[");
        for (size_t k = 0; k < count; k++) {
            printf("%s", k ? "," : "");
            print_theme_json(options, &themes[k], bar);
        }
        printf("],\"files\":[");
        for (size_t i = 0; i < file_count; i++) {
            printf("%s", i ? "," : "");
            print_json_string(files[i]);
        }
        printf("]}\n");
        return;
    }
    printf("%s (%s), key %s, meter %d/%d, %zu bars, cadence %s", options->into_text, pairing, key,
           options->meter.beats_per_bar, options->meter.beat_unit, options->bars, cli_cadence_name(options->cadence));
    if (options->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT) {
        printf(", first cadence %s", cli_cadence_name(options->first_cadence));
    }
    printf("\n");
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

typedef struct {
    char final_path[1024];
    char temporary[1032];
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
static int write_files(const Options *options, const Theme *themes, size_t count, Pending *pending,
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
            PlanExportError error;
            if (!have_rendering) {
                if (plan_render(themes[k].plan, &rendered) != PLAN_RENDER_OK) {
                    status = cli_fail(CLI_EXIT_WRITE, "cannot render the plan");
                    break;
                }
                have_rendering = true;
            }
            PlanExportResult result = kind == 1 ? rendered_plan_export_midi(&rendered, entry->temporary, &error)
                                                : rendered_plan_export_musicxml(&rendered, entry->temporary, &error);
            if (result != PLAN_EXPORT_OK) {
                status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", entry->final_path, error.message);
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

int run_grow(int argc, char **argv) {
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
    memset(&motive_options, 0, sizeof(motive_options));
    motive_options.has_meter = true;
    motive_options.meter = options.meter;
    motive_options.has_key = true;
    motive_options.key = options.key;
    Motive motive;
    MotiveError motive_error;
    MotiveResult loaded = motive_load_file(options.input, MOTIVE_FORMAT_AUTO, &motive_options, &motive, &motive_error);
    if (loaded != MOTIVE_OK) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot load '%s' (%s)%s%s: %s", options.input,
                        motive_result_name(loaded), motive_error.location[0] ? " at " : "", motive_error.location,
                        motive_error.message);
    }

    /* The seeds are tried one after another; a stuck run is passed over, and a theme that is the same as one already
       made is not counted. A request no seed can mend ends the search at once. */
    Theme themes[MAX_SUGGESTIONS];
    memset(themes, 0, sizeof(themes));
    size_t made = 0;
    unsigned long tries = (unsigned long)CLI_MAX_SEED_TRIES * options.suggest;
    unsigned long last_seed = options.seed;
    for (unsigned long attempt = 0; attempt < tries && made < options.suggest; attempt++) {
        Theme theme;
        last_seed = (options.seed + attempt) & CLI_SEED_MAX;
        GrowResult result = grow_one(&options, &motive, last_seed, &theme);
        if (result == GROW_OK) {
            bool repeated = false;
            for (size_t k = 0; k < made; k++) {
                repeated = repeated || strcmp(themes[k].signature, theme.signature) == 0;
            }
            if (repeated) {
                theme_destroy(&theme);
            } else {
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
    if (status == 0 && made == 0) {
        status = cli_fail(CLI_EXIT_UNSATISFIABLE, "no theme could be grown for %s, %s, %zu bars, %s from seeds %lu to %lu",
                          options.key_text, options.meter_text, options.bars, options.into_text, options.seed,
                          last_seed);
    }
    if (status == 0 && made < options.suggest && options.suggest_text) {
        fprintf(stderr, "Note: only %zu different theme%s found for this request\n", made, made == 1 ? "" : "s");
    }

    if (status == 0) {
        Pending pending[MAX_FILES];
        const char *files[MAX_FILES];
        size_t pending_count = 0;
        status = write_files(&options, themes, made, pending, &pending_count);
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
    return status;
}
