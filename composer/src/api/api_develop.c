#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <generation/candidate.h>

#include <ursatz_composer/export.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_suggest.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_develop.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/render.h>

#include "api_workflow.h"
#include "motive/motive_internal.h"
#include "util/plan_util.h"

/* `develop` as a composer API entry (docs/cli-develop.md): motives related to one motive, best first. */

#define DEFAULT_SUGGEST 5
#define MAX_SUGGEST 16
#define MAX_FACTOR 16

typedef enum { OP_RESTATE, OP_INVERT, OP_RETROGRADE, OP_INTERVAL, OP_FRAGMENT, OP_LIQUIDATE, OP_AUGMENT, OP_DIMINISH } Op;

static const char *const OP_NAMES[] = {"restate", "invert", "retrograde", "interval", "fragment", "liquidate", "augment", "diminish"};

/* The members that say how an operation is done, as bits; each operation takes some of them. */
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
    F_BY | F_DEGREE | F_TONAL,              /* restate */
    F_TONAL | F_AXIS,                       /* invert */
    0,                                      /* retrograde */
    F_INTERVAL | F_AMOUNT | F_KEEP_CONTOUR, /* interval */
    F_POSITION | F_LENGTH | F_MIN,          /* fragment */
    F_DROP | F_KEEP_RHYTHM | F_MIN,         /* liquidate */
    F_RATIO,                                /* augment */
    F_RATIO,                                /* diminish */
};

/* The member names in the order of the bits above. */
static const char *const FLAG_NAMES[] = {"by",     "degree", "tonal", "axis",         "interval",     "amount", "keep_contour",
                                         "position", "length", "min",   "drop", "keep_rhythm", "ratio"};

/* --- the numbers a request gives ------------------------------------------------------------------------- */

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
    *out = motive_pitch(&motive, 0);
    return true;
}

/* A member given as text or as a number, as the text; NULL when absent or null. */
static bool member_text(const cJSON *request, const char *name, char *buffer, size_t size, const char **out,
                        ComposerError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(request, name);
    *out = NULL;
    if (!item || cJSON_IsNull(item)) {
        return true;
    }
    if (cJSON_IsString(item)) {
        *out = item->valuestring;
        return true;
    }
    if (cJSON_IsNumber(item)) {
        snprintf(buffer, size, "%g", item->valuedouble);
        *out = buffer;
        return true;
    }
    char path[sizeof error->path];
    snprintf(path, sizeof path, "/%s", name);
    api_fail(error, COMPOSER_ERR_REQUEST, path, "%s must be a number or text", name);
    return false;
}

/* --- a motive as text ------------------------------------------------------------------------------------- */

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
} LengthValue;

/* The plain and single-dotted values a note list writes, longest first. */
static const LengthValue VALUES[] = {{1, 1, "w"},  {3, 4, "h."},  {1, 2, "h"},  {3, 8, "q."}, {1, 4, "q"},
                                     {3, 16, "e."}, {1, 8, "e"}, {3, 32, "s."}, {1, 16, "s"}, {1, 32, "t"}};

/* The triplet values (two thirds of w to t) a note list writes for a length whose denominator is divisible by 3. */
static const LengthValue TRIPLET_VALUES[] = {{2, 3, "w3"}, {1, 3, "h3"},  {1, 6, "q3"},
                                             {1, 12, "e3"}, {1, 24, "s3"}, {1, 48, "t3"}};

