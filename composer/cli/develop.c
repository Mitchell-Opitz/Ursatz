#include "develop.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "generation/candidate.h"

#include <ursatz_composer/export.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_suggest.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/plan_state.h>

#define EXIT_USAGE 1
#define EXIT_NOTHING 2
#define EXIT_WRITE 3

#define DEFAULT_SUGGEST 5
#define MAX_SUGGEST 16
#define MAX_FACTOR 16

typedef enum { OP_RESTATE, OP_INVERT, OP_RETROGRADE, OP_INTERVAL, OP_FRAGMENT, OP_LIQUIDATE, OP_AUGMENT, OP_DIMINISH } Op;

static const char *const OP_NAMES[] = {"restate",  "invert",   "retrograde", "interval",
                                       "fragment", "liquidate", "augment",    "diminish"};

/* The flags that say how an operation is done, as bits; each operation takes some of them. */
enum {
    F_BY = 1 << 0,
    F_DEGREE = 1 << 1,
    F_TONAL = 1 << 2,
    F_AXIS = 1 << 3,
    F_INTERVAL = 1 << 4,
    F_AMOUNT = 1 << 5,
    F_KEEP_CONTOUR = 1 << 6,
    F_POSITION = 1 << 7,
    F_LENGTH = 1 << 8,
    F_MIN = 1 << 9,
    F_DROP = 1 << 10,
    F_KEEP_RHYTHM = 1 << 11,
    F_RATIO = 1 << 12
};

static const unsigned OP_FLAGS[] = {
    F_BY | F_DEGREE | F_TONAL,         /* restate */
    F_TONAL | F_AXIS,                  /* invert */
    0,                                 /* retrograde */
    F_INTERVAL | F_AMOUNT | F_KEEP_CONTOUR, /* interval */
    F_POSITION | F_LENGTH | F_MIN,     /* fragment */
    F_DROP | F_KEEP_RHYTHM | F_MIN,    /* liquidate */
    F_RATIO,                           /* augment */
    F_RATIO,                           /* diminish */
};

