#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_support.h"

#ifndef COMPOSER_SNAPSHOT_DIR
#error "COMPOSER_SNAPSHOT_DIR is not defined"
#endif
#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The variant operations, variants, recognize and random motive entries (docs/api.md) against the shapes the GUI runs: each
 * request is the arguments of a CLI snapshot (tests/cli_snapshots.cmake), and its response is that snapshot's JSON, compared
 * as a value as the snapshot test does. The requests and responses are written beside the test, for the schema test.
 */

#define GUI COMPOSER_FIXTURE_DIR "/gui/"

static void write_pair(const char *kind, const char *tag, const char *request, const ComposerResponse *response) {
    char path[160];
    if (request) {
        snprintf(path, sizeof path, "api_%s_request_%s.json", kind, tag);
        test_write_file(path, request);
    }
    snprintf(path, sizeof path, "api_%s_response_%s.json", kind, tag);
    test_write_file(path, response->json);
}

static void expect_snapshot(const ComposerResponse *response, const char *snapshot) {
    char path[256];
    snprintf(path, sizeof path, COMPOSER_SNAPSHOT_DIR "/%s", snapshot);
    char *expected = test_read_file(path, NULL);
    cJSON *actual_json = cJSON_Parse(response->json);
    cJSON *expected_json = cJSON_Parse(expected);
    assert(actual_json && expected_json);
    if (!cJSON_Compare(actual_json, expected_json, true)) {
        fprintf(stderr, "the response differs from %s:\n%s\n", snapshot, response->json);
    }
    assert(cJSON_Compare(actual_json, expected_json, true));
    cJSON_Delete(actual_json);
    cJSON_Delete(expected_json);
    free(expected);
}

/* The request text: the members of options (a JSON object text) and, for each named part, "input" with the file's text. */
static cJSON *input_object(const char *file, const char *format) {
    cJSON *part = cJSON_CreateObject();
    char *text = test_read_file(file, NULL);
    cJSON *input = cJSON_AddObjectToObject(part, "input");
    assert(input && cJSON_AddStringToObject(input, "text", text));
    if (format) {
        assert(cJSON_AddStringToObject(input, "format", format));
    }
    free(text);
    return part;
}

static char *print(cJSON *request) {
    char *text = cJSON_PrintUnformatted(request);
    assert(text);
    cJSON_Delete(request);
    return text;
}

/* The GUI's variants request (variants-basic): a degree list, a basic motive, two functions, a distance, a count and a seed. */
static char *variants_basic_request(const char *extra_options) {
    cJSON *request = input_object(GUI "variants-source.txt", "degrees");
    cJSON *options = cJSON_Parse(extra_options);
    assert(request && options);
    for (cJSON *item = options->child; item; item = item->next) {
        assert(cJSON_AddItemToObject(request, item->string, cJSON_Duplicate(item, true)));
    }
    cJSON_Delete(options);
    assert(cJSON_AddStringToObject(request, "meter", "4/4") && cJSON_AddStringToObject(request, "key", "C major") &&
           cJSON_AddNumberToObject(request, "pickup", 0));
    assert(cJSON_AddItemToObject(request, "basic", input_object(GUI "variants-basic.txt", "degrees")));
    return print(request);
}

static void test_operations(void) {
    ComposerResponse response;
    assert(composer_api_variant_operations(&response) == COMPOSER_OK);
    assert(response.json && !response.plan_json && !response.midi && !response.musicxml);
    expect_snapshot(&response, "variants-list-ops.stdout.json");
    write_pair("variant_operations", "all", NULL, &response);
    composer_response_destroy(&response);
}

