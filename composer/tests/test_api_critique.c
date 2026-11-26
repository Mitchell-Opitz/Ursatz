#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The critique entry (R-721, docs/critic.md): a motive and a grown period critiqued together (a set of two, so one_rhythm compares their
 * first bars), the period alone (the same evaluators but one_rhythm), the same period twice (one_rhythm 1), a request that gives the
 * critic group a weight of 0 and a reversed evaluator, and each refusal with its path. The requests and the responses are written beside
 * the test as api_critique_request_<n>.json and api_critique_response_<n>.json for the schema test.
 */

static int written = 0;

static char *plan_member = NULL;   /* the plan document of a period, as text */
static char *motive_member = NULL; /* the motive document of the pickup fixture, as text */

static void write_file(const char *kind, const char *text) {
    char name[64];
    snprintf(name, sizeof name, "api_critique_%s_%d.json", kind, written);
    FILE *file = fopen(name, "wb");
    assert(file);
    fputs(text, file);
    fclose(file);
}

/* The response of the request text; the request and its response are written for the schema test. */
static cJSON *critique(const char *request, UrsatzStatus expected, ComposerResponse *out) {
    UrsatzStatus status = composer_api_critique(test_context(), request, NULL, out);
    if (status != expected) {
        fprintf(stderr, "%s\nstatus %d, expected %d: %s at %s\n", request, status, expected, out->error.message, out->error.path);
        assert(false);
    }
    if (status != URSATZ_OK) {
        return NULL;
    }
    written++;
    write_file("request", request);
    write_file("response", out->json);
    cJSON *document = cJSON_Parse(out->json);
    assert(document);
    return document;
}

static void refused(const char *request, const char *path) {
    ComposerResponse response;
    UrsatzStatus status = composer_api_critique(test_context(), request, NULL, &response);
    if (status == URSATZ_OK || strcmp(response.error.path, path) != 0 || response.error.message[0] == '\0') {
        fprintf(stderr, "%s\nexpected a refusal at %s, got status %d path \"%s\" message \"%s\"\n", request, path, status, response.error.path,
                response.error.message);
        assert(false);
    }
    composer_response_destroy(&response);
}

static char *request_of(const char *candidates, const char *extra) {
    size_t size = strlen(candidates) + strlen(extra) + 128;
    char *text = malloc(size);
    assert(text);
    snprintf(text, size, "{\"schema\":\"ursatz-critique-request\",\"schema_version\":1,\"candidates\":[%s]%s}", candidates, extra);
    return text;
}

static char *candidate(const char *member, const char *document) {
    size_t size = strlen(member) + strlen(document) + 8;
    char *text = malloc(size);
    assert(text);
    snprintf(text, size, "{\"%s\":%s}", member, document);
    return text;
}

static char *joined(const char *a, const char *b) {
    size_t size = strlen(a) + strlen(b) + 2;
    char *text = malloc(size);
    assert(text);
    snprintf(text, size, "%s,%s", a, b);
    return text;
}

static const cJSON *item(const cJSON *document, int index) {
    return cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "candidates"), index);
}

static const cJSON *evaluator(const cJSON *candidate_item, const char *rule) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(candidate_item, "evaluators")) {
        if (strcmp(cJSON_GetObjectItemCaseSensitive(entry, "rule")->valuestring, rule) == 0) {
            return entry;
        }
    }
    assert(false);
    return NULL;
}

static double ratio(const cJSON *object) {
    return cJSON_GetObjectItemCaseSensitive(object, "numerator")->valuedouble / cJSON_GetObjectItemCaseSensitive(object, "denominator")->valuedouble;
}

static double penalty(const cJSON *candidate_item, const char *rule) {
    return ratio(cJSON_GetObjectItemCaseSensitive(evaluator(candidate_item, rule), "penalty"));
}