typedef struct {
    const char *input;
    const char *op_text;
    const char *seed_text;
    const char *suggest_text;
    const char *meter_text;
    const char *key_text;
    const char *format_text;
    const char *midi_path;
    const char *musicxml_path;
    const char *by_text;
    const char *degree_text;
    const char *axis_text;
    const char *interval_text;
    const char *amount_text;
    const char *position_text;
    const char *length_text;
    const char *min_text;
    const char *drop_text;
    const char *ratio_text;
    unsigned given; /* the F_ flags that were on the command line */
    bool help;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", DEVELOP_USAGE_LINE + 7);
    fprintf(out, "Makes motives related to the one in <motive-input> (a MIDI file, a MusicXML file, a note list or a\n");
    fprintf(out, "motive JSON file) and prints them best first, each as a note list with its recognizability score.\n\n");
    fprintf(out, "  --op <name>          restate, invert, retrograde, interval, fragment, liquidate, augment, diminish\n");
    fprintf(out, "  --seed <n>           whole number, 0 to 4294967295 (default 0); the same seed gives the same output\n");
    fprintf(out, "  --suggest <n>        how many candidates to print, 1 to %d (default %d)\n", MAX_SUGGEST, DEFAULT_SUGGEST);
    fprintf(out, "  --meter <n/m>, --key <key>, --format <auto|midi|musicxml|notes|json>   as for the motive command\n");
    fprintf(out, "  --midi <file>, --musicxml <file>   write the source and the best candidate\n\n");
    fprintf(out, "Options by operation (the operation chooses at random from what is left open):\n");
    fprintf(out, "  restate     --by <n> (semitones; steps with --tonal), --degree <n> (move the first note to scale\n");
    fprintf(out, "              degree n, tonal), --tonal;  with none, the exact repetition and transpositions\n");
    fprintf(out, "  invert      --axis <pitch> (D4 or a MIDI number; the first note by default), --tonal\n");
    fprintf(out, "  retrograde  none\n");
    fprintf(out, "  interval    --interval <n> (1-based; every one by default), --amount <n> (semitones, negative to\n");
    fprintf(out, "              narrow), --keep-contour\n");
    fprintf(out, "  fragment    --position <leading|trailing|middle>, --length <n>, --min <n>\n");
    fprintf(out, "  liquidate   --drop <n>, --keep-rhythm, --min <n>\n");
    fprintf(out, "  augment     --ratio <n:m> (2:1 by default)\n");
    fprintf(out, "  diminish    --ratio <n:m> (1:2 by default)\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the input cannot be loaded or nothing can be\n");
    fprintf(out, "developed as asked; 3 an output file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer develop theme.txt --op restate --tonal --meter 4/4 --key \"D minor\" --suggest 3\n");
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

typedef struct {
    const char *flag;
    const char **slot; /* NULL for a flag with no value */
    unsigned bit;      /* the F_ flag it is, or 0 for one every operation takes */
    bool *toggle;      /* set by a flag with no value */
} FlagSpec;

static int read_options(int argc, char **argv, Options *options) {
    bool tonal = false;
    bool keep_contour = false;
    bool keep_rhythm = false;
    memset(options, 0, sizeof(*options));
    const FlagSpec specs[] = {
        {"--op", &options->op_text, 0, NULL},
        {"--seed", &options->seed_text, 0, NULL},
        {"--suggest", &options->suggest_text, 0, NULL},
        {"--meter", &options->meter_text, 0, NULL},
        {"--key", &options->key_text, 0, NULL},
        {"--format", &options->format_text, 0, NULL},
        {"--midi", &options->midi_path, 0, NULL},
        {"--musicxml", &options->musicxml_path, 0, NULL},
        {"--by", &options->by_text, F_BY, NULL},
        {"--degree", &options->degree_text, F_DEGREE, NULL},
        {"--axis", &options->axis_text, F_AXIS, NULL},
        {"--interval", &options->interval_text, F_INTERVAL, NULL},
        {"--amount", &options->amount_text, F_AMOUNT, NULL},
        {"--position", &options->position_text, F_POSITION, NULL},
        {"--length", &options->length_text, F_LENGTH, NULL},
        {"--min", &options->min_text, F_MIN, NULL},
        {"--drop", &options->drop_text, F_DROP, NULL},
        {"--ratio", &options->ratio_text, F_RATIO, NULL},
        {"--tonal", NULL, F_TONAL, &tonal},
        {"--keep-contour", NULL, F_KEEP_CONTOUR, &keep_contour},
        {"--keep-rhythm", NULL, F_KEEP_RHYTHM, &keep_rhythm},
    };
    for (int i = 2; i < argc; i++) {
        const char *argument = argv[i];
        if (strcmp(argument, "--help") == 0) {
            options->help = true;
            continue;
        }
        const FlagSpec *spec = NULL;
        for (size_t k = 0; k < sizeof(specs) / sizeof(specs[0]); k++) {
            if (strcmp(argument, specs[k].flag) == 0) {
                spec = &specs[k];
            }
        }
        if (spec) {
            options->given |= spec->bit;
            if (!spec->slot) {
                *spec->toggle = true;
            } else if (i + 1 >= argc) {
                return fail(EXIT_USAGE, "%s needs a value", argument);
            } else if (*spec->slot) {
                return fail(EXIT_USAGE, "%s given twice", argument);
            } else {
                *spec->slot = argv[++i];
            }
        } else if (argument[0] == '-' && argument[1] != '\0') {
            return fail(EXIT_USAGE, "unknown option '%s'", argument);
        } else if (!options->input) {
            options->input = argument;
        } else {
            return fail(EXIT_USAGE, "unexpected argument '%s'; the input is one file", argument);
        }
    }
    return 0;
}

static bool parse_number(const char *text, long minimum, long maximum, long *out) {
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < minimum || value > maximum) {
        return false;
    }
    *out = value;
    return true;
}