static void test_variants(void) {
    ComposerResponse response;
    const char *const gui = "{\"functions\":[\"beginning\",\"continuation\"],\"distance\":\"medium\",\"count\":4,\"seed\":3,"
                            "\"api_version\":1}";
    char *request = variants_basic_request(gui);
    assert(composer_api_variants(request, NULL, &response) == COMPOSER_OK);
    expect_snapshot(&response, "variants-basic.stdout.json");
    write_pair("variants", "basic", request, &response);
    composer_response_destroy(&response);
    free(request);

    /* Every operation that applies, one step each. */
    cJSON *each = input_object(COMPOSER_FIXTURE_DIR "/motive_d_minor.json", "json");
    assert(cJSON_AddTrueToObject(each, "each_op") && cJSON_AddNumberToObject(each, "seed", 1));
    request = print(each);
    assert(composer_api_variants(request, NULL, &response) == COMPOSER_OK);
    expect_snapshot(&response, "variants-each-op.stdout.json");
    write_pair("variants", "each_op", request, &response);
    composer_response_destroy(&response);
    free(request);

    /* The same chain for every variant, with the count and the seed the CLI gives: variant i uses seed + i. */
    cJSON *chain = input_object(COMPOSER_FIXTURE_DIR "/motive_d_minor.txt", "notes");
    assert(cJSON_AddStringToObject(chain, "meter", "4/4") && cJSON_AddStringToObject(chain, "key", "D minor") &&
           cJSON_AddNumberToObject(chain, "count", 2) && cJSON_AddNumberToObject(chain, "seed", 1));
    cJSON *steps = cJSON_CreateObject();
    assert(cJSON_AddItemToObject(chain, "chain", steps));
    assert(cJSON_AddItemToObject(steps, "steps", cJSON_Parse("[{\"op\":\"invert\",\"params\":{\"tonal\":true}},"
                                                             "{\"op\":\"augment\",\"params\":{\"ratio\":\"2:1\"}}]")));
    request = print(chain);
    assert(composer_api_variants(request, NULL, &response) == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response.json);
    const cJSON *variants = cJSON_GetObjectItemCaseSensitive(document, "variants");
    assert(cJSON_GetArraySize(variants) == 2);
    assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(variants, 1), "seed")->valuedouble == 2);
    assert(!cJSON_GetObjectItemCaseSensitive(document, "notices"));
    write_pair("variants", "chain", request, &response);
    cJSON_Delete(document);
    composer_response_destroy(&response);

    /* The same request gives the same variants, and the exact figures are asked for. */
    ComposerResponse again;
    assert(composer_api_variants(request, NULL, &response) == COMPOSER_OK);
    assert(composer_api_variants(request, NULL, &again) == COMPOSER_OK);
    assert(strcmp(response.json, again.json) == 0);
    composer_response_destroy(&response);
    composer_response_destroy(&again);
    free(request);
    request = variants_basic_request("{\"count\":1,\"seed\":3,\"detail\":true}");
    assert(composer_api_variants(request, NULL, &response) == COMPOSER_OK);
    assert(strstr(response.json, "\"remoteness_fraction\"") && strstr(response.json, "\"leaves_harmony\""));
    composer_response_destroy(&response);
    free(request);
}

static void test_recognize(void) {
    ComposerResponse response;
    cJSON *request = cJSON_CreateObject();
    assert(cJSON_AddItemToObject(request, "source", input_object(GUI "recognize-source.txt", "degrees")));
    assert(cJSON_AddItemToObject(request, "target", input_object(GUI "recognize-target.txt", "degrees")));
    assert(cJSON_AddStringToObject(request, "meter", "4/4") && cJSON_AddStringToObject(request, "key", "C major") &&
           cJSON_AddNumberToObject(request, "pickup", 0));
    char *text = print(request);
    assert(composer_api_recognize(text, NULL, &response) == COMPOSER_OK);
    expect_snapshot(&response, "recognize.stdout.json");
    write_pair("recognize", "gui", text, &response);
    composer_response_destroy(&response);

    /* The meter both motives are read in is required, and a bad one is named at the request's own path. */
    cJSON *bad = cJSON_Parse(text);
    cJSON_DeleteItemFromObjectCaseSensitive(bad, "meter");
    char *without = print(bad);
    assert(composer_api_recognize(without, NULL, &response) == COMPOSER_ERR_REQUEST);
    assert(strcmp(response.error.path, "/meter") == 0);
    composer_response_destroy(&response);
    free(without);
    bad = cJSON_Parse(text);
    cJSON_ReplaceItemInObjectCaseSensitive(bad, "meter", cJSON_CreateString("5/3"));
    without = print(bad);
    assert(composer_api_recognize(without, NULL, &response) == COMPOSER_ERR_REQUEST);
    assert(strcmp(response.error.path, "/meter") == 0);
    composer_response_destroy(&response);
    free(without);
    bad = cJSON_Parse(text);
    cJSON_AddItemToObject(bad, "target_pickup", cJSON_CreateString("x"));
    without = print(bad);
    assert(composer_api_recognize(without, NULL, &response) == COMPOSER_ERR_REQUEST);
    assert(strcmp(response.error.path, "/target_pickup") == 0);
    composer_response_destroy(&response);
    free(without);
    free(text);
}

/*
 * What a seed draws (docs/api.md, materials-model.js randomSettings): m = ((seed + 1) * 2654435761) mod 2^32, the mode
 * is major when (m >> 8) is even, and the meter is by (m >> 16) mod 4. The table is the formula worked by hand.
 */
