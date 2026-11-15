#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/regenerate.h>

#include "api_workflow.h"

/* `regenerate` as a composer API entry (docs/cli-regenerate.md): one node of a filled plan written again. */

static ComposerStatus fail_cancelled(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_CANCELLED, "", "cancelled");
}

typedef struct {
    size_t sections;
    char letters[API_MAX_SECTIONS][PLAN_SECTION_LETTER_MAX];
    size_t spans[PLAN_LANE_INNER_VOICE + 1];
} Reopened;

static void read_reopened(const PiecePlan *plan, const FillTarget *scope, size_t count, Reopened *out) {
    ApiSection sections[API_MAX_SECTIONS];
    size_t section_count = api_plan_sections(plan, sections, API_MAX_SECTIONS);
    section_count = section_count > API_MAX_SECTIONS ? API_MAX_SECTIONS : section_count;
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < count; i++) {
        if (scope[i].kind == FILL_TARGET_LANE_SPAN) {
            out->spans[plan_lane_span_lane(plan, (LaneSpanId)scope[i].id)]++;
            continue;
        }
        for (size_t s = 0; s < section_count && out->sections < API_MAX_SECTIONS; s++) {
            if (sections[s].node == (NodeId)scope[i].id) {
                snprintf(out->letters[out->sections++], PLAN_SECTION_LETTER_MAX, "%s", sections[s].letter);
            }
        }
    }
}

/* {"reopened": {"sections": [...], "spans": {"harmony", "bass", "melody", "inner_voice"}}}. */
static cJSON *reopened_json(const Reopened *reopened) {
    cJSON *extra = cJSON_CreateObject();
    cJSON *object = extra ? cJSON_AddObjectToObject(extra, "reopened") : NULL;
    cJSON *sections = object ? cJSON_AddArrayToObject(object, "sections") : NULL;
    cJSON *spans = object ? cJSON_CreateObject() : NULL;
    bool ok = sections && spans;
    for (size_t i = 0; ok && i < reopened->sections; i++) {
        ok = cJSON_AddItemToArray(sections, cJSON_CreateString(reopened->letters[i]));
    }
    ok = ok && cJSON_AddNumberToObject(spans, "harmony", (double)reopened->spans[PLAN_LANE_HARMONY]) &&
         cJSON_AddNumberToObject(spans, "bass", (double)reopened->spans[PLAN_LANE_BASS]) &&
         cJSON_AddNumberToObject(spans, "melody", (double)reopened->spans[PLAN_LANE_MELODY]) &&
         cJSON_AddNumberToObject(spans, "inner_voice", (double)reopened->spans[PLAN_LANE_INNER_VOICE]) &&
         cJSON_AddItemToObject(object, "spans", spans);
    if (!ok) {
        cJSON_Delete(spans);
        cJSON_Delete(extra);
        return NULL;
    }
    return extra;
}

ComposerStatus composer_api_regenerate(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *root = NULL;
    char *text = NULL;
    ApiTarget target;
    ApiOutputs outputs;
    unsigned long seed = 0;
    bool seeded = false;
    ComposerStatus status = api_parse_request(request_json, &root, &out->error);
    if (status == COMPOSER_OK) {
        status = api_read_target(root, &target, &out->error);
    }
    if (status == COMPOSER_OK) {
        status = api_read_outputs(root, false, &outputs, &out->error);
    }
    if (status == COMPOSER_OK) {
        if (!api_member_whole(root, "seed", 0, API_SEED_MAX, &seed, &seeded, &out->error)) {
            status = COMPOSER_ERR_REQUEST;
        }
        seed = seeded ? seed : api_default_seed();
    }
    if (status == COMPOSER_OK) {
        status = api_read_plan_text(root, "plan", &text, &out->error);
    }

    /* What will be reopened, read from the plan as it is now. */
    PiecePlan *before = NULL;
    Reopened reopened;
    char what[96] = "";
    memset(&reopened, 0, sizeof reopened);
    FillTarget *targets = NULL;
    size_t target_count = 0;
    if (status == COMPOSER_OK) {
        status = api_plan_from_text(text, "plan", &before, &out->error);
    }
    if (status == COMPOSER_OK) {
        status = api_target_list(before, &target, &targets, &target_count, &out->error);
        api_target_describe(before, &target, what, sizeof what);
    }
    if (status == COMPOSER_OK) {
        FillTarget *scope = NULL;
        size_t scope_count = 0;
        RegenerateResult scoped = plan_regenerate_scope_all(before, targets, target_count, &scope, &scope_count);
        if (scoped != REGENERATE_OK) {
            status = api_refused(scoped, what, &out->error);
        } else {
            read_reopened(before, scope, scope_count, &reopened);
        }
        free(scope);
    }
    free(targets);
    targets = NULL;
    plan_destroy(before);

    /* The seeds are tried one after another, each from a new copy of the plan; only a stuck run can be mended by the next. */
    PiecePlan *plan = NULL;
    ComposeOutcome outcome;
    memset(&outcome, 0, sizeof(outcome));
    bool done = false;
    unsigned long used = seed;
    for (unsigned long attempt = 0; attempt < API_SEED_TRIES && !done && status == COMPOSER_OK; attempt++) {
        if (job_step(job, "seed", (unsigned)attempt, API_SEED_TRIES)) {
            status = fail_cancelled(&out->error);
            break;
        }
        PiecePlan *fresh = NULL;
        status = api_plan_from_text(text, "plan", &fresh, &out->error);
        if (status != COMPOSER_OK) {
            break;
        }
        FillTarget *again = NULL;
        size_t again_count = 0;
        status = api_target_list(fresh, &target, &again, &again_count, &out->error);
        if (status != COMPOSER_OK) {
            plan_destroy(fresh);
            break;
        }
        ComposeOutcome result;
        memset(&result, 0, sizeof(result));
        ComposeConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = (seed + attempt) & API_SEED_MAX;
        config.job = job;
        RegenerateResult ran = plan_regenerate_all(fresh, again, again_count, &config, &result);
        free(again);
        if (plan) {
            plan_destroy(plan);
            compose_outcome_destroy(&outcome);
        }
        plan = fresh;
        outcome = result;
        used = config.seed;
        if (result.cancelled || job_cancelled(job)) {
            status = fail_cancelled(&out->error);
        } else if (ran == REGENERATE_OK) {
            done = true;
        } else if (ran != REGENERATE_ERR_STUCK) {
            status = api_refused(ran, what, &out->error);
        }
    }
    free(text);
    if (status == COMPOSER_OK && !done) {
        status = api_compose_stuck(plan, &outcome, seed, used, out);
    }
    if (status == COMPOSER_OK) {
        ApiGivenRuns *given = calloc(1, sizeof(*given));
        cJSON *extra = given ? reopened_json(&reopened) : NULL;
        cJSON *document = extra ? api_compose_document(plan, used, false, &outcome, given, extra) : NULL;
        free(given);
        if (!document) {
            status = api_fail_alloc(&out->error);
        } else {
            status = api_render_outputs(plan, &outputs, out);
            if (status == COMPOSER_OK) {
                status = api_respond_json(out, document);
            } else {
                cJSON_Delete(document);
            }
        }
    }
    if (plan) {
        plan_destroy(plan);
        compose_outcome_destroy(&outcome);
    }
    cJSON_Delete(root);
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        char *stuck = out->json;
        out->json = NULL;
        composer_response_destroy(out);
        out->error = error;
        out->json = stuck;
    }
    return status;
}