/* A pitch as a note list writes it ("D4", "Bb3") or a MIDI note number. */
static bool parse_pitch(const char *text, int *out) {
    long number;
    if (parse_number(text, 0, 127, &number)) {
        *out = (int)number;
        return true;
    }
    char note[64];
    snprintf(note, sizeof(note), "%s:q", text);
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = 4;
    options.meter.beat_unit = 4;
    Motive motive;
    if (motive_from_notes_text(note, &options, &motive, NULL) != MOTIVE_OK || motive.note_count != 1) {
        return false;
    }
    *out = motive.notes[0].midi_pitch;
    return true;
}

/* --- a motive as text ---------------------------------------------------- */

static void append(char *out, size_t size, size_t *used, const char *format, ...) {
    va_list args;
    va_start(args, format);
    if (*used < size) {
        int written = vsnprintf(out + *used, size - *used, format, args);
        *used += written > 0 ? (size_t)written : 0;
    }
    va_end(args);
}

typedef struct {
    long num;
    unsigned long den;
    const char *name;
} Value;

/* The plain and single-dotted values a note list writes, longest first. */
static const Value VALUES[] = {{1, 1, "w"},  {3, 4, "h."},  {1, 2, "h"},  {3, 8, "q."}, {1, 4, "q"},
                               {3, 16, "e."}, {1, 8, "e"}, {3, 32, "s."}, {1, 16, "s"}, {1, 32, "t"}};

/* A length as one or more tokens: "q", or "q~ q" for a note (tie true) when no one value is the length. */
static void append_length(char *out, size_t size, size_t *used, const char *pitch, PlanTime length, bool tie) {
    bool first = true;
    while (length.num > 0) {
        const Value *piece = NULL;
        for (size_t i = 0; i < sizeof(VALUES) / sizeof(VALUES[0]) && !piece; i++) {
            PlanTime value;
            plan_time_make(VALUES[i].num, VALUES[i].den, &value);
            if (plan_time_compare(value, length) <= 0) {
                piece = &VALUES[i];
            }
        }
        if (!piece) {
            append(out, size, used, "%s%s:%ld/%lu", first ? "" : "~ ", pitch, length.num, length.den);
            return;
        }
        PlanTime value;
        plan_time_make(piece->num, piece->den, &value);
        PlanTime rest;
        plan_time_make(length.num * (long)value.den - value.num * (long)length.den, length.den * value.den, &rest);
        append(out, size, used, "%s%s:%s", first ? "" : " ", pitch, piece->name);
        if (rest.num > 0 && tie) {
            append(out, size, used, "~");
        }
        length = rest;
        first = false;
    }
}

static void motive_text(const Motive *motive, char *out, size_t size) {
    size_t used = 0;
    PlanTime cursor = {0, 1};
    out[0] = '\0';
    for (size_t i = 0; i < motive->note_count; i++) {
        const PlanNote *note = &motive->notes[i];
        PlanTime gap;
        plan_time_make(note->offset.num * (long)cursor.den - cursor.num * (long)note->offset.den,
                       note->offset.den * cursor.den, &gap);
        char pitch[16];
        motive_pitch_name(note->midi_pitch, pitch, sizeof(pitch));
        if (i > 0) {
            append(out, size, &used, " ");
        }
        if (gap.num > 0) {
            append_length(out, size, &used, "r", gap, false);
            append(out, size, &used, " ");
        }
        append_length(out, size, &used, pitch, note->duration, true);
        plan_time_add(note->offset, note->duration, &cursor);
    }
}

/* --- the scene ------------------------------------------------------------ */

/* A plan that holds the source Given, then an open motive node after it that asks for the development. */
typedef struct {
    PiecePlan *plan;
    NodeId source;
    NodeId target;
} Scene;

static bool add_node(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, NodeId *out) {
    return plan_add_node(plan, parent, kind, span, NULL, out) == PLAN_OK &&
           plan_node_set_state(plan, *out, PLAN_STATE_GIVEN) == PLAN_OK;
}