static const struct {
    unsigned long seed;
    unsigned long mixed;
    const char *mode;
    const char *meter;
} DRAWS[] = {
    {0UL, 0x9e3779b1UL, "minor", "6/8"},
    {1UL, 0x3c6ef362UL, "minor", "2/4"},
    {2UL, 0xdaa66d13UL, "minor", "2/4"},
    {4294967295UL, 0x0UL, "major", "4/4"}, /* seed + 1 is 2^32: the product wraps to 0 */
};

static void test_random(void) {
    ComposerResponse response;
    char *request = print(cJSON_Parse("{\"mode\":\"major\",\"meter\":\"4/4\",\"key\":\"C major\",\"form\":\"full\","
                                      "\"role\":\"accompaniment\",\"strategy\":\"drawn\",\"count\":2,\"seed\":5}"));
    assert(composer_api_random_motive(request, NULL, &response) == COMPOSER_OK);
    expect_snapshot(&response, "random-motive-style.stdout.json");
    assert(!strstr(response.json, "\"drawn\""));
    write_pair("random_motive", "gui", request, &response);
    composer_response_destroy(&response);
    free(request);

    request = print(cJSON_Parse("{\"mode\":\"minor\",\"meter\":\"3/4\",\"strategy\":\"drawn\",\"count\":3,\"seed\":1}"));
    assert(composer_api_random_motive(request, NULL, &response) == COMPOSER_OK);
    expect_snapshot(&response, "random-motive.stdout.json");
    composer_response_destroy(&response);
    free(request);

    for (size_t i = 0; i < sizeof DRAWS / sizeof DRAWS[0]; i++) {
        /* The formula, worked in the test. */
        unsigned long long product = ((unsigned long long)DRAWS[i].seed + 1ULL) * 2654435761ULL;
        unsigned long mixed = (unsigned long)(product % 4294967296ULL);
        assert(mixed == DRAWS[i].mixed);
        assert(strcmp(((mixed >> 8) % 2) == 0 ? "major" : "minor", DRAWS[i].mode) == 0);
        assert(strcmp((const char *const[]){"4/4", "3/4", "2/4", "6/8"}[(mixed >> 16) % 4], DRAWS[i].meter) == 0);

        char text[160];
        snprintf(text, sizeof text, "{\"mode\":\"random\",\"meter\":\"random\",\"strategy\":\"drawn\",\"seed\":%lu}", DRAWS[i].seed);
        assert(composer_api_random_motive(text, NULL, &response) == COMPOSER_OK);
        cJSON *document = cJSON_Parse(response.json);
        const cJSON *drawn = cJSON_GetObjectItemCaseSensitive(document, "drawn");
        assert(drawn && strcmp(cJSON_GetObjectItemCaseSensitive(drawn, "mode")->valuestring, DRAWS[i].mode) == 0);
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(drawn, "meter")->valuestring, DRAWS[i].meter) == 0);
        /* The motive is in the mode and the meter drawn. */
        const cJSON *motive = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "motives"), 0), "motive");
        const cJSON *key = cJSON_GetObjectItemCaseSensitive(motive, "key");
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(key, "mode")->valuestring, DRAWS[i].mode) == 0);
        assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(motive, "meter"), "beats_per_bar")->valueint ==
               atoi(DRAWS[i].meter));
        if (i == 0) {
            write_pair("random_motive", "drawn", text, &response);
        }
        cJSON_Delete(document);
        composer_response_destroy(&response);
    }

    /* Only what was left to chance is reported, and the same seed draws the same motives. */
    ComposerResponse again;
    assert(composer_api_random_motive("{\"mode\":\"random\",\"meter\":\"4/4\",\"seed\":1}", NULL, &response) == COMPOSER_OK);
    assert(composer_api_random_motive("{\"mode\":\"random\",\"meter\":\"4/4\",\"seed\":1}", NULL, &again) == COMPOSER_OK);
    assert(strcmp(response.json, again.json) == 0);
    cJSON *only = cJSON_Parse(response.json);
    const cJSON *drawn_only = cJSON_GetObjectItemCaseSensitive(only, "drawn");
    assert(drawn_only && cJSON_GetObjectItemCaseSensitive(drawn_only, "mode") && !cJSON_GetObjectItemCaseSensitive(drawn_only, "meter"));
    cJSON_Delete(only);
    composer_response_destroy(&response);
    composer_response_destroy(&again);
}

