#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <plan/plan_json.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/critic.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/preferences.h>

#include "api_critique.h"
#include "api_internal.h"
#include "api_workflow.h"
#include "check/hearing.h"

/*
 * composer_api_critique (R-721, docs/critic.md): the critic's judgment of candidates, one set of one to 16. A candidate is a plan
 * ("plan": the plan document of a theme or a piece) or a motive ("motive": a motive document), the request's optional "preferences" the
 * settings of the critic. Each candidate is judged with every candidate's line as the set (so one_rhythm compares them), and a plan that
 * has a node named "theme" is also observed against the Composer's hearing of it.
 */

#define SCHEMA_NAME "ursatz-critique-request"
#define MAX_CANDIDATES 16

typedef struct {
    Plan *plan; /* NULL for a motive candidate */
    ComposerHearing *hearing;
    CriticLine line;
    CriticJudgment judgment;
} Candidate;

static void candidate_destroy(Candidate *candidate) {
    critic_line_destroy(&candidate->line);
    hearing_destroy(candidate->hearing);
    plan_destroy(candidate->plan);
    memset(candidate, 0, sizeof(*candidate));
}

/* Prefixes the error's path with the candidate's. */
static UrsatzStatus under_candidate(UrsatzError *error, size_t index, UrsatzStatus status) {
    char prefix[32];
    char inner[sizeof error->path];
    snprintf(prefix, sizeof prefix, "/candidates/%zu", index);
    memcpy(inner, error->path, sizeof inner);
    /* The reader's path was cut at the buffer's size: a path longer than the room left loses its end. */
    snprintf(error->path, sizeof error->path, "%s%.*s", prefix, (int)(sizeof error->path - strlen(prefix) - 1), inner);
    return status;
}

static UrsatzStatus read_plan_candidate(const ComposerContext *context, const cJSON *item, size_t index, Candidate *out, UrsatzError *error) {
    char *text = NULL;
    UrsatzStatus status = api_read_plan_text(context->knowledge, item, "plan", &text, error);
    if (status != URSATZ_OK) {
        return under_candidate(error, index, status);
    }
    status = api_plan_from_text(context->knowledge, text, "plan", &out->plan, error);
    free(text);
    if (status != URSATZ_OK) {
        return under_candidate(error, index, status);
    }
    HearingResult heard = hearing_create(out->plan, context->knowledge, &out->hearing);
    if (heard != HEARING_OK) {
        snprintf(error->path, sizeof error->path, "/candidates/%zu/plan", index);
        return heard == HEARING_ERR_ALLOC ? api_fail_alloc(error)
                                          : api_fail(error, URSATZ_ERR_UNSATISFIABLE, error->path,
                                                     "the plan cannot be heard: it has no meter, key or written melody");
    }
    CriticResult made = critic_line_from_render(out->plan, hearing_rendered(out->hearing), &out->line);
    if (made != CRITIC_OK) {
        snprintf(error->path, sizeof error->path, "/candidates/%zu/plan", index);
        return made == CRITIC_ERR_ALLOC ? api_fail_alloc(error)
                                        : api_fail(error, URSATZ_ERR_UNSATISFIABLE, error->path, "the plan has no melody to judge");
    }
    return URSATZ_OK;
}