static bool build_scene(const Motive *source, const PlanMotiveRef *ref, Scene *scene) {
    memset(scene, 0, sizeof(*scene));
    scene->plan = plan_create();
    if (!scene->plan) {
        return false;
    }
    /* The development begins on the barline after the source, and has room for a long augmentation. */
    PlanTime bar;
    plan_time_make(source->meter.beats_per_bar, (unsigned long)source->meter.beat_unit, &bar);
    long bars = (source->length.num * (long)bar.den + (long)source->length.den * bar.num - 1) /
                ((long)source->length.den * bar.num);
    PlanTime start;
    plan_time_make(bars * bar.num, bar.den, &start);
    PlanTime room;
    plan_time_make(source->length.num * MAX_FACTOR, source->length.den, &room);
    PlanTime total;
    plan_time_add(start, room, &total);
    PlanSpan all = {{0, 1}, total};
    PlanSpan target_span = {start, room};
    NodeId form;
    NodeId section;
    NodeId phrase;
    NodeId idea;
    bool ok = add_node(scene->plan, PLAN_NODE_NONE, PLAN_NODE_FORM, all, &form) &&
              add_node(scene->plan, form, PLAN_NODE_SECTION, all, &section) &&
              add_node(scene->plan, section, PLAN_NODE_PHRASE, all, &phrase) &&
              add_node(scene->plan, phrase, PLAN_NODE_IDEA, all, &idea) &&
              motive_attach(scene->plan, idea, (PlanTime){0, 1}, source, "source", &scene->source, NULL) == PLAN_OK &&
              plan_add_node(scene->plan, idea, PLAN_NODE_MOTIVE, target_span, "development", &scene->target) == PLAN_OK;
    /* The meter and the key, so that a file written from the plan has them. */
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.meter = source->meter;
    LaneSpanId lane;
    ok = ok && plan_add_lane_span(scene->plan, idea, PLAN_LANE_METER, 0, all, &payload, &lane) == PLAN_OK &&
         plan_lane_span_set_state(scene->plan, lane, PLAN_STATE_GIVEN) == PLAN_OK;
    if (ok && source->key_source != MOTIVE_KEY_UNSPECIFIED) {
        payload.key = source->key;
        ok = plan_add_lane_span(scene->plan, idea, PLAN_LANE_KEY, 0, all, &payload, &lane) == PLAN_OK &&
             plan_lane_span_set_state(scene->plan, lane, PLAN_STATE_GIVEN) == PLAN_OK;
    }
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    constraint.params.motive = *ref;
    constraint.params.motive.motive = scene->source;
    ok = ok && plan_node_add_constraint(scene->plan, scene->target, &constraint) == PLAN_OK;
    if (!ok) {
        plan_destroy(scene->plan);
        scene->plan = NULL;
    }
    return ok;
}

/* --- what the options ask for ---------------------------------------------- */