/* A request that is refused names the member at fault. */
static void expect_refused(ComposerStatus (*entry)(const char *, const ComposerJob *, ComposerResponse *), const char *request,
                           ComposerStatus status, const char *path) {
    ComposerResponse response;
    ComposerStatus got = entry(request, NULL, &response);
    if (got != status || strcmp(response.error.path, path) != 0) {
        fprintf(stderr, "%s: status %d path '%s' message '%s'\n", request, got, response.error.path, response.error.message);
    }
    assert(got == status && strcmp(response.error.path, path) == 0);
    assert(response.error.message[0] != '\0' && !response.json);
    composer_response_destroy(&response);
}

/* The strategy member: segments is the default and gives segments and a given harmony; drawn gives neither. */
static void test_random_strategy(void) {
    static const char *const REQUESTS[] = {
        "{\"mode\":\"major\",\"meter\":\"4/4\",\"count\":3,\"seed\":4}",
        "{\"mode\":\"major\",\"meter\":\"4/4\",\"strategy\":\"segments\",\"count\":3,\"seed\":4}",
        "{\"mode\":\"major\",\"meter\":\"4/4\",\"strategy\":\"drawn\",\"count\":3,\"seed\":4}",
    };
    char *documents[3];
    for (size_t r = 0; r < 3; r++) {
        ComposerResponse response;
        assert(composer_api_random_motive(REQUESTS[r], NULL, &response) == COMPOSER_OK);
        documents[r] = strdup(response.json);
        cJSON *document = cJSON_Parse(response.json);
        const cJSON *motives = cJSON_GetObjectItemCaseSensitive(document, "motives");
        assert(cJSON_GetArraySize(motives) == 3);
        const cJSON *motive = NULL;
        cJSON_ArrayForEach(motive, motives) {
            const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(motive, "analysis");
            const cJSON *segments = cJSON_GetObjectItemCaseSensitive(analysis, "segments");
            const cJSON *base = cJSON_GetObjectItemCaseSensitive(analysis, "base_harmony");
            const cJSON *items = cJSON_GetObjectItemCaseSensitive(segments, "items");
            assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(segments, "given")) == (r < 2));
            assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(base, "given")) == (r < 2));
            assert(cJSON_GetArraySize(items) >= 1 &&
                   strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(items, 0), "label")->valuestring, "a") == 0);
            assert(r == 2 || cJSON_GetArraySize(items) >= 2);
            const cJSON *stored = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(motive, "motive"), "features");
            assert((r < 2) == (stored && cJSON_GetArraySize(stored) >= 2));
        }
        cJSON_Delete(document);
        composer_response_destroy(&response);
    }
    assert(strcmp(documents[0], documents[1]) == 0 && strcmp(documents[0], documents[2]) != 0);
    for (size_t r = 0; r < 3; r++) {
        free(documents[r]);
    }
}

static void test_refusals(void) {
    expect_refused(composer_api_random_motive, "{\"mode\":\"major\",\"meter\":\"4/4\",\"style\":\"no-such-style.json\"}",
                   COMPOSER_ERR_UNSATISFIABLE, "/style");
    expect_refused(composer_api_random_motive, "{\"mode\":\"major\",\"meter\":\"4/4\",\"style_without\":[\"duration\"]}",
                   COMPOSER_ERR_REQUEST, "/style_without");
    expect_refused(composer_api_random_motive, "{\"mode\":\"dorian\",\"meter\":\"4/4\"}", COMPOSER_ERR_REQUEST, "/mode");
    expect_refused(composer_api_random_motive, "{\"mode\":\"major\"}", COMPOSER_ERR_REQUEST, "/meter");
    expect_refused(composer_api_random_motive, "{\"mode\":\"major\",\"meter\":\"4\"}", COMPOSER_ERR_REQUEST, "/meter");
    expect_refused(composer_api_random_motive, "{\"mode\":\"major\",\"meter\":\"4/4\",\"key\":\"D minor\"}", COMPOSER_ERR_REQUEST,
                   "/key");
    expect_refused(composer_api_random_motive, "{\"mode\":\"major\",\"meter\":\"4/4\",\"count\":0}", COMPOSER_ERR_REQUEST,
                   "/count");
    expect_refused(composer_api_random_motive, "{\"mode\":\"major\",\"meter\":\"4/4\",\"api_version\":7}", COMPOSER_ERR_REQUEST,
                   "/api_version");
    expect_refused(composer_api_random_motive, "{\"mode\":\"major\",\"meter\":\"4/4\",\"strategy\":\"dice\"}", COMPOSER_ERR_REQUEST,
                   "/strategy");

    const char *const input = "\"input\":{\"text\":\"1:q 3:e 5:e 1':h\",\"format\":\"degrees\"},\"meter\":\"4/4\",\"key\":\"C major\"";
    char request[512];
    snprintf(request, sizeof request, "{%s,\"count\":0}", input);
    expect_refused(composer_api_variants, request, COMPOSER_ERR_REQUEST, "/count");
    snprintf(request, sizeof request, "{%s,\"each_op\":true,\"count\":2}", input);
    expect_refused(composer_api_variants, request, COMPOSER_ERR_REQUEST, "/each_op");
    snprintf(request, sizeof request, "{%s,\"distance\":\"near\"}", input);
    expect_refused(composer_api_variants, request, COMPOSER_ERR_REQUEST, "/distance");
    snprintf(request, sizeof request, "{%s,\"functions\":[\"beginning\",\"dramatic\"]}", input);
    expect_refused(composer_api_variants, request, COMPOSER_ERR_REQUEST, "/functions/1");
    snprintf(request, sizeof request, "{%s,\"chain\":{\"steps\":[{\"op\":\"spin\"}]}}", input);
    expect_refused(composer_api_variants, request, COMPOSER_ERR_REQUEST, "/chain/steps/0");
    snprintf(request, sizeof request, "{%s,\"chain\":{\"steps\":[{\"op\":\"invert\",\"params\":{\"by\":1}}]}}", input);
    expect_refused(composer_api_variants, request, COMPOSER_ERR_REQUEST, "/chain/steps/0/params/by");
    snprintf(request, sizeof request, "{%s,\"basic\":{\"input\":{\"text\":\"1:q 9:e\",\"format\":\"degrees\"}}}", input);
    expect_refused(composer_api_variants, request, COMPOSER_ERR_UNSATISFIABLE, "/basic/input/text");
    snprintf(request, sizeof request, "{%s,\"seed\":4294967295}", input);
    expect_refused(composer_api_variants, request, COMPOSER_ERR_REQUEST, "/seed");
    expect_refused(composer_api_variants, "{\"meter\":\"4/4\"}", COMPOSER_ERR_REQUEST, "/input");
    expect_refused(composer_api_recognize, "{\"meter\":\"4/4\",\"source\":{}}", COMPOSER_ERR_REQUEST, "/source/input");
}