static void load_candidates(void) {
    cJSON *request = cJSON_Parse(test_must_read_file(COMPOSER_FIXTURE_DIR "/theme/period-contrast.json", NULL));
    assert(request && cJSON_AddNumberToObject(request, "seed", 1) && cJSON_AddBoolToObject(request, "plans", true));
    cJSON *preferences = cJSON_Parse("{\"development\":{\"functions\":{\"contrast\":true}}}");
    assert(preferences && cJSON_AddItemToObject(request, "preferences", preferences));
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    ComposerResponse response;
    assert(composer_api_theme(test_context(), printed, NULL, &response) == URSATZ_OK);
    free(printed);
    cJSON *document = cJSON_Parse(response.json);
    composer_response_destroy(&response);
    plan_member = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0), "plan"));
    cJSON_Delete(document);
    assert(plan_member);
    cJSON *motive = cJSON_Parse(test_must_read_file(COMPOSER_FIXTURE_DIR "/critic/motif-pickup.json", NULL));
    motive_member = cJSON_PrintUnformatted(motive);
    cJSON_Delete(motive);
    assert(motive_member);
}

static void test_sets(void) {
    char *motive = candidate("motive", motive_member);
    char *plan = candidate("plan", plan_member);
    ComposerResponse response;
    /* a motive and a period together: both judged, a score and eight evaluators each */
    char *both_text = joined(motive, plan);
    char *request = request_of(both_text, "");
    cJSON *both = critique(request, URSATZ_OK, &response);
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(both, "candidates")) == 2);
    for (int i = 0; i < 2; i++) {
        const cJSON *entry = item(both, i);
        assert(cJSON_GetObjectItemCaseSensitive(entry, "index")->valueint == i);
        assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(entry, "evaluators")) == 10);
        assert(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(entry, "score")));
        assert(evaluator(entry, "leap_load") && cJSON_GetObjectItemCaseSensitive(evaluator(entry, "leap_load"), "weight")->valueint == 1);
        assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(evaluator(entry, "leap_load"), "reversed")));
    }
    /* the first bars of the two differ: nothing to compare */
    assert(penalty(item(both, 0), "one_rhythm") == 0 && penalty(item(both, 1), "one_rhythm") == 0);
    /* every observation names a rule, whether it is scored, and its words */
    const cJSON *observation;
    cJSON_ArrayForEach(observation, cJSON_GetObjectItemCaseSensitive(item(both, 1), "observations")) {
        assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(observation, "rule")) && cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(observation, "scored")));
        assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(observation, "message")));
        const cJSON *bar = cJSON_GetObjectItemCaseSensitive(observation, "bar");
        assert(cJSON_IsNull(bar) || cJSON_IsNumber(bar));
    }
    free(request);
    composer_response_destroy(&response);

    /* the period alone: the same evaluators, and one_rhythm of a set of one */
    request = request_of(plan, "");
    cJSON *alone = critique(request, URSATZ_OK, &response);
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(alone, "candidates")) == 1);
    static const char *const OTHERS[] = {"rhythmic_monotony", "contrast_distance", "leap_load", "open_ending", "stray_syncopation", "repeated_notes", "wide_range",
                                         "register_shift", "half_notes"};
    for (size_t i = 0; i < sizeof OTHERS / sizeof OTHERS[0]; i++) {
        assert(penalty(item(alone, 0), OTHERS[i]) == penalty(item(both, 1), OTHERS[i]));
    }
    assert(penalty(item(alone, 0), "one_rhythm") == 0);
    assert(ratio(cJSON_GetObjectItemCaseSensitive(item(alone, 0), "score")) == ratio(cJSON_GetObjectItemCaseSensitive(item(both, 1), "score")));
    cJSON_Delete(alone);
    free(request);
    composer_response_destroy(&response);

    /* the same period twice: each opens with the rhythm of the other */
    char *twice_text = joined(plan, plan);
    request = request_of(twice_text, "");
    cJSON *twice = critique(request, URSATZ_OK, &response);
    assert(penalty(item(twice, 0), "one_rhythm") == 1 && penalty(item(twice, 1), "one_rhythm") == 1);
    cJSON_Delete(twice);
    free(request);
    free(twice_text);
    composer_response_destroy(&response);

    /* the critic group: leap_load weighs 0 and wide_range counts the other way */
    request = request_of(plan, ",\"preferences\":{\"critic\":{\"weights\":{\"leap_load\":0},\"reverse\":[\"wide_range\"]}}");
    cJSON *weighed = critique(request, URSATZ_OK, &response);
    assert(cJSON_GetObjectItemCaseSensitive(evaluator(item(weighed, 0), "leap_load"), "weight")->valueint == 0);
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(evaluator(item(weighed, 0), "wide_range"), "reversed")));
    assert(penalty(item(weighed, 0), "leap_load") == penalty(item(both, 1), "leap_load"));
    assert(ratio(cJSON_GetObjectItemCaseSensitive(item(weighed, 0), "score")) >= ratio(cJSON_GetObjectItemCaseSensitive(item(both, 1), "score")));
    cJSON_Delete(weighed);
    free(request);
    composer_response_destroy(&response);
    cJSON_Delete(both);
    free(both_text);
    free(motive);
    free(plan);
}

