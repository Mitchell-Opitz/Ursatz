#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#include "api_piece.h"
#include "api_vocabulary.h"
#include "api_workflow.h"
#include "util/plan_util.h"
#include "util/request_parse.h"
#include "util/util.h"

/* `progression` as a composer API entry (docs/cli-progression.md): a chord progression for one phrase, with a bass line. */

#define MAX_BARS 64
#define MAX_CHORDS 64
#define MAX_SUGGESTIONS 8

typedef enum { EMBELLISH_NONE, EMBELLISH_LOW, EMBELLISH_MEDIUM, EMBELLISH_HIGH } EmbellishLevel;

typedef struct {
    const char *key_text;
    const char *meter_text;
    const char *schema_text;
    const char *sequence_text;
    AnalysisKey key;
    PlanMeter meter;
    long bars;
    CadenceType cadence;
    unsigned long seed;
    EmbellishLevel embellish;
    bool bass;
    GalantSchemaId schema;
    bool has_schema;
    ScalePatternId sequence;
    bool has_sequence;
    unsigned long suggest; /* 0 for none */
    bool plans;
    const ComposerJob *job;
} Options;

static ComposerStatus fail_alloc(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

/* --- reading the request --------------------------------------------------------------------------------- */

/*
 * A sequence continues something, and so do the schemas that are not opening gambits: for those the first bar
 * is a tonic chord, written before the operators run, and the skeleton follows it.
 */
static bool needs_lead(const Options *options) {
    return options->has_sequence ||
           (options->has_schema && galant_prototype(options->schema)->function != SCHEMA_FUNCTION_OPENING_GAMBIT);
}

static bool present(const cJSON *request, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(request, name);
    return item && !cJSON_IsNull(item);
}

static ComposerStatus read_request(const cJSON *request, Options *options, ApiOutputs *outputs, ComposerError *error) {
    const char *cadence_text = NULL;
    const char *embellish_text = NULL;
    bool found;
    memset(options, 0, sizeof(*options));
    options->bass = true;
    if (!present(request, "key") || !present(request, "meter") || !present(request, "bars") || !present(request, "cadence")) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "", "progression needs key, meter, bars and cadence");
    }
    if (!api_member_string(request, "key", &options->key_text, &found, error) ||
        !api_member_string(request, "meter", &options->meter_text, &found, error) ||
        !api_member_string(request, "cadence", &cadence_text, &found, error) ||
        !api_member_string(request, "embellish", &embellish_text, &found, error) ||
        !api_member_string(request, "schema", &options->schema_text, &found, error) ||
        !api_member_string(request, "sequence", &options->sequence_text, &found, error) ||
        !api_member_bool(request, "bass", &options->bass, &found, error) ||
        !api_member_bool(request, "plans", &options->plans, &found, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (!request_parse_key(options->key_text, &options->key)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/key",
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                        "for example \"D minor\"",
                        options->key_text);
    }
    if (!request_parse_meter(options->meter_text, &options->meter) || !harmony_meter_supported(&options->meter)) {
        return api_fail_meter(error, "/meter", options->meter_text);
    }
    unsigned long bars = 0;
    if (!api_member_whole(request, "bars", 1, MAX_BARS, &bars, &found, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    options->bars = (long)bars;
    if (!request_parse_cadence(cadence_text, &options->cadence)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/cadence", "unknown cadence '%s'; use PAC, IAC, HC, DC or PC", cadence_text);
    }
    unsigned long seed = 0;
    if (!api_member_whole(request, "seed", 0, API_SEED_MAX, &seed, &found, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    options->seed = found ? seed : api_default_seed();
    if (embellish_text) {
        static const char *const LEVELS[4] = {"none", "low", "medium", "high"};
        bool known = false;
        for (int i = 0; i < 4; i++) {
            if (request_equals_ignore_case(embellish_text, LEVELS[i])) {
                options->embellish = (EmbellishLevel)i;
                known = true;
            }
        }
        if (!known) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/embellish", "embellish must be none, low, medium or high, not '%s'",
                            embellish_text);
        }
    }
    ComposerStatus status = api_read_outputs(request, false, outputs, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (options->schema_text) {
        if (!harmony_schema_find(options->schema_text, &options->schema)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/schema", "unknown schema '%s' (list_schemas lists them)",
                            options->schema_text);
        }
        options->has_schema = true;
        if (!harmony_schema_available(options->schema, options->key.mode)) {
            return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/schema", "the schema '%s' cannot be written in a %s key",
                            options->schema_text, options->key.mode == KEY_STRUCTURE_MODE_MINOR ? "minor" : "major");
        }
    }
    if (options->sequence_text) {
        if (!harmony_sequence_find(options->sequence_text, &options->sequence)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/sequence", "unknown sequence '%s' (list_schemas lists them)",
                            options->sequence_text);
        }
        options->has_sequence = true;
    }
    if (!api_member_whole(request, "suggest", 1, MAX_SUGGESTIONS, &options->suggest, &found, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (!options->bass && (options->has_schema || options->has_sequence)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/bass",
                        "schema and sequence write their own bass, so they cannot be used with bass false");
    }
    if (!options->bass && (outputs->midi || outputs->musicxml)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/bass",
                        "midi and musicxml write the bass line (harmony makes no sound yet), so they cannot be used with bass "
                        "false");
    }
    size_t minimum = harmony_phrase_min_bars(options->cadence, &options->meter) + (needs_lead(options) ? 1 : 0);
    if ((size_t)options->bars < minimum) {
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/bars", "%ld bar%s is too short for a %s in %s; at least %zu bars are needed",
                        options->bars, options->bars == 1 ? "" : "s", plan_cadence_name(options->cadence), options->meter_text,
                        minimum);
    }
    return COMPOSER_OK;
}