typedef struct {
    unsigned steps;
    unsigned cancel_at;
    unsigned last_done;
    bool decreased;
    char stage[32];
} Job;

static void progress(void *user, const char *stage, unsigned done, unsigned total) {
    Job *job = user;
    assert(done <= total);
    if (strcmp(job->stage, stage) == 0 && done < job->last_done) {
        job->decreased = true;
    }
    snprintf(job->stage, sizeof job->stage, "%s", stage);
    job->last_done = done;
    job->steps++;
}

static bool cancelled(void *user) {
    const Job *job = user;
    return job->cancel_at != 0 && job->steps >= job->cancel_at;
}

static void test_cancel(void) {
    char *request = variants_basic_request("{\"count\":4,\"seed\":3}");
    ComposerResponse response;

    /* Progress once for each variant tried; a run that is not cancelled gives the response it gives with no job. */
    Job counting = {0, 0, 0, false, ""};
    ComposerJob job = {progress, cancelled, &counting};
    assert(composer_api_variants(request, &job, &response) == COMPOSER_OK);
    assert(strcmp(counting.stage, "variants") == 0 && counting.steps >= 4 && !counting.decreased);
    ComposerResponse plain;
    assert(composer_api_variants(request, NULL, &plain) == COMPOSER_OK);
    assert(strcmp(response.json, plain.json) == 0);
    composer_response_destroy(&response);
    composer_response_destroy(&plain);

    /* A cancel at the third variant stops there: no response, only the error. */
    Job stopping = {0, 3, 0, false, ""};
    job.user = &stopping;
    assert(composer_api_variants(request, &job, &response) == COMPOSER_ERR_CANCELLED);
    assert(!response.json && response.error.message[0] != '\0');
    assert(stopping.steps == 3 && strcmp(stopping.stage, "variants") == 0);
    composer_response_destroy(&response);
    free(request);

    /* Every operation: cancelled at the first. */
    cJSON *each = input_object(COMPOSER_FIXTURE_DIR "/motive_d_minor.json", "json");
    assert(cJSON_AddTrueToObject(each, "each_op"));
    request = print(each);
    Job first = {0, 1, 0, false, ""};
    job.user = &first;
    assert(composer_api_variants(request, &job, &response) == COMPOSER_ERR_CANCELLED);
    composer_response_destroy(&response);
    free(request);
}

int main(void) {
    assert(strcmp(composer_api_version(), "1.6") == 0);
    test_operations();
    test_variants();
    test_recognize();
    test_random();
    test_random_strategy();
    test_refusals();
    test_cancel();
    printf("test_api_variants: ok\n");
    return 0;
}