/* The motive_ref constraint the operation and its options describe, or a message in *problem. */
static bool build_ref(const Options *options, Op op, const Motive *source, size_t *min_length, PlanMotiveRef *ref,
                      const char **problem, char *buffer, size_t size) {
    static const PlanMotiveTransform TRANSFORMS[] = {PLAN_TRANSFORM_RESTATED,   PLAN_TRANSFORM_INVERTED,
                                                     PLAN_TRANSFORM_RETROGRADE, PLAN_TRANSFORM_INTERVAL_CHANGED,
                                                     PLAN_TRANSFORM_FRAGMENTED, PLAN_TRANSFORM_LIQUIDATED,
                                                     PLAN_TRANSFORM_AUGMENTED,  PLAN_TRANSFORM_DIMINISHED};
    long value;
    memset(ref, 0, sizeof(*ref));
    ref->transform = TRANSFORMS[op];
    ref->tonal = (options->given & F_TONAL) != 0;
    ref->keep_contour = (options->given & F_KEEP_CONTOUR) != 0;
    ref->keep_rhythm = (options->given & F_KEEP_RHYTHM) != 0;
    *problem = buffer;

    if (options->by_text) {
        if (!parse_number(options->by_text, -127, 127, &value) || value == 0) {
            snprintf(buffer, size, "--by must be a whole number from -127 to 127, not 0 or '%s'", options->by_text);
            return false;
        }
        ref->transform = PLAN_TRANSFORM_TRANSPOSED;
        if (ref->tonal) {
            ref->steps = (int)value;
        } else {
            ref->semitones = (int)value;
        }
    }
    if (options->degree_text) {
        MotiveAnalysis analysis;
        if (!parse_number(options->degree_text, 1, 7, &value)) {
            snprintf(buffer, size, "--degree must be a scale degree from 1 to 7, not '%s'", options->degree_text);
            return false;
        }
        if (options->by_text) {
            snprintf(buffer, size, "--by and --degree both say how far to move; give one");
            return false;
        }
        if (source->key_source == MOTIVE_KEY_UNSPECIFIED || !motive_analyze(source, &analysis)) {
            snprintf(buffer, size, "--degree needs a key; give --key");
            return false;
        }
        /* The nearest way to the degree, up to three steps either way. */
        int steps = (int)((value - analysis.degrees[0].degree + 7) % 7);
        ref->transform = PLAN_TRANSFORM_TRANSPOSED;
        ref->tonal = true;
        ref->steps = steps > 3 ? steps - 7 : steps;
        if (ref->steps == 0) {
            snprintf(buffer, size, "the first note is already on degree %ld", value);
            return false;
        }
    }
    if (ref->tonal && source->key_source == MOTIVE_KEY_UNSPECIFIED) {
        snprintf(buffer, size, "--tonal needs a key; give --key");
        return false;
    }
    if (options->axis_text) {
        int axis;
        if (!parse_pitch(options->axis_text, &axis)) {
            snprintf(buffer, size, "--axis must be a pitch such as D4 or a MIDI number, not '%s'", options->axis_text);
            return false;
        }
        ref->has_axis = true;
        ref->axis = axis;
    }
    if (options->interval_text) {
        if (!parse_number(options->interval_text, 1, 127, &value)) {
            snprintf(buffer, size, "--interval must be a whole number from 1, not '%s'", options->interval_text);
            return false;
        }
        ref->interval = (int)value;
    }
    if (options->amount_text) {
        if (!parse_number(options->amount_text, -127, 127, &value) || value == 0) {
            snprintf(buffer, size, "--amount must be a whole number of semitones, not 0 or '%s'", options->amount_text);
            return false;
        }
        ref->amount = (int)value;
    }
    if (options->position_text) {
        static const char *const NAMES[] = {"any", "leading", "trailing", "middle"};
        int found = 0;
        for (int i = 1; i < 4; i++) {
            if (strcmp(options->position_text, NAMES[i]) == 0) {
                found = i;
            }
        }
        if (found == 0) {
            snprintf(buffer, size, "--position must be leading, trailing or middle, not '%s'", options->position_text);
            return false;
        }
        ref->position = (PlanFragmentPosition)found;
    }
    if (options->length_text) {
        if (!parse_number(options->length_text, 1, MOTIVE_MAX_NOTES, &value)) {
            snprintf(buffer, size, "--length must be a number of notes, not '%s'", options->length_text);
            return false;
        }
        ref->length = (int)value;
    }
    if (options->drop_text) {
        if (!parse_number(options->drop_text, 1, MOTIVE_MAX_NOTES, &value)) {
            snprintf(buffer, size, "--drop must be a number of notes, not '%s'", options->drop_text);
            return false;
        }
        ref->drop = (int)value;
    }
    if (options->min_text) {
        if (!parse_number(options->min_text, 1, MOTIVE_MAX_NOTES, &value)) {
            snprintf(buffer, size, "--min must be a number of notes, not '%s'", options->min_text);
            return false;
        }
        *min_length = (size_t)value;
    }
    if (options->ratio_text) {
        long num;
        long den;
        char *colon = NULL;
        num = strtol(options->ratio_text, &colon, 10);
        if (colon == options->ratio_text || *colon != ':' || !parse_number(colon + 1, 1, MAX_FACTOR, &den) || num < 1 ||
            num > MAX_FACTOR) {
            snprintf(buffer, size, "--ratio must be n:m with each from 1 to %d, as in 3:2, not '%s'", MAX_FACTOR,
                     options->ratio_text);
            return false;
        }
        ref->ratio_num = (int)num;
        ref->ratio_den = (int)den;
    }
    return true;
}

/* --- running ---------------------------------------------------------------- */