/* A length as one or more tokens: "q", or "q~ q" for a note (tie true) when no one value is the length. */
static void append_length(char *out, size_t size, size_t *used, const char *pitch, Rational length, bool tie) {
    bool first = true;
    while (length.num > 0) {
        const LengthValue *table = length.den % 3 == 0 ? TRIPLET_VALUES : VALUES;
        size_t table_size = length.den % 3 == 0 ? sizeof(TRIPLET_VALUES) / sizeof(TRIPLET_VALUES[0])
                                                : sizeof(VALUES) / sizeof(VALUES[0]);
        const LengthValue *piece = NULL;
        for (size_t i = 0; i < table_size && !piece; i++) {
            Rational value;
            rational_make(table[i].num, table[i].den, &value);
            if (rational_cmp(value, length) <= 0) {
                piece = &table[i];
            }
        }
        if (!piece) {
            append(out, size, used, "%s%s:%ld/%lu", first ? "" : "~ ", pitch, length.num, length.den);
            return;
        }
        Rational value;
        rational_make(piece->num, piece->den, &value);
        Rational rest;
        rational_sub(length, value, &rest);
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
    Rational cursor = {0, 1};
    out[0] = '\0';
    for (size_t i = 0; i < motive->note_count; i++) {
        const MotiveSketchNote *note = &motive->notes[i];
        Rational gap;
        rational_sub(note->onset, cursor, &gap);
        char pitch[16];
        motive_pitch_name(motive_pitch(motive, i), pitch, sizeof(pitch));
        if (i > 0) {
            append(out, size, &used, " ");
        }
        if (gap.num > 0) {
            append_length(out, size, &used, "r", gap, false);
            append(out, size, &used, " ");
        }
        append_length(out, size, &used, pitch, note->duration, true);
        rational_add(note->onset, note->duration, &cursor);
    }
}

/* --- the scene -------------------------------------------------------------------------------------------- */

/* A plan that holds the source Given, then an open motive node after it that asks for the development. */
typedef struct {
    PiecePlan *plan;
    NodeId source;
    NodeId target;
} Scene;

static bool build_scene(const Motive *source, const PlanMotiveRef *ref, Scene *scene) {
    memset(scene, 0, sizeof(*scene));
    scene->plan = plan_create();
    if (!scene->plan) {
        return false;
    }
    /* The development begins on the barline after the source, and has room for a long augmentation. */
    Rational bar;
    rational_make(motive_meter(source).beats_per_bar, (long long)motive_meter(source).beat_unit, &bar);
    long bars = (source->length.num * (long)bar.den + (long)source->length.den * bar.num - 1) /
                ((long)source->length.den * bar.num);
    Rational start;
    rational_make(bars * bar.num, bar.den, &start);
    Rational room;
    rational_mul_int(source->length, (long long)MAX_FACTOR, &room);
    Rational total;
    rational_add(start, room, &total);
    PlanSpan all = {{0, 1}, total};
    PlanSpan target_span = {start, room};
    NodeId form;
    NodeId section;
    NodeId phrase;
    NodeId idea;
    bool ok = plan_add_given_node(scene->plan, PLAN_NODE_NONE, PLAN_NODE_FORM, all, &form) &&
              plan_add_given_node(scene->plan, form, PLAN_NODE_SECTION, all, &section) &&
              plan_add_given_node(scene->plan, section, PLAN_NODE_PHRASE, all, &phrase) &&
              plan_add_given_node(scene->plan, phrase, PLAN_NODE_IDEA, all, &idea) &&
              motive_attach(scene->plan, idea, (Rational){0, 1}, source, "source", &scene->source, NULL) == PLAN_OK &&
              plan_add_node(scene->plan, idea, PLAN_NODE_MOTIVE, target_span, "development", &scene->target) == PLAN_OK;
    /* The meter and the key, so that a file written from the plan has them. */
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.meter = motive_meter(source);
    LaneSpanId lane;
    ok = ok && plan_add_lane_span(scene->plan, idea, PLAN_LANE_METER, 0, all, &payload, &lane) == PLAN_OK &&
         plan_lane_span_set_state(scene->plan, lane, PLAN_STATE_GIVEN) == PLAN_OK;
    if (ok && source->key_source != MOTIVE_SKETCH_KEY_UNSPECIFIED) {
        payload.key = motive_key(source);
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

/* --- what the request asks for ---------------------------------------------------------------------------- */

typedef struct {
    const char *by;
    const char *degree;
    const char *axis;
    const char *interval;
    const char *amount;
    const char *position;
    const char *length;
    const char *min;
    const char *drop;
    const char *ratio;
    unsigned given; /* the F_ bits of the members that were given */
} Flags;

/* The motive_ref constraint the operation and its members describe, or a message in *problem. */
static bool build_ref(const Flags *flags, Op op, const Motive *source, size_t *min_length, PlanMotiveRef *ref,
                      const char **problem, char *buffer, size_t size) {
    static const MotiveRelationKind KINDS[] = {MOTIVE_RELATION_EXACT,   MOTIVE_RELATION_INVERT,   MOTIVE_RELATION_RETROGRADE,
                                               MOTIVE_RELATION_INTERVAL_CHANGE, MOTIVE_RELATION_FRAGMENT,
                                               MOTIVE_RELATION_LIQUIDATE, MOTIVE_RELATION_AUGMENT, MOTIVE_RELATION_DIMINISH};
    long value;
    /* Every member is read whatever the operation; the step takes the ones its relation has. */
    MotiveRelationKind kind = KINDS[op];
    bool tonal = (flags->given & F_TONAL) != 0;
    bool keep_contour = (flags->given & F_KEEP_CONTOUR) != 0;
    bool keep_rhythm = (flags->given & F_KEEP_RHYTHM) != 0;
    long by = 0;
    long axis = -1;
    long interval = 0;
    long amount = 0;
    long position = 0;
    long length = 0;
    long drop = 0;
    long ratio_num = 0;
    long ratio_den = 0;
    memset(ref, 0, sizeof(*ref));
    ref->has_step = true;
    *problem = buffer;

    if (flags->by) {
        if (!parse_number(flags->by, -127, 127, &value) || value == 0) {
            snprintf(buffer, size, "by must be a whole number from -127 to 127, not 0 or '%s'", flags->by);
            return false;
        }
        kind = MOTIVE_RELATION_TRANSPOSE;
        by = value;
    }
    if (flags->degree) {
        MotiveAnalysis analysis;
        if (!parse_number(flags->degree, 1, 7, &value)) {
            snprintf(buffer, size, "degree must be a scale degree from 1 to 7, not '%s'", flags->degree);
            return false;
        }
        if (flags->by) {
            snprintf(buffer, size, "by and degree both say how far to move; give one");
            return false;
        }
        if (source->key_source == MOTIVE_SKETCH_KEY_UNSPECIFIED || !motive_analyze(source, &analysis)) {
            snprintf(buffer, size, "degree needs a key; give key");
            return false;
        }
        /* The nearest way to the degree, up to three steps either way. */
        int steps = (int)((value - analysis.degrees[0].degree + 7) % 7);
        kind = MOTIVE_RELATION_TRANSPOSE;
        tonal = true;
        by = steps > 3 ? steps - 7 : steps;
        if (by == 0) {
            snprintf(buffer, size, "the first note is already on degree %ld", value);
            return false;
        }
    }
    if (tonal && source->key_source == MOTIVE_SKETCH_KEY_UNSPECIFIED) {
        snprintf(buffer, size, "tonal needs a key; give key");
        return false;
    }
    if (flags->axis) {
        int pitch;
        if (!parse_pitch(flags->axis, &pitch)) {
            snprintf(buffer, size, "axis must be a pitch such as D4 or a MIDI number, not '%s'", flags->axis);
            return false;
        }
        axis = pitch;
    }
    if (flags->interval) {
        if (!parse_number(flags->interval, 1, 127, &value)) {
            snprintf(buffer, size, "interval must be a whole number from 1, not '%s'", flags->interval);
            return false;
        }
        interval = value;
    }
    if (flags->amount) {
        if (!parse_number(flags->amount, -127, 127, &value) || value == 0) {
            snprintf(buffer, size, "amount must be a whole number of semitones, not 0 or '%s'", flags->amount);
            return false;
        }
        amount = value;
    }
    if (flags->position) {
        int found = 0;
        for (int i = 1; i < 4; i++) {
            if (strcmp(flags->position, plan_fragment_position_names[i]) == 0) {
                found = i;
            }
        }
        if (found == 0) {
            snprintf(buffer, size, "position must be leading, trailing or middle, not '%s'", flags->position);
            return false;
        }
        position = found;
    }
    if (flags->length) {
        if (!parse_number(flags->length, 1, MOTIVE_MAX_NOTES, &value)) {
            snprintf(buffer, size, "length must be a number of notes, not '%s'", flags->length);
            return false;
        }
        length = value;
    }
    if (flags->drop) {
        if (!parse_number(flags->drop, 1, MOTIVE_MAX_NOTES, &value)) {
            snprintf(buffer, size, "drop must be a number of notes, not '%s'", flags->drop);
            return false;
        }
        drop = value;
    }
    if (flags->min) {
        if (!parse_number(flags->min, 1, MOTIVE_MAX_NOTES, &value)) {
            snprintf(buffer, size, "min must be a number of notes, not '%s'", flags->min);
            return false;
        }
        *min_length = (size_t)value;
    }
    if (flags->ratio) {
        long num;
        long den;
        char *colon = NULL;
        num = strtol(flags->ratio, &colon, 10);
        if (colon == flags->ratio || *colon != ':' || !parse_number(colon + 1, 1, MAX_FACTOR, &den) || num < 1 || num > MAX_FACTOR) {
            snprintf(buffer, size, "ratio must be n:m with each from 1 to %d, as in 3:2, not '%s'", MAX_FACTOR, flags->ratio);
            return false;
        }
        ratio_num = num;
        ratio_den = den;
    }
    MotiveRelationStep *step = &ref->step;
    step->kind = kind;
    switch (kind) {
    case MOTIVE_RELATION_TRANSPOSE:
        step->value[0] = tonal ? 1 : 0;
        step->value[1] = by;
        break;
    case MOTIVE_RELATION_INVERT:
        step->value[0] = axis;
        step->value[1] = tonal ? 1 : 0;
        break;
    case MOTIVE_RELATION_INTERVAL_CHANGE:
        step->value[0] = interval;
        step->value[1] = amount;
        step->value[2] = keep_contour ? 1 : 0;
        break;
    case MOTIVE_RELATION_FRAGMENT:
        step->value[0] = position;
        step->value[1] = length;
        break;
    case MOTIVE_RELATION_LIQUIDATE:
        step->value[0] = drop;
        step->value[1] = keep_rhythm ? 1 : 0;
        break;
    case MOTIVE_RELATION_AUGMENT:
    case MOTIVE_RELATION_DIMINISH:
        step->value[0] = ratio_num;
        step->den[0] = ratio_den;
        break;
    default:
        break;
    }
    return true;
}

static bool parse_op(const char *text, Op *out) {
    for (int i = 0; i < 8; i++) {
        if (strcmp(text, OP_NAMES[i]) == 0) {
            *out = (Op)i;
            return true;
        }
    }
    return false;
}

/* Reads the members that say how the operation is done, and which of them were given. */
static ComposerStatus read_flags(const cJSON *request, Flags *flags, char (*buffers)[32], ComposerError *error) {
    const char **texts[] = {&flags->by, &flags->degree, NULL, &flags->axis, &flags->interval, &flags->amount, NULL,
                            &flags->position, &flags->length, &flags->min, &flags->drop, NULL, &flags->ratio};
    memset(flags, 0, sizeof(*flags));
    for (int bit = 0; bit < 13; bit++) {
        if (texts[bit]) {
            if (!member_text(request, FLAG_NAMES[bit], buffers[bit], 32, texts[bit], error)) {
                return COMPOSER_ERR_REQUEST;
            }
            if (*texts[bit]) {
                flags->given |= 1u << bit;
            }
        } else {
            bool value = false;
            bool found;
            if (!api_member_bool(request, FLAG_NAMES[bit], &value, &found, error)) {
                return COMPOSER_ERR_REQUEST;
            }
            if (value) {
                flags->given |= 1u << bit;
            }
        }
    }
    return COMPOSER_OK;
}

/* The notes written on the target once suggestion index is applied to a copy of the plan. */
static bool suggestion_motive(const Scene *scene, const FillSuggestions *suggestions, size_t index, const Motive *source,
                              Motive *out) {
    char *json = plan_to_json(scene->plan, NULL);
    PiecePlan *copy = NULL;
    bool ok = json && plan_from_json(json, &copy, NULL) == PLAN_OK && fill_apply_suggestion(copy, suggestions, index) == FILL_OK;
    LaneSpanId ids[4];
    if (ok && plan_node_lane_spans(copy, scene->target, ids, 4) == 1) {
        const PlanLanePayload *payload = plan_lane_span_payload(copy, ids[0]);
        memset(out, 0, sizeof(*out));
        *out = *source;
        motive_set_lane_notes(out, payload->notes.notes, payload->notes.note_count);
        out->length = plan_lane_span_span(copy, ids[0]).length;
    } else {
        ok = false;
    }
    free(json);
    plan_destroy(copy);
    return ok;
}

/* The files of the best candidate: the source and that candidate applied to the plan. */
static ComposerStatus render_best(Scene *scene, const FillSuggestions *suggestions, const ApiOutputs *outputs,
                                  ComposerResponse *out) {
    FillResult applied = fill_apply_suggestion(scene->plan, suggestions, 0);
    if (applied != FILL_OK) {
        return api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "cannot build the plan to write (fill result %d)", (int)applied);
    }
    return api_render_outputs(scene->plan, outputs, out);
}

/* --- the entry -------------------------------------------------------------------------------------------- */

ComposerStatus composer_api_develop(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *request = NULL;
    Flags flags;
    char buffers[13][32];
    ApiOutputs outputs;
    Motive source;
    Scene scene;
    FillOperatorRegistry *registry = NULL;
    FillSuggestions *suggestions = NULL;
    bool have_scene = false;
    const char *op_text = NULL;
    unsigned long seed = 0;
    unsigned long suggest = DEFAULT_SUGGEST;
    bool found;
    Op op = OP_RESTATE;
    memset(&outputs, 0, sizeof(outputs));
    ComposerStatus status = api_parse_request(request_json, &request, &out->error);
    if (status == COMPOSER_OK && !api_member_string(request, "op", &op_text, &found, &out->error)) {
        status = COMPOSER_ERR_REQUEST;
    } else if (status == COMPOSER_OK && !found) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/op", "the request needs \"op\"");
    }
    if (status == COMPOSER_OK && !parse_op(op_text, &op)) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/op",
                          "unknown operation '%s'; use restate, invert, retrograde, interval, fragment, liquidate, "
                          "augment or diminish",
                          op_text);
    }
    if (status == COMPOSER_OK) {
        status = read_flags(request, &flags, buffers, &out->error);
    }
    if (status == COMPOSER_OK && (flags.given & ~OP_FLAGS[op])) {
        for (int bit = 0; bit < 13 && status == COMPOSER_OK; bit++) {
            if (flags.given & ~OP_FLAGS[op] & (1u << bit)) {
                char path[64];
                snprintf(path, sizeof path, "/%s", FLAG_NAMES[bit]);
                status = api_fail(&out->error, COMPOSER_ERR_REQUEST, path, "%s does not apply to %s", FLAG_NAMES[bit], OP_NAMES[op]);
            }
        }
    }
    if (status == COMPOSER_OK &&
        (!api_member_whole(request, "seed", 0, 4294967295UL, &seed, &found, &out->error) ||
         !api_member_whole(request, "suggest", 1, MAX_SUGGEST, &suggest, &found, &out->error))) {
        status = COMPOSER_ERR_REQUEST;
    }
    if (status == COMPOSER_OK && !found) {
        suggest = DEFAULT_SUGGEST;
    }
    if (status == COMPOSER_OK) {
        status = api_read_outputs(request, false, &outputs, &out->error);
    }
    if (status == COMPOSER_OK) {
        status = api_read_motive(request, "", &source, NULL, &out->error);
    }
    MotiveDevelopContext context;
    PlanMotiveRef ref;
    if (status == COMPOSER_OK) {
        char problem_text[160];
        const char *problem = NULL;
        motive_develop_context_default(&context);
        context.meter = motive_meter(&source);
        context.has_key = source.key_source != MOTIVE_SKETCH_KEY_UNSPECIFIED;
        context.key = motive_key(&source);
        context.candidates = (size_t)suggest * 3 > 12 ? (size_t)suggest * 3 : 12;
        if (!build_ref(&flags, op, &source, &context.min_length, &ref, &problem, problem_text, sizeof(problem_text))) {
            status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "", "%s", problem);
        }
    }
    if (status == COMPOSER_OK) {
        registry = fill_registry_create();
        if (!registry || motive_develop_register(registry, &context) != FILL_OK) {
            status = api_fail_alloc(&out->error);
        } else if (!build_scene(&source, &ref, &scene)) {
            status = api_fail_alloc(&out->error);
        } else {
            have_scene = true;
        }
    }
    if (status == COMPOSER_OK) {
        FillConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        config.checker = motive_develop_checker;
        config.checker_context = &context;
        config.job = job;
        FillTarget target = {FILL_TARGET_NODE, scene.target};
        if (job_step(job, "develop", 0, 1)) {
            status = api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
        } else if (fill_suggest(scene.plan, registry, &config, target, (size_t)suggest, &suggestions) != FILL_OK) {
            status = job && job->cancelled && job->cancelled(job->user)
                         ? api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled")
                         : api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "the development could not be run");
        }
    }
    if (status == COMPOSER_OK) {
        size_t count = fill_suggestions_count(suggestions);
        char text[2048];
        motive_text(&source, text, sizeof(text));
        if (count == 0) {
            status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "no %s development can be made from %s as asked",
                              OP_NAMES[op], text);
        } else {
            cJSON *document = cJSON_CreateObject();
            cJSON *list = document ? cJSON_CreateArray() : NULL;
            bool ok = list && cJSON_AddStringToObject(document, "source", text) &&
                      cJSON_AddStringToObject(document, "operation", OP_NAMES[op]) &&
                      cJSON_AddNumberToObject(document, "seed", (double)seed) && cJSON_AddItemToObject(document, "candidates", list);
            if (!ok) {
                cJSON_Delete(list);
                status = api_fail_alloc(&out->error);
            }
            const CandidateSet *set = status == COMPOSER_OK ? fill_suggestions_set(suggestions) : NULL;
            for (size_t i = 0; status == COMPOSER_OK && i < count; i++) {
                const Candidate *candidate = candidate_set_candidate_at(set, i);
                Motive development;
                if (!suggestion_motive(&scene, suggestions, i, &source, &development)) {
                    status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "cannot read candidate %zu", i + 1);
                    break;
                }
                motive_text(&development, text, sizeof(text));
                cJSON *entry = cJSON_CreateObject();
                ok = entry && cJSON_AddNumberToObject(entry, "index", (double)(i + 1)) &&
                     cJSON_AddStringToObject(entry, "label", fill_suggestion_label(suggestions, i)) &&
                     cJSON_AddNumberToObject(entry, "score_numerator", (double)candidate_score_numerator(candidate)) &&
                     cJSON_AddNumberToObject(entry, "score_denominator", (double)candidate_score_denominator(candidate)) &&
                     cJSON_AddStringToObject(entry, "text", text) && cJSON_AddItemToArray(list, entry);
                if (!ok) {
                    cJSON_Delete(entry);
                    status = api_fail_alloc(&out->error);
                }
            }
            if (status == COMPOSER_OK && (outputs.midi || outputs.musicxml)) {
                status = render_best(&scene, suggestions, &outputs, out);
            }
            if (status == COMPOSER_OK) {
                status = api_respond_json(out, document);
            } else {
                cJSON_Delete(document);
            }
        }
    }
    fill_suggestions_destroy(suggestions);
    fill_registry_destroy(registry);
    if (have_scene) {
        plan_destroy(scene.plan);
    }
    cJSON_Delete(request);
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}