static UrsatzStatus read_motive_candidate(const ComposerContext *context, const cJSON *item, size_t index, Candidate *out, UrsatzError *error) {
    const cJSON *document = cJSON_GetObjectItemCaseSensitive(item, "motive");
    char *text = cJSON_IsObject(document) ? cJSON_PrintUnformatted(document) : NULL;
    MotiveOptions options;
    Motive motive;
    MotiveError detail;
    memset(&options, 0, sizeof(options));
    memset(&detail, 0, sizeof(detail));
    options.knowledge = context->knowledge;
    char path[sizeof error->path];
    snprintf(path, sizeof path, "/candidates/%zu/motive", index);
    if (text == NULL) {
        return cJSON_IsObject(document) ? api_fail_alloc(error) : api_fail(error, URSATZ_ERR_REQUEST, path, "\"motive\" must be a motive document");
    }
    MotiveResult read = motive_from_json(text, &options, &motive, &detail);
    cJSON_free(text);
    if (read != MOTIVE_OK) {
        return read == MOTIVE_ERR_ALLOC ? api_fail_alloc(error) : api_fail(error, URSATZ_ERR_REQUEST, path, "cannot read the motive: %s", detail.message);
    }
    CriticResult made = critic_line_from_motive(&motive, &out->line);
    if (made != CRITIC_OK) {
        return made == CRITIC_ERR_ALLOC ? api_fail_alloc(error) : api_fail(error, URSATZ_ERR_UNSATISFIABLE, path, "the motive has no melody to judge");
    }
    return URSATZ_OK;
}

