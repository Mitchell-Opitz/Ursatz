#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/plan_json.h>

#include "api_grow.h"
#include "api_workflow.h"
#include "util/request_parse.h"

/* `fill` as a composer API entry (docs/cli-fill.md): a plan realized in stages. */

#define MAX_CANDIDATES 32

static ComposerStatus fail_alloc(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

typedef struct {
    const char *style_path;
    const char *style_weight_text;
    const char *blend_path;
    const char *blend_weight_text;
    const char *blend_weights_text;
    const char *without[PLAN_STYLE_WITHOUT_MAX];
    size_t without_count;
    ComposerRatio style_weight; /* 0/0 unless style_weight was given */
} FillStyle;

/* The style members of a request (fill and the commands that compose a plan again). */
static ComposerStatus read_style_members(const cJSON *request, FillStyle *out, ComposerError *error) {
    bool present = false;
    memset(out, 0, sizeof(*out));
    if (!api_member_string(request, "style", &out->style_path, &present, error) ||
        !api_member_string(request, "style_weight", &out->style_weight_text, &present, error) ||
        !api_member_string(request, "blend", &out->blend_path, &present, error) ||
        !api_member_string(request, "blend_weight", &out->blend_weight_text, &present, error) ||
        !api_member_string(request, "blend_weights", &out->blend_weights_text, &present, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(request, "style_without");
    if (without && !cJSON_IsNull(without)) {
        if (!cJSON_IsArray(without)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without must be a list of style categories");
        }
        const cJSON *name = NULL;
        cJSON_ArrayForEach(name, without) {
            if (!cJSON_IsString(name)) {
                return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without must be a list of style categories");
            }
            ComposerStatus known = api_style_category_check(name->valuestring, error);
            if (known != COMPOSER_OK) {
                return known;
            }
            if (out->without_count == PLAN_STYLE_WITHOUT_MAX) {
                return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without names at most %d style categories",
                                PLAN_STYLE_WITHOUT_MAX);
            }
            out->without[out->without_count++] = name->valuestring;
        }
    }
    if (out->style_weight_text && !request_parse_ratio(out->style_weight_text, &out->style_weight)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/style_weight",
                        "style_weight must be a fraction like 1/2 or a whole number, not '%s'", out->style_weight_text);
    }
    return COMPOSER_OK;
}

/*
 * The style the run prefers: the request's, else the STYLE constraint on the plan's form node, and the weight from the
 * request, else the constraint's. *style stays NULL when there is none.
 */
static ComposerStatus resolve_style(const FillStyle *options, const PiecePlan *plan, ComposerStyle **style,
                                    ComposerRatio *weight, ComposerError *error) {
    const PlanStyle *planned = NULL;
    NodeId root = plan_root(plan);
    for (size_t i = 0; root != PLAN_NODE_NONE && i < plan_node_constraint_count(plan, root); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, root, i);
        if (constraint->kind == PLAN_CONSTRAINT_STYLE) {
            planned = &constraint->params.style;
        }
    }
    const char *path = options->style_path ? options->style_path : (planned ? planned->path : NULL);
    /* style_without is the list when given; else the plan keeps the categories its style was read without, and a style
       of its own starts with none. */
    const char *without[PLAN_STYLE_WITHOUT_MAX];
    size_t without_count = 0;
    if (options->without_count > 0) {
        for (; without_count < options->without_count; without_count++) {
            without[without_count] = options->without[without_count];
        }
    } else if (planned && !options->style_path) {
        for (; without_count < planned->without_count; without_count++) {
            without[without_count] = planned->without[without_count];
        }
    }
    /* The plan's blend goes with the plan's style: a style of its own is not blended unless blend says so. */
    const char *blend_path = options->blend_path;
    if (!blend_path && !options->style_path && planned && planned->blend_path[0]) {
        blend_path = planned->blend_path;
    }
    ComposerStyleWeights blend_weights;
    memset(&blend_weights, 0, sizeof blend_weights);
    bool weights_given = options->blend_path || options->blend_weight_text || options->blend_weights_text;
    if (!path) {
        if (options->style_weight_text) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/style_weight", "style_weight needs a style: give style or a plan that has one");
        }
        if (options->without_count > 0) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without",
                            "style_without needs a style: give style or a plan that has one");
        }
        return api_read_blend(false, blend_path, options->blend_weight_text, options->blend_weights_text, &blend_weights, error);
    }
    weight->numerator = planned ? planned->weight_numerator : 0;
    weight->denominator = planned ? planned->weight_denominator : 0;
    if (options->style_weight_text) {
        *weight = options->style_weight;
    }
    if (!blend_path) {
        ComposerStatus checked =
            api_read_blend(true, NULL, options->blend_weight_text, options->blend_weights_text, &blend_weights, error);
        return checked != COMPOSER_OK ? checked : api_load_style(path, without, without_count, "/style", "/style_without", style, error);
    }
    if (weights_given) {
        ComposerStatus read =
            api_read_blend(true, blend_path, options->blend_weight_text, options->blend_weights_text, &blend_weights, error);
        if (read != COMPOSER_OK) {
            return read;
        }
    } else {
        for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
            blend_weights.numerator[d] = planned->blend_numerator[d];
            blend_weights.denominator[d] = planned->blend_denominator[d];
        }
    }
    return api_load_blend(path, blend_path, &blend_weights, without, without_count, style, error);
}

