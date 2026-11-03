#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/counterpoint.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/render.h>

#include "api_piece.h"
#include "api_workflow.h"
#include "motive/motive_internal.h"
#include "util/request_parse.h"
#include "util/util.h"

/* `counterpoint` as a composer API entry (docs/cli-counterpoint.md): a counter-melody against a line, in strict species. */

#define MAX_SUGGESTIONS 16
#define SIGNATURE_MAX 4096

typedef struct {
    const char *key_text;
    const char *meter_text;
    int species;
    bool above;
    AnalysisKey key;
    PlanMeter meter;
    unsigned long seed;
    unsigned long suggest;
    bool suggested; /* "suggest" was given */
    bool plans;
    const ComposerJob *job;
} Options;

/* One counter-melody written: its plan, the target and the verdict of the library. */
typedef struct {
    PiecePlan *plan;
    LaneSpanId target;
    unsigned long seed;
    CounterpointReport report;
    char signature[SIGNATURE_MAX];
} Result;

static ComposerStatus fail_alloc(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

static bool job_cancelled(const ComposerJob *job) {
    return job && job->cancelled && job->cancelled(job->user);
}

/* A member's value as text for a message: a string as it is, a number with %g. */
static const char *value_text(const cJSON *item, char *buffer, size_t size) {
    if (cJSON_IsString(item)) {
        return item->valuestring;
    }
    if (cJSON_IsNumber(item)) {
        snprintf(buffer, size, "%g", item->valuedouble);
        return buffer;
    }
    return "?";
}

/* --- reading the request ---------------------------------------------------------------------------------- */

static ComposerStatus read_request(const cJSON *request, Options *options, ApiOutputs *outputs, cJSON **line,
                                   ComposerError *error) {
    const char *mode_text = NULL;
    const char *position_text = NULL;
    bool found;
    char buffer[32];
    memset(options, 0, sizeof(*options));
    *line = NULL;
    const cJSON *species = cJSON_GetObjectItemCaseSensitive(request, "species");
    if (!species || cJSON_IsNull(species) || !cJSON_GetObjectItemCaseSensitive(request, "key") ||
        !cJSON_GetObjectItemCaseSensitive(request, "meter")) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "", "counterpoint needs species, key and meter");
    }
    unsigned long number = 0;
    const char *species_text = value_text(species, buffer, sizeof buffer);
    if (!(cJSON_IsString(species) || cJSON_IsNumber(species)) || !request_parse_whole_number(species_text, 4, &number) ||
        number < 1) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/species",
                        "species must be 1, 2, 3 or 4, not '%s' (fifth species is not written)", species_text);
    }
    options->species = (int)number;
    if (!api_member_string(request, "position", &position_text, &found, error) ||
        !api_member_string(request, "key", &options->key_text, &found, error) ||
        !api_member_string(request, "mode", &mode_text, &found, error) ||
        !api_member_string(request, "meter", &options->meter_text, &found, error) ||
        !api_member_bool(request, "plans", &options->plans, &found, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (position_text) {
        if (request_equals_ignore_case(position_text, "above")) {
            options->above = true;
        } else if (!request_equals_ignore_case(position_text, "below")) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/position", "position must be above or below, not '%s'", position_text);
        }
    }
    char combined[64];
    snprintf(combined, sizeof(combined), "%s", options->key_text);
    if (mode_text) {
        if (!request_equals_ignore_case(mode_text, "major") && !request_equals_ignore_case(mode_text, "minor")) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/mode", "mode must be major or minor, not '%s'", mode_text);
        }
        if (strchr(options->key_text, ' ')) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/key", "key already names a mode; give a tonic alone with mode");
        }
        snprintf(combined, sizeof(combined), "%s %s", options->key_text, mode_text);
        if (!request_parse_key(combined, &options->key)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/key", "unknown key '%s'", options->key_text);
        }
    } else if (!request_parse_key(options->key_text, &options->key)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/key",
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, for "
                        "example \"D minor\"",
                        options->key_text);
    }
    if (!request_parse_meter(options->meter_text, &options->meter)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/meter",
                        "meter '%s' is not a meter; write beats over a beat unit, for example 4/4", options->meter_text);
    }
    unsigned long seed = 0;
    if (!api_member_whole(request, "seed", 0, API_SEED_MAX, &seed, &found, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    options->seed = found ? seed : api_default_seed();
    options->suggest = 1;
    if (!api_member_whole(request, "suggest", 1, MAX_SUGGESTIONS, &options->suggest, &options->suggested, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    ComposerStatus status = api_read_outputs(request, false, outputs, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    /* The line is read as a motive of any length in the key and meter above. */
    *line = cJSON_Duplicate(request, true);
    if (!*line) {
        return fail_alloc(error);
    }
    cJSON_DeleteItemFromObjectCaseSensitive(*line, "key");
    if (!cJSON_AddStringToObject(*line, "key", combined)) {
        return fail_alloc(error);
    }
    return COMPOSER_OK;
}

/* --- the plan --------------------------------------------------------------------------------------------- */

static Rational fraction(long num, unsigned long den) {
    Rational time = {0, 1};
    rational_make(num, den, &time);
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
    PlanNote lane_notes[MOTIVE_MAX_NOTES];
    motive_lane_notes(motive, lane_notes);
    payload.notes.notes = lane_notes;
    payload.notes.note_count = motive->note_count;
    ok = ok && plan_add_lane_span(plan, owner, options->above ? PLAN_LANE_BASS : PLAN_LANE_MELODY, 0, all, &payload, &id) ==
                   PLAN_OK &&
         plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN) == PLAN_OK;
    ok = ok && plan_node_set_state(plan, owner, PLAN_STATE_GIVEN) == PLAN_OK;
    placeholder.midi = 60;
    placeholder.spelled = false;
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
        used += (size_t)snprintf(out + used, size - used, "%d@%lld/%lld,", notes->notes[i].midi, notes->notes[i].offset.num,
                                 notes->notes[i].offset.den);
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
    config.job = options->job;
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

/* --- the result ------------------------------------------------------------------------------------------- */

static cJSON *notes_json(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan span) {
    LaneSpanId ids[512];
    size_t found = plan_lane_find(plan, lane, voice, span, ids, 512);
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < found; i++) {
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        Rational base = plan_lane_span_span(plan, ids[i]).start;
        for (size_t n = 0; list && n < notes->note_count; n++) {
            Rational start;
            rational_add(base, notes->notes[n].offset, &start);
            cJSON *entry = cJSON_CreateObject();
            cJSON *offset = entry ? cJSON_CreateObject() : NULL;
            cJSON *duration = offset ? cJSON_CreateObject() : NULL;
            bool ok = duration && cJSON_AddNumberToObject(entry, "midi_pitch", notes->notes[n].midi) &&
                      cJSON_AddNumberToObject(offset, "num", (double)start.num) &&
                      cJSON_AddNumberToObject(offset, "den", (double)start.den) && cJSON_AddItemToObject(entry, "offset", offset) &&
                      cJSON_AddNumberToObject(duration, "num", (double)notes->notes[n].duration.num) &&
                      cJSON_AddNumberToObject(duration, "den", (double)notes->notes[n].duration.den) &&
                      cJSON_AddItemToObject(entry, "duration", duration) && cJSON_AddItemToArray(list, entry);
            if (!ok) {
                cJSON_Delete(duration);
                cJSON_Delete(offset);
                cJSON_Delete(entry);
                cJSON_Delete(list);
                list = NULL;
            }
        }
    }
    return list;
}

static ComposerStatus result_json(const Options *options, const Result *result, PlanSpan span, const ApiOutputs *outputs,
                                  cJSON **out, ComposerError *error) {
    cJSON *entry = cJSON_CreateObject();
    cJSON *line = notes_json(result->plan, options->above ? PLAN_LANE_BASS : PLAN_LANE_MELODY, 0, span);
    cJSON *counter = notes_json(result->plan, PLAN_LANE_INNER_VOICE, 1, span);
    cJSON *groups = cJSON_CreateArray();
    bool ok = entry && line && counter && groups && cJSON_AddNumberToObject(entry, "seed", (double)result->seed) &&
              cJSON_AddItemToObject(entry, "line", line);
    if (!ok) {
        cJSON_Delete(line);
        cJSON_Delete(counter);
        cJSON_Delete(groups);
        cJSON_Delete(entry);
        return fail_alloc(error);
    }
    ok = cJSON_AddItemToObject(entry, "counter_melody", counter) && cJSON_AddItemToObject(entry, "groups", groups);
    if (!ok) {
        cJSON_Delete(counter);
        cJSON_Delete(groups);
        cJSON_Delete(entry);
        return fail_alloc(error);
    }
    for (size_t g = 0; g < result->report.count; g++) {
        cJSON *group = cJSON_CreateObject();
        ok = group && cJSON_AddStringToObject(group, "name", result->report.name[g]) &&
             cJSON_AddBoolToObject(group, "hard_held", result->report.hard_ok[g]) &&
             cJSON_AddBoolToObject(group, "soft_held", result->report.soft_ok[g]) && cJSON_AddItemToArray(groups, group);
        if (!ok) {
            cJSON_Delete(group);
            cJSON_Delete(entry);
            return fail_alloc(error);
        }
    }
    if (options->plans) {
        cJSON *plan = api_piece_plan_object(result->plan);
        if (!plan || !cJSON_AddItemToObject(entry, "plan", plan)) {
            cJSON_Delete(plan);
            cJSON_Delete(entry);
            return fail_alloc(error);
        }
    }
    if (outputs->musicxml) {
        RenderedPlan rendered;
        PlanExportError detail;
        char *text = NULL;
        if (plan_export_render(result->plan, &rendered, &detail) != PLAN_EXPORT_OK) {
            cJSON_Delete(entry);
            return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/musicxml", "cannot render the plan");
        }
        PlanExportResult written = rendered_plan_musicxml_text(&rendered, &outputs->export_options, &text, &detail);
        rendered_plan_destroy(&rendered);
        if (written != PLAN_EXPORT_OK) {
            cJSON_Delete(entry);
            return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/musicxml", "cannot write the MusicXML file: %s", detail.message);
        }
        ok = cJSON_AddStringToObject(entry, "musicxml", text) != NULL;
        free(text);
        if (!ok) {
            cJSON_Delete(entry);
            return fail_alloc(error);
        }
    }
    *out = entry;
    return COMPOSER_OK;
}

/* --- the entry -------------------------------------------------------------------------------------------- */

ComposerStatus composer_api_counterpoint(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *request = NULL;
    cJSON *line = NULL;
    cJSON *document = NULL;
    Options options;
    ApiOutputs outputs;
    Motive motive;
    Result results[MAX_SUGGESTIONS];
    size_t made = 0;
    unsigned long last_seed = 0;
    memset(results, 0, sizeof(results));
    memset(&options, 0, sizeof(options));
    memset(&outputs, 0, sizeof(outputs));
    ComposerStatus status = api_parse_request(request_json, &request, &out->error);
    if (status == COMPOSER_OK) {
        status = read_request(request, &options, &outputs, &line, &out->error);
        options.job = job;
        last_seed = options.seed;
    }
    if (status == COMPOSER_OK) {
        /* A line of any length is read here: the one-bar limit belongs to a motive, not to a line to write against. */
        status = api_read_motive(line, "", &motive, NULL, &out->error);
    }
    /* Whether the line is one the species can be written against, before any seed is tried. */
    if (status == COMPOSER_OK) {
        LaneSpanId target;
        PiecePlan *plan = build_plan(&options, &motive, &target);
        const char *why = plan ? counterpoint_why_not(plan, target) : "the plan could not be built";
        if (why) {
            status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "cannot be written as asked: %s", why);
        }
        plan_destroy(plan);
    }
    if (status == COMPOSER_OK) {
        unsigned long tries = (unsigned long)API_SEED_TRIES * options.suggest;
        for (unsigned long attempt = 0; attempt < tries && made < options.suggest && status == COMPOSER_OK; attempt++) {
            Result result;
            if (composer_job_step(job, "counterpoint", (unsigned)attempt, (unsigned)tries)) {
                status = api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
                break;
            }
            last_seed = (options.seed + attempt) & API_SEED_MAX;
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
        if (status == COMPOSER_OK && job_cancelled(job)) {
            status = api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
        }
    }
    if (status == COMPOSER_OK && made == 0) {
        status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "",
                          "cannot be written as asked: no species %d counter-melody %s the line holds the rules in %s, "
                          "from seeds %lu to %lu",
                          options.species, options.above ? "above" : "below", options.key_text, options.seed, last_seed);
    }
    if (status == COMPOSER_OK) {
        char key[24];
        char meter[16];
        PlanSpan span = {fraction(0, 1), motive.length};
        util_key_name(analysis_key_tonic_pitch_class(&options.key), options.key.mode == KEY_STRUCTURE_MODE_MINOR, key, sizeof(key));
        snprintf(meter, sizeof(meter), "%d/%d", options.meter.beats_per_bar, options.meter.beat_unit);
        document = cJSON_CreateObject();
        cJSON *list = document ? cJSON_CreateArray() : NULL;
        bool ok = list && cJSON_AddNumberToObject(document, "species", options.species) &&
                  cJSON_AddStringToObject(document, "position", options.above ? "above" : "below") &&
                  cJSON_AddStringToObject(document, "key", key) && cJSON_AddStringToObject(document, "meter", meter) &&
                  cJSON_AddItemToObject(document, "results", list);
        if (!ok) {
            cJSON_Delete(list);
            status = fail_alloc(&out->error);
        }
        for (size_t k = 0; status == COMPOSER_OK && k < made; k++) {
            cJSON *entry = NULL;
            status = result_json(&options, &results[k], span, &outputs, &entry, &out->error);
            if (status == COMPOSER_OK && !cJSON_AddItemToArray(list, entry)) {
                cJSON_Delete(entry);
                status = fail_alloc(&out->error);
            }
        }
        if (status == COMPOSER_OK && made < options.suggest && options.suggested) {
            char notice[96];
            cJSON *notices = cJSON_AddArrayToObject(document, "notices");
            snprintf(notice, sizeof(notice), "only %zu different counter-melod%s found for this request", made,
                     made == 1 ? "y" : "ies");
            if (!notices || !cJSON_AddItemToArray(notices, cJSON_CreateString(notice))) {
                status = fail_alloc(&out->error);
            }
        }
        if (status == COMPOSER_OK) {
            /* The first counter-melody's own files. */
            status = api_render_outputs(results[0].plan, &outputs, out);
        }
        if (status == COMPOSER_OK) {
            status = api_respond_json(out, document);
            document = NULL;
        }
    }
    cJSON_Delete(document);
    for (size_t k = 0; k < made; k++) {
        result_destroy(&results[k]);
    }
    cJSON_Delete(line);
    cJSON_Delete(request);
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}