static bool parse_op(const char *text, Op *out) {
    for (int i = 0; i < 8; i++) {
        if (strcmp(text, OP_NAMES[i]) == 0) {
            *out = (Op)i;
            return true;
        }
    }
    return false;
}

static int write_files(const Options *options, Scene *scene, const FillSuggestions *suggestions) {
    if (!options->midi_path && !options->musicxml_path) {
        return 0;
    }
    FillResult applied = fill_apply_suggestion(scene->plan, suggestions, 0);
    if (applied != FILL_OK) {
        return fail(EXIT_WRITE, "cannot build the plan to write (fill result %d)", (int)applied);
    }
    PlanExportError error;
    if (options->midi_path) {
        if (plan_export_midi(scene->plan, options->midi_path, &error) != PLAN_EXPORT_OK) {
            return fail(EXIT_WRITE, "MIDI export failed: %s", error.message);
        }
        printf("MIDI written to %s\n", options->midi_path);
    }
    if (options->musicxml_path) {
        if (plan_export_musicxml(scene->plan, options->musicxml_path, &error) != PLAN_EXPORT_OK) {
            return fail(EXIT_WRITE, "MusicXML export failed: %s", error.message);
        }
        printf("MusicXML written to %s\n", options->musicxml_path);
    }
    return 0;
}

/* The notes written on the target once suggestion index is applied to a copy of the plan. */
static bool suggestion_motive(const Scene *scene, const FillSuggestions *suggestions, size_t index,
                              const Motive *source, Motive *out) {
    char *json = plan_to_json(scene->plan, NULL);
    PiecePlan *copy = NULL;
    bool ok = json && plan_from_json(json, &copy, NULL) == PLAN_OK && fill_apply_suggestion(copy, suggestions, index) == FILL_OK;
    LaneSpanId ids[4];
    if (ok && plan_node_lane_spans(copy, scene->target, ids, 4) == 1) {
        const PlanLanePayload *payload = plan_lane_span_payload(copy, ids[0]);
        memset(out, 0, sizeof(*out));
        *out = *source;
        out->note_count = payload->notes.note_count;
        memcpy(out->notes, payload->notes.notes, out->note_count * sizeof(PlanNote));
        out->length = plan_lane_span_span(copy, ids[0]).length;
    } else {
        ok = false;
    }
    free(json);
    plan_destroy(copy);
    return ok;
}