static cJSON *candidates_json(const ComposeCandidates *candidates, unsigned long written) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < candidates->count; i++) {
        const ComposeCandidate *candidate = &candidates->items[i];
        char score[48];
        snprintf(score, sizeof score, "%lu/%lu", candidate->score.numerator, candidate->score.denominator);
        cJSON *item = cJSON_CreateObject();
        if (!item || !cJSON_AddNumberToObject(item, "seed", (double)candidate->seed) ||
            !cJSON_AddNumberToObject(item, "agreed", (double)candidate->agreed) ||
            !cJSON_AddNumberToObject(item, "planned", (double)candidate->planned) ||
            !cJSON_AddStringToObject(item, "score", score) ||
            !cJSON_AddBoolToObject(item, "written", candidate->seed == written) || !cJSON_AddItemToArray(list, item)) {
            cJSON_Delete(item);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

ComposerStatus composer_api_fill(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *root = NULL;
    char *text = NULL;
    ApiOutputs outputs;
    FillStyle style_options;
    unsigned long seed = 0;
    unsigned long candidates_wanted = 0;
    bool seeded = false;
    bool present = false;
    ComposerStatus status = api_parse_request(request_json, &root, &out->error);
    bool check = false;
    if (status == COMPOSER_OK) {
        status = api_read_outputs(root, true, &outputs, &out->error);
    }
    if (status == COMPOSER_OK) {
        if (!api_member_whole(root, "seed", 0, API_SEED_MAX, &seed, &seeded, &out->error) ||
            !api_member_whole(root, "candidates", 1, MAX_CANDIDATES, &candidates_wanted, &present, &out->error) ||
            !api_member_bool(root, "check", &check, &present, &out->error)) {
            status = COMPOSER_ERR_REQUEST;
        }
        seed = seeded ? seed : api_default_seed();
    }
    if (status == COMPOSER_OK) {
        status = read_style_members(root, &style_options, &out->error);
    }
    if (status == COMPOSER_OK) {
        status = api_read_plan_text(root, "plan", &text, &out->error);
    }

    /* The seeds are tried one after another, each from a new copy of the plan; only a stuck run can be mended by the next. */
    PiecePlan *plan = NULL;
    ComposeOutcome outcome;
    memset(&outcome, 0, sizeof(outcome));
    GivenSnapshot *snapshot = NULL;
    bool done = false;
    bool realized = false;
    unsigned long used = seed;
    ComposeCandidates candidates;
    memset(&candidates, 0, sizeof(candidates));
    ComposerStyle *style = NULL;
    ComposerRatio style_weight = {0, 0};
    for (unsigned long attempt = 0; attempt < API_SEED_TRIES && !done && status == COMPOSER_OK; attempt++) {
        if (job_step(job, "seed", (unsigned)attempt, API_SEED_TRIES)) {
            status = api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
            break;
        }
        PiecePlan *fresh = NULL;
        status = api_plan_from_text(text, "plan", &fresh, &out->error);
        if (status != COMPOSER_OK) {
            break;
        }
        if (!snapshot) {
            /* What the user gave, kept to show after the run that none of it was changed. */
            snapshot = given_snapshot(fresh);
            status = resolve_style(&style_options, fresh, &style, &style_weight, &out->error);
            if (status != COMPOSER_OK) {
                plan_destroy(fresh);
                break;
            }
        }
        ComposeConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = (seed + attempt) & API_SEED_MAX;
        config.analyzer_check = check;
        config.style = style;
        config.style_weight = style_weight;
        config.job = job;
        if (candidates_wanted > 0) {
            /* The candidates run from the seed and the next ones; the best is written. */
            PieceResult ran = compose_candidates(fresh, &config, candidates_wanted, &candidates);
            if (job_cancelled(job)) {
                plan_destroy(fresh);
                status = api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
            } else if (ran == PIECE_ERR_BAD_REQUEST) {
                plan = fresh;
                realized = true;
                done = true;
            } else if (ran != PIECE_OK) {
                plan_destroy(fresh);
                status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "%s", piece_result_name(ran));
            } else if (candidates.count == 0) {
                plan_destroy(fresh);
                status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "",
                                  "no candidate could be completed from seeds %lu to %lu", config.seed,
                                  (config.seed + candidates.tried - 1) & API_SEED_MAX);
            } else {
                plan_destroy(fresh);
                plan = candidates.items[0].plan;
                outcome = candidates.items[0].outcome;
                used = candidates.items[0].seed & API_SEED_MAX;
                candidates.items[0].plan = NULL;
                memset(&candidates.items[0].outcome, 0, sizeof(candidates.items[0].outcome));
                done = true;
            }
            break;
        }
        ComposeOutcome result;
        memset(&result, 0, sizeof(result));
        PieceResult ran = compose_run(fresh, &config, &result);
        if (plan) {
            plan_destroy(plan);
            compose_outcome_destroy(&outcome);
        }
        plan = fresh;
        outcome = result;
        used = config.seed;
        if (result.cancelled || job_cancelled(job)) {
            status = api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
        } else if (ran == PIECE_OK) {
            done = true;
        } else if (ran == PIECE_ERR_BAD_REQUEST) {
            /* No open theme section: the plan is written already (one from grow --out is), so it is only exported. */
            realized = true;
            done = true;
        } else if (ran != PIECE_ERR_STUCK) {
            status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "%s", piece_result_name(ran));
        }
    }
    free(text);
    if (status == COMPOSER_OK && !done) {
        status = api_compose_stuck(plan, &outcome, seed, used, out);
    }
    ApiGivenRuns *given = calloc(1, sizeof(*given));
    if (!given) {
        status = status == COMPOSER_OK ? fail_alloc(&out->error) : status;
    } else {
        given->unchanged = true;
    }
    if (status == COMPOSER_OK && done && !realized && snapshot) {
        given->count = given_snapshot_report(snapshot, plan, given->runs, API_MAX_GIVEN_RUNS);
        given->count = given->count > API_MAX_GIVEN_RUNS ? API_MAX_GIVEN_RUNS : given->count;
        for (size_t i = 0; i < given->count; i++) {
            given->unchanged = given->unchanged && given->runs[i].unchanged;
        }
        if (!given->unchanged) {
            status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "the run changed the given material; nothing is written");
        }
    }
    given_snapshot_destroy(snapshot);
    if (status == COMPOSER_OK) {
        cJSON *extra = NULL;
        if (candidates.count > 0) {
            extra = cJSON_CreateObject();
            if (!extra || !cJSON_AddItemToObject(extra, "candidates", candidates_json(&candidates, used)) ||
                !cJSON_AddNumberToObject(extra, "candidates_tried", (double)candidates.tried)) {
                cJSON_Delete(extra);
                extra = NULL;
                status = fail_alloc(&out->error);
            }
        }
        cJSON *document = status == COMPOSER_OK ? api_compose_document(plan, used, realized, &outcome, given, extra) : NULL;
        if (status == COMPOSER_OK && !document) {
            status = fail_alloc(&out->error);
        }
        if (status == COMPOSER_OK) {
            status = api_record_outputs(plan, &outcome, &outputs, document, out);
        }
        if (status == COMPOSER_OK) {
            status = api_render_outputs(plan, &outputs, out);
        }
        if (status == COMPOSER_OK) {
            status = api_respond_json(out, document);
        } else {
            cJSON_Delete(document);
        }
    }
    free(given);
    compose_candidates_destroy(&candidates);
    if (plan) {
        plan_destroy(plan);
        compose_outcome_destroy(&outcome);
    }
    composer_style_destroy(style);
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