static cJSON *ratio_json(Rational value) {
    cJSON *object = cJSON_CreateObject();
    if (!object || !cJSON_AddNumberToObject(object, "numerator", (double)value.num) || !cJSON_AddNumberToObject(object, "denominator", (double)value.den)) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

cJSON *api_critic_json(const CriticJudgment *judgment, const CriticSettings *settings) {
    cJSON *object = cJSON_CreateObject();
    cJSON *evaluators = object ? cJSON_AddArrayToObject(object, "evaluators") : NULL;
    cJSON *observations = evaluators ? cJSON_AddArrayToObject(object, "observations") : NULL;
    bool ok = observations != NULL && api_add(object, "score", ratio_json(judgment->score));
    for (int i = 0; ok && i < CRITIC_EVALUATOR_COUNT; i++) {
        cJSON *item = cJSON_CreateObject();
        ok = item && cJSON_AddItemToArray(evaluators, item) && cJSON_AddStringToObject(item, "rule", critic_evaluator_code((CriticEvaluator)i)) &&
             api_add(item, "penalty", ratio_json(judgment->findings[i].penalty)) && cJSON_AddNumberToObject(item, "weight", settings->weights[i]) &&
             cJSON_AddBoolToObject(item, "reversed", settings->reversed[i]);
    }
    for (size_t i = 0; ok && i < judgment->observation_count; i++) {
        const CriticObservation *observation = &judgment->observations[i];
        cJSON *item = cJSON_CreateObject();
        ok = item && cJSON_AddItemToArray(observations, item) && cJSON_AddStringToObject(item, "rule", observation->rule) &&
             cJSON_AddBoolToObject(item, "scored", observation->scored) &&
             api_add(item, "bar", observation->bar >= 0 ? cJSON_CreateNumber(observation->bar) : cJSON_CreateNull()) &&
             api_add(item, "beat", observation->bar >= 0 && observation->beat.num > 0 ? ratio_json(observation->beat) : cJSON_CreateNull()) &&
             cJSON_AddStringToObject(item, "message", observation->message);
    }
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

UrsatzStatus composer_api_critique(const ComposerContext *context, const char *request_json, const UrsatzJob *job, ComposerResponse *out) {
    (void)job; /* a judgment of at most 16 candidates is quick: there is no step to cancel at */
    memset(out, 0, sizeof(*out));
    UrsatzStatus status = api_require_knowledge(context, NULL, &out->error);
    cJSON *request = NULL;
    if (status == URSATZ_OK) {
        status = api_parse_request(request_json, &request, &out->error);
    }
    const char *schema = NULL;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(request, "schema_version");
    if (status == URSATZ_OK && !api_request_string(request, "", "schema", true, &schema, &out->error)) {
        status = URSATZ_ERR_REQUEST;
    }
    if (status == URSATZ_OK && strcmp(schema, SCHEMA_NAME) != 0) {
        status = api_fail(&out->error, URSATZ_ERR_REQUEST, "/schema", "the request must have \"schema\": \"%s\"", SCHEMA_NAME);
    }
    if (status == URSATZ_OK && (!cJSON_IsNumber(version) || version->valuedouble != 1)) {
        status = api_fail(&out->error, URSATZ_ERR_REQUEST, "/schema_version", "the request must have \"schema_version\": 1");
    }
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(request, "candidates");
    int count = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : 0;
    if (status == URSATZ_OK && (count < 1 || count > MAX_CANDIDATES)) {
        status = api_fail(&out->error, URSATZ_ERR_REQUEST, "/candidates", "the request needs \"candidates\": a list of 1 to %d candidates", MAX_CANDIDATES);
    }
    ComposerPreferences preferences;
    if (status == URSATZ_OK && !api_read_preferences(request, &preferences, NULL, &out->error)) {
        status = URSATZ_ERR_REQUEST;
    }
    Candidate candidates[MAX_CANDIDATES];
    memset(candidates, 0, sizeof(candidates));
    for (int i = 0; status == URSATZ_OK && i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(list, i);
        bool has_plan = cJSON_IsObject(item) && cJSON_GetObjectItemCaseSensitive(item, "plan") != NULL;
        bool has_motive = cJSON_IsObject(item) && cJSON_GetObjectItemCaseSensitive(item, "motive") != NULL;
        if (has_plan == has_motive) {
            char path[sizeof out->error.path];
            snprintf(path, sizeof path, "/candidates/%d", i);
            status = api_fail(&out->error, URSATZ_ERR_REQUEST, path, "a candidate is an object with a \"plan\" or a \"motive\", not both or neither");
        } else if (has_plan) {
            status = read_plan_candidate(context, item, (size_t)i, &candidates[i], &out->error);
        } else {
            status = read_motive_candidate(context, item, (size_t)i, &candidates[i], &out->error);
        }
    }
    const CriticLine *set[MAX_CANDIDATES];
    for (int i = 0; status == URSATZ_OK && i < count; i++) {
        set[i] = &candidates[i].line;
    }
    for (int i = 0; status == URSATZ_OK && i < count; i++) {
        Candidate *candidate = &candidates[i];
        CriticResult judged = critic_judge(&candidate->line, set, (size_t)count, context->knowledge, &preferences.critic, &candidate->judgment);
        NodeId theme = candidate->plan != NULL ? api_find_node(candidate->plan, "theme") : PLAN_NODE_NONE;
        if (judged == CRITIC_OK && theme != PLAN_NODE_NONE) {
            judged = critic_observe(candidate->hearing, candidate->plan, theme, &candidate->line, context->knowledge, &candidate->judgment);
        }
        if (judged == CRITIC_ERR_ALLOC) {
            status = api_fail_alloc(&out->error);
        } else if (judged != CRITIC_OK) {
            status = api_fail(&out->error, URSATZ_ERR_INTERNAL, "", "the critic could not judge candidate %d", i);
        }
    }
    cJSON *root = NULL;
    if (status == URSATZ_OK) {
        root = cJSON_CreateObject();
        cJSON *items = root ? cJSON_AddArrayToObject(root, "candidates") : NULL;
        bool ok = items && cJSON_AddStringToObject(root, "schema", "ursatz-critique") && cJSON_AddNumberToObject(root, "schema_version", 1);
        for (int i = 0; ok && i < count; i++) {
            cJSON *item = api_critic_json(&candidates[i].judgment, &preferences.critic);
            ok = item && cJSON_AddItemToArray(items, item) && cJSON_AddNumberToObject(item, "index", i);
            if (!ok) {
                cJSON_Delete(item);
            }
        }
        if (!ok) {
            cJSON_Delete(root);
            root = NULL;
            status = api_fail_alloc(&out->error);
        }
    }
    for (int i = 0; i < count; i++) {
        candidate_destroy(&candidates[i]);
    }
    cJSON_Delete(request);
    if (status != URSATZ_OK) {
        return status;
    }
    return api_respond_json(out, root);
}