/* --- building the plan ------------------------------------------------------------------------------------ */

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
            if (pitch % 12 == analysis_key_tonic_pitch_class(&options->key) &&
                (tonic_note.midi % 12 != analysis_key_tonic_pitch_class(&options->key) || distance < best)) {
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

/* --- running the operators -------------------------------------------------------------------------------- */

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
static bool run_stage(PiecePlan *plan, const Options *options, unsigned long seed, Stage stage, const Parameters *parameters,
                      FillRecord **record) {
    FillOperatorRegistry *registry = registry_for(options, parameters, stage);
    FillConfig config = config_for(seed);
    FillOutcome outcome;
    if (!registry) {
        return false;
    }
    config.job = options->job;
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
static Made make_progression(const Options *options, const Parameters *parameters, unsigned long seed, long suggestion,
                             Built *built) {
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

/* --- the result ------------------------------------------------------------------------------------------- */

static const char *function_name(const PlanHarmony *harmony) {
    const char *name = harmony->has_function ? harmonic_function_name(harmony->function) : NULL;
    return name ? name : "unknown";
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
                util_pitch_name(notes->notes[n].midi, false, name, sizeof(name));
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

/* Adds "heard", "chords" and, when asked, "plan" and "musicxml" of one progression to object. */
static ComposerStatus add_progression(cJSON *object, const Options *options, const Built *built, const ApiOutputs *outputs,
                                      ComposerError *error) {
    Row rows[MAX_CHORDS];
    size_t count = collect_rows(options, built, rows);
    cJSON *chords = cJSON_CreateArray();
    bool ok = cJSON_AddStringToObject(object, "heard", heard_name(options, built)) != NULL && chords &&
              cJSON_AddItemToObject(object, "chords", chords);
    if (!ok) {
        cJSON_Delete(chords);
        return fail_alloc(error);
    }
    for (size_t i = 0; i < count; i++) {
        cJSON *row = cJSON_CreateObject();
        ok = row && cJSON_AddNumberToObject(row, "bar", rows[i].bar) && cJSON_AddNumberToObject(row, "beat", rows[i].beat) &&
             cJSON_AddStringToObject(row, "chord", rows[i].roman) && cJSON_AddStringToObject(row, "function", rows[i].function) &&
             cJSON_AddStringToObject(row, "bass", rows[i].bass) && cJSON_AddItemToArray(chords, row);
        if (!ok) {
            cJSON_Delete(row);
            return fail_alloc(error);
        }
    }
    if (options->plans) {
        cJSON *plan = api_piece_plan_object(built->plan);
        if (!plan || !cJSON_AddItemToObject(object, "plan", plan)) {
            cJSON_Delete(plan);
            return fail_alloc(error);
        }
    }
    if (outputs->musicxml) {
        RenderedPlan rendered;
        PlanExportError detail;
        char *text = NULL;
        if (plan_export_render(built->plan, &rendered, &detail) != PLAN_EXPORT_OK) {
            return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/musicxml", "cannot render the plan");
        }
        PlanExportResult result = rendered_plan_musicxml_text(&rendered, &outputs->export_options, &text, &detail);
        rendered_plan_destroy(&rendered);
        if (result != PLAN_EXPORT_OK) {
            return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/musicxml", "cannot write the MusicXML file: %s", detail.message);
        }
        ok = cJSON_AddStringToObject(object, "musicxml", text) != NULL;
        free(text);
        if (!ok) {
            return fail_alloc(error);
        }
    }
    return COMPOSER_OK;
}

static ComposerStatus build_document(const Options *options, const Built *built, size_t made, unsigned long seed,
                                     const ApiOutputs *outputs, cJSON **result, ComposerError *error) {
    char key[24];
    char meter[16];
    cJSON *document = cJSON_CreateObject();
    util_key_name(analysis_key_tonic_pitch_class(&options->key), options->key.mode == KEY_STRUCTURE_MODE_MINOR, key, sizeof(key));
    snprintf(meter, sizeof(meter), "%d/%d", options->meter.beats_per_bar, options->meter.beat_unit);
    bool ok = document && cJSON_AddStringToObject(document, "key", key) && cJSON_AddStringToObject(document, "meter", meter) &&
              cJSON_AddNumberToObject(document, "bars", (double)options->bars) &&
              cJSON_AddStringToObject(document, "cadence", plan_cadence_name(options->cadence)) &&
              cJSON_AddNumberToObject(document, "seed", (double)seed);
    ComposerStatus status = ok ? COMPOSER_OK : fail_alloc(error);
    if (status == COMPOSER_OK && options->suggest > 0) {
        cJSON *list = cJSON_AddArrayToObject(document, "alternatives");
        for (size_t k = 0; list && k < made && status == COMPOSER_OK; k++) {
            cJSON *entry = cJSON_CreateObject();
            if (!entry || !cJSON_AddItemToArray(list, entry)) {
                cJSON_Delete(entry);
                status = fail_alloc(error);
                break;
            }
            status = add_progression(entry, options, &built[k], outputs, error);
        }
        if (!list) {
            status = fail_alloc(error);
        }
        if (status == COMPOSER_OK && made < options->suggest) {
            char notice[96];
            cJSON *notices = cJSON_AddArrayToObject(document, "notices");
            snprintf(notice, sizeof(notice), "only %zu alternative%s found for this request", made, made == 1 ? "" : "s");
            if (!notices || !cJSON_AddItemToArray(notices, cJSON_CreateString(notice))) {
                status = fail_alloc(error);
            }
        }
    } else if (status == COMPOSER_OK) {
        status = add_progression(document, options, &built[0], outputs, error);
    }
    if (status != COMPOSER_OK) {
        cJSON_Delete(document);
        return status;
    }
    *result = document;
    return COMPOSER_OK;
}

/* --- the lists of names --------------------------------------------------------------------------------- */

/* `list_schemas`: {"schemas": [{"name", "function", "modes"}], "sequences": [name]}. */
static ComposerStatus list_names(ComposerResponse *out) {
    cJSON *document = cJSON_CreateObject();
    cJSON *schemas = document ? cJSON_AddArrayToObject(document, "schemas") : NULL;
    cJSON *sequences = schemas ? cJSON_AddArrayToObject(document, "sequences") : NULL;
    bool ok = sequences != NULL;
    for (int id = 0; ok && id < GALANT_SCHEMA_COUNT; id++) {
        bool major = harmony_schema_available((GalantSchemaId)id, KEY_STRUCTURE_MODE_MAJOR);
        bool minor = harmony_schema_available((GalantSchemaId)id, KEY_STRUCTURE_MODE_MINOR);
        if (!major && !minor) {
            continue;
        }
        cJSON *entry = cJSON_CreateObject();
        cJSON *modes = entry ? cJSON_CreateArray() : NULL;
        ok = entry && modes && cJSON_AddStringToObject(entry, "name", galant_schema_name((GalantSchemaId)id)) &&
             cJSON_AddStringToObject(entry, "function", schema_function_name(galant_prototype((GalantSchemaId)id)->function)) &&
             (!major || cJSON_AddItemToArray(modes, cJSON_CreateString("major"))) &&
             (!minor || cJSON_AddItemToArray(modes, cJSON_CreateString("minor"))) && cJSON_AddItemToObject(entry, "modes", modes) &&
             cJSON_AddItemToArray(schemas, entry);
        if (!ok) {
            cJSON_Delete(modes);
            cJSON_Delete(entry);
        }
    }
    for (int id = 0; ok && id < SCALE_PATTERN_COUNT; id++) {
        ok = cJSON_AddItemToArray(sequences, cJSON_CreateString(scale_pattern_name((ScalePatternId)id)));
    }
    if (!ok) {
        cJSON_Delete(document);
        return fail_alloc(&out->error);
    }
    return api_respond_json(out, document);
}

/* --- the entry -------------------------------------------------------------------------------------------- */

ComposerStatus composer_api_progression(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *request = NULL;
    Options options;
    ApiOutputs outputs;
    Parameters parameters;
    Built built[MAX_SUGGESTIONS];
    size_t made = 0;
    unsigned long seed = 0;
    bool skeleton_unused = false;
    memset(built, 0, sizeof(built));
    memset(&options, 0, sizeof(options));
    memset(&outputs, 0, sizeof(outputs));
    ComposerStatus status = api_parse_request(request_json, &request, &out->error);
    if (status == COMPOSER_OK) {
        bool list = false;
        bool found;
        if (!api_member_bool(request, "list_schemas", &list, &found, &out->error)) {
            status = COMPOSER_ERR_REQUEST;
        } else if (list) {
            status = list_names(out);
            cJSON_Delete(request);
            if (status != COMPOSER_OK) {
                ComposerError error = out->error;
                composer_response_destroy(out);
                out->error = error;
            }
            return status;
        }
    }
    if (status == COMPOSER_OK) {
        status = read_request(request, &options, &outputs, &out->error);
        if (status == COMPOSER_OK) {
            options.job = job;
            seed = options.seed;
        }
    }
    if (status == COMPOSER_OK) {
        parameters_for(&options, &parameters);
        if (options.suggest > 0) {
            /* The alternatives are the candidates the harmony stage proposes, each taken through the other stages. */
            size_t proposed = count_suggestions(&options, &parameters, seed);
            for (size_t i = 0; i < proposed && made < options.suggest && status == COMPOSER_OK; i++) {
                if (job_step(job, "progression", (unsigned)i, (unsigned)proposed)) {
                    status = api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
                } else if (make_progression(&options, &parameters, seed, (long)i, &built[made]) == MADE) {
                    made++;
                }
            }
        } else {
            for (int attempt = 0; attempt < API_SEED_TRIES && made == 0 && status == COMPOSER_OK; attempt++) {
                if (job_step(job, "progression", (unsigned)attempt, API_SEED_TRIES)) {
                    status = api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
                    break;
                }
                seed = (options.seed + (unsigned long)attempt) & 0xFFFFFFFFUL;
                Made result = make_progression(&options, &parameters, seed, -1, &built[0]);
                skeleton_unused = skeleton_unused || result == SKELETON_UNUSED;
                made += result == MADE;
            }
        }
        if (status == COMPOSER_OK && job_cancelled(job)) {
            status = api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
        }
    }
    if (status == COMPOSER_OK && made == 0) {
        unsigned long last = (options.seed + API_SEED_TRIES - 1) & 0xFFFFFFFFUL;
        if (skeleton_unused) {
            status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "%s could not be used for %s, %s, %ld bars, %s from seeds %lu to %lu",
                              options.has_schema ? "the schema" : "the sequence", options.key_text, options.meter_text,
                              options.bars, plan_cadence_name(options.cadence), options.seed, last);
        } else {
            status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "no valid progression for %s, %s, %ld bars, %s from seeds %lu to %lu",
                              options.key_text, options.meter_text, options.bars, plan_cadence_name(options.cadence),
                              options.seed, last);
        }
    }
    if (status == COMPOSER_OK) {
        cJSON *document = NULL;
        status = build_document(&options, built, made, seed, &outputs, &document, &out->error);
        if (status == COMPOSER_OK) {
            /* The first progression's own files. */
            status = api_render_outputs(built[0].plan, &outputs, out);
            if (status != COMPOSER_OK) {
                cJSON_Delete(document);
            } else {
                status = api_respond_json(out, document);
            }
        }
    }
    for (size_t i = 0; i < MAX_SUGGESTIONS; i++) {
        plan_destroy(built[i].plan);
    }
    cJSON_Delete(request);
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}
