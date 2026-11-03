#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>
#include <ursatz_composer/variations.h>

#include "api_piece.h"
#include "api_workflow.h"

/* `variations` and `harmonize` as composer API entries (docs/cli-variations.md): several distinct versions of a node. */

static ComposerStatus fail_alloc(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

static bool job_cancelled(const ComposerJob *job) {
    return job && job->cancelled && job->cancelled(job->user);
}

/* The MusicXML of a version as it is in memory (its ids are the ones a plan file renumbers). */
static ComposerStatus variation_musicxml(const PiecePlan *plan, const ApiOutputs *outputs, char **text, ComposerError *error) {
    RenderedPlan rendered;
    PlanExportError detail;
    *text = NULL;
    if (plan_export_render(plan, &rendered, &detail) != PLAN_EXPORT_OK) {
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/musicxml", "cannot render the plan");
    }
    PlanExportResult result = rendered_plan_musicxml_text(&rendered, &outputs->export_options, text, &detail);
    rendered_plan_destroy(&rendered);
    if (result != PLAN_EXPORT_OK) {
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/musicxml", "cannot write the MusicXML file: %s", detail.message);
    }
    return COMPOSER_OK;
}

static ComposerStatus variations_document(const PlanVariations *variations, const ApiOutputs *outputs, unsigned long requested,
                                          cJSON **result, ComposerError *error) {
    cJSON *document = cJSON_CreateObject();
    cJSON *list = document ? cJSON_AddArrayToObject(document, "variations") : NULL;
    bool ok = list != NULL;
    for (size_t i = 0; ok && i < variations->count; i++) {
        const PlanVariation *variation = &variations->items[i];
        cJSON *entry = cJSON_CreateObject();
        cJSON *plan = api_piece_plan_object(variation->plan);
        ok = entry && plan && cJSON_AddNumberToObject(entry, "index", (double)(i + 1)) &&
             cJSON_AddNumberToObject(entry, "seed", (double)variation->seed) && cJSON_AddItemToObject(entry, "plan", plan);
        if (!ok) {
            cJSON_Delete(plan);
        }
        if (ok && outputs->musicxml) {
            char *text = NULL;
            ComposerStatus rendered = variation_musicxml(variation->plan, outputs, &text, error);
            if (rendered != COMPOSER_OK) {
                cJSON_Delete(entry);
                cJSON_Delete(document);
                return rendered;
            }
            ok = cJSON_AddStringToObject(entry, "musicxml", text) != NULL;
            free(text);
        }
        cJSON *heard = ok ? api_heard_json(variation->plan, &variation->outcome) : NULL;
        ok = ok && heard && cJSON_AddItemToObject(entry, "sections", heard);
        if (!ok) {
            cJSON_Delete(heard);
        }
        if (!ok || !cJSON_AddItemToArray(list, entry)) {
            cJSON_Delete(entry);
            ok = false;
        }
    }
    ok = ok && cJSON_AddNumberToObject(document, "kept", (double)variations->count) &&
         cJSON_AddNumberToObject(document, "requested", (double)requested) &&
         cJSON_AddNumberToObject(document, "seeds_tried", (double)variations->seeds_tried);
    if (!ok) {
        cJSON_Delete(document);
        return fail_alloc(error);
    }
    *result = document;
    return COMPOSER_OK;
}

static ComposerStatus run_variations(const char *request_json, const ComposerJob *job, ComposerResponse *out, bool harmonize) {
    memset(out, 0, sizeof *out);
    cJSON *root = NULL;
    char *text = NULL;
    PiecePlan *plan = NULL;
    ApiTarget target;
    ApiOutputs outputs;
    FillTarget *targets = NULL;
    size_t target_count = 0;
    unsigned long count = 0;
    unsigned long seed = 0;
    bool present = false;
    bool seeded = false;
    PlanVariations variations;
    bool made = false;
    memset(&variations, 0, sizeof(variations));
    ComposerStatus status = api_parse_request(request_json, &root, &out->error);
    if (status == COMPOSER_OK) {
        status = api_read_target(root, &target, &out->error);
        if (status == COMPOSER_OK && harmonize && (!target.section || target.has_lane)) {
            status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "", "harmonize needs \"section\" (a letter) and no lane");
        }
    }
    if (status == COMPOSER_OK) {
        status = api_read_outputs(root, false, &outputs, &out->error);
    }
    if (status == COMPOSER_OK) {
        if (!api_member_whole(root, "count", 1, PLAN_VARIATIONS_MAX, &count, &present, &out->error) ||
            !api_member_whole(root, "seed", 0, API_SEED_MAX, &seed, &seeded, &out->error)) {
            status = COMPOSER_ERR_REQUEST;
        } else if (!present) {
            status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/count", "the request needs \"count\": 1 to %d", PLAN_VARIATIONS_MAX);
        }
        seed = seeded ? seed : api_default_seed();
    }
    if (status == COMPOSER_OK) {
        status = api_read_plan_text(root, "plan", &text, &out->error);
    }
    if (status == COMPOSER_OK) {
        status = api_plan_from_text(text, "plan", &plan, &out->error);
    }
    char what[96] = "";
    if (status == COMPOSER_OK) {
        api_target_describe(plan, &target, what, sizeof what);
        if (composer_job_step(job, "variations", 0, (unsigned)count)) {
            status = api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
        }
    }
    if (status == COMPOSER_OK) {
        ComposeConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        config.job = job;
        RegenerateResult ran;
        if (harmonize) {
            NodeId section;
            status = api_target_node(plan, &target, &section, &out->error);
            ran = status == COMPOSER_OK ? plan_harmonizations(plan, section, &config, count, &variations) : REGENERATE_ERR_NULL;
            made = status == COMPOSER_OK;
            if (status == COMPOSER_OK && ran == REGENERATE_ERR_BAD_TARGET) {
                status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "",
                                  "%s has no given melody over all its bars to harmonize (plan it with a given melody and fill it)",
                                  what);
            } else if (status == COMPOSER_OK && ran != REGENERATE_OK) {
                status = api_refused(ran, what, &out->error);
            }
        } else {
            status = api_target_list(plan, &target, &targets, &target_count, &out->error);
            ran = status == COMPOSER_OK ? plan_variations(plan, targets, target_count, &config, count, &variations)
                                        : REGENERATE_ERR_NULL;
            made = status == COMPOSER_OK;
            if (status == COMPOSER_OK && ran != REGENERATE_OK) {
                status = api_refused(ran, what, &out->error);
            }
        }
    }
    if (status == COMPOSER_OK && job_cancelled(job)) {
        status = api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
    }
    if (status == COMPOSER_OK && variations.count == 0) {
        status = api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "",
                          "no version of %s that differs from the plan was found in %zu seeds from %lu", what,
                          variations.seeds_tried, seed);
    }
    if (status == COMPOSER_OK) {
        cJSON *document = NULL;
        status = variations_document(&variations, &outputs, count, &document, &out->error);
        if (status == COMPOSER_OK) {
            status = api_respond_json(out, document);
        }
    }
    if (made) {
        plan_variations_destroy(&variations);
    }
    free(targets);
    free(text);
    plan_destroy(plan);
    cJSON_Delete(root);
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}

ComposerStatus composer_api_variations(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    return run_variations(request_json, job, out, false);
}

ComposerStatus composer_api_harmonize(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    return run_variations(request_json, job, out, true);
}