int run_develop(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    if (!options.input) {
        return fail(EXIT_USAGE, "develop needs an input file\n%s", DEVELOP_USAGE_LINE);
    }
    if (!options.op_text) {
        return fail(EXIT_USAGE, "develop needs --op: restate, invert, retrograde, interval, fragment, liquidate, "
                                "augment or diminish");
    }
    Op op;
    if (!parse_op(options.op_text, &op)) {
        return fail(EXIT_USAGE, "unknown operation '%s'; use restate, invert, retrograde, interval, fragment, "
                                "liquidate, augment or diminish", options.op_text);
    }
    unsigned stray = options.given & ~OP_FLAGS[op];
    if (stray) {
        static const char *const FLAGS[] = {"--by", "--degree", "--tonal", "--axis", "--interval", "--amount",
                                            "--keep-contour", "--position", "--length", "--min", "--drop",
                                            "--keep-rhythm", "--ratio"};
        for (int bit = 0; bit < 13; bit++) {
            if (stray & (1u << bit)) {
                return fail(EXIT_USAGE, "%s does not apply to %s", FLAGS[bit], OP_NAMES[op]);
            }
        }
    }

    long seed = 0;
    long suggest = DEFAULT_SUGGEST;
    if (options.seed_text && !parse_number(options.seed_text, 0, 4294967295L, &seed)) {
        return fail(EXIT_USAGE, "--seed must be a whole number from 0 to 4294967295, not '%s'", options.seed_text);
    }
    if (options.suggest_text && !parse_number(options.suggest_text, 1, MAX_SUGGEST, &suggest)) {
        return fail(EXIT_USAGE, "--suggest must be a whole number from 1 to %d, not '%s'", MAX_SUGGEST,
                    options.suggest_text);
    }
    MotiveOptions motive_options;
    memset(&motive_options, 0, sizeof(motive_options));
    if (options.meter_text) {
        if (!motive_parse_meter(options.meter_text, &motive_options.meter)) {
            return fail(EXIT_USAGE, "meter '%s' is not a meter; write beats over a beat unit, as in 4/4 or 6/8",
                        options.meter_text);
        }
        motive_options.has_meter = true;
    }
    if (options.key_text) {
        if (!motive_parse_key(options.key_text, &motive_options.key)) {
            return fail(EXIT_USAGE,
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                        "for example \"D minor\"",
                        options.key_text);
        }
        motive_options.has_key = true;
    }
    MotiveFormat format = MOTIVE_FORMAT_AUTO;
    if (options.format_text && !motive_format_parse(options.format_text, &format)) {
        return fail(EXIT_USAGE, "--format must be auto, midi, musicxml, notes or json, not '%s'", options.format_text);
    }

    Motive source;
    MotiveError error;
    MotiveResult loaded = motive_load_file(options.input, format, &motive_options, &source, &error);
    if (loaded != MOTIVE_OK) {
        return fail(EXIT_NOTHING, "cannot load '%s' (%s)%s%s: %s", options.input, motive_result_name(loaded),
                    error.location[0] ? " at " : "", error.location, error.message);
    }

    MotiveDevelopContext context;
    motive_develop_context_default(&context);
    context.meter = source.meter;
    context.has_key = source.key_source != MOTIVE_KEY_UNSPECIFIED;
    context.key = source.key;
    context.candidates = (size_t)suggest * 3 > 12 ? (size_t)suggest * 3 : 12;
    PlanMotiveRef ref;
    char problem_text[160];
    const char *problem = NULL;
    if (!build_ref(&options, op, &source, &context.min_length, &ref, &problem, problem_text, sizeof(problem_text))) {
        return fail(EXIT_USAGE, "%s", problem);
    }

    Scene scene;
    FillOperatorRegistry *registry = fill_registry_create();
    if (!registry || motive_develop_register(registry, &context) != FILL_OK || !build_scene(&source, &ref, &scene)) {
        fill_registry_destroy(registry);
        return fail(EXIT_NOTHING, "out of memory");
    }
    FillConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = (unsigned long)seed;
    config.checker = motive_develop_checker;
    config.checker_context = &context;
    FillTarget target = {FILL_TARGET_NODE, scene.target};
    FillSuggestions *suggestions = NULL;
    if (fill_suggest(scene.plan, registry, &config, target, (size_t)suggest, &suggestions) != FILL_OK) {
        fill_registry_destroy(registry);
        plan_destroy(scene.plan);
        return fail(EXIT_NOTHING, "the development could not be run");
    }

    size_t count = fill_suggestions_count(suggestions);
    char text[2048];
    motive_text(&source, text, sizeof(text));
    int exit_status = 0;
    if (count == 0) {
        exit_status = fail(EXIT_NOTHING, "no %s development can be made from %s as asked", OP_NAMES[op], text);
    } else {
        printf("Source: %s\n", text);
        printf("Operation: %s (seed %ld)\n", OP_NAMES[op], seed);
        const CandidateSet *set = fill_suggestions_set(suggestions);
        for (size_t i = 0; i < count; i++) {
            const Candidate *candidate = candidate_set_candidate_at(set, i);
            Motive development;
            if (!suggestion_motive(&scene, suggestions, i, &source, &development)) {
                exit_status = fail(EXIT_NOTHING, "cannot read candidate %zu", i + 1);
                break;
            }
            motive_text(&development, text, sizeof(text));
            unsigned long num = candidate_score_numerator(candidate);
            unsigned long den = candidate_score_denominator(candidate);
            printf("%zu. %s  score %lu/%lu (%.0f%%)\n   %s\n", i + 1, fill_suggestion_label(suggestions, i), num, den,
                   100.0 * (double)num / (double)den, text);
        }
        if (exit_status == 0) {
            exit_status = write_files(&options, &scene, suggestions);
        }
    }
    fill_suggestions_destroy(suggestions);
    fill_registry_destroy(registry);
    plan_destroy(scene.plan);
    return exit_status;
}