static void test_refusals(void) {
    char *plan = candidate("plan", plan_member);
    char *motive = candidate("motive", motive_member);
    char *request = request_of("{}", "");
    refused(request, "/candidates/0");
    free(request);
    request = request_of(motive, "");
    refused("{\"schema\":\"ursatz-critique-request\",\"schema_version\":1,\"candidates\":[]}", "/candidates");
    refused("{\"schema\":\"ursatz-critique-request\",\"schema_version\":1}", "/candidates");
    refused("{\"schema\":\"ursatz-theme-request\",\"schema_version\":1,\"candidates\":[]}", "/schema");
    refused("{\"schema\":\"ursatz-critique-request\",\"schema_version\":2,\"candidates\":[]}", "/schema_version");
    free(request);
    char *both = joined(plan, motive);
    request = request_of(both, "");
    free(both);
    char *two = joined(plan, "{\"plan\":{},\"motive\":{}}");
    free(request);
    request = request_of(two, "");
    refused(request, "/candidates/1");
    free(request);
    free(two);
    refused("{\"schema\":\"ursatz-critique-request\",\"schema_version\":1,\"candidates\":[{\"plan\":{\"notes\":1}}]}", "/candidates/0/plan");
    refused("{\"schema\":\"ursatz-critique-request\",\"schema_version\":1,\"candidates\":[{\"motive\":{\"notes\":5}}]}", "/candidates/0/motive");
    refused("{\"schema\":\"ursatz-critique-request\",\"schema_version\":1,\"candidates\":[{\"motive\":\"C4\"}]}", "/candidates/0/motive");
    char *three = joined(motive, plan);
    char *wrong = joined(three, "{\"motive\":{\"meter\":1}}");
    request = request_of(wrong, "");
    refused(request, "/candidates/2/motive");
    free(request);
    free(wrong);
    free(three);
    /* more than 16 candidates */
    size_t size = 17 * (strlen(motive) + 2) + 256;
    char *many = calloc(1, size);
    assert(many);
    for (int i = 0; i < 17; i++) {
        strcat(many, i ? "," : "");
        strcat(many, motive);
    }
    request = request_of(many, "");
    refused(request, "/candidates");
    free(request);
    free(many);
    /* a setting out of its limit or unknown is refused at its path under /preferences */
    request = request_of(motive, ",\"preferences\":{\"critic\":{\"candidates\":9}}");
    refused(request, "/preferences/critic/candidates");
    free(request);
    request = request_of(motive, ",\"preferences\":{\"critic\":{\"weights\":{\"colour\":1}}}");
    refused(request, "/preferences/critic/weights/colour");
    free(request);
    refused("not json", "");
    free(plan);
    free(motive);
}

int main(void) {
    load_candidates();
    test_sets();
    test_refusals();
    assert(written == 4);
    printf("test_api_critique: ok\n");
    free(plan_member);
    free(motive_member);
    return 0;
}
