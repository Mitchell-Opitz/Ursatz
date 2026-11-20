#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/motive.h>

#include "motive/motive_random_internal.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

/*
 * Variety (R-673): the candidates group of the preferences and the random motive's avoid. Off by default: variety 0 gives the
 * motives of a request without preferences. With variety 20 the four motives of seed 1 are at least as far apart as with
 * variety 0 (by motive_random_distance, the measure the draw uses), a motive of avoid is not among those kept, and the bad
 * requests are refused at their paths.
 */

#define COUNT 4

static char *random_request(unsigned long seed, const char *candidates, const char *avoid) {
    char *text = malloc(8192 + (avoid ? strlen(avoid) : 0));
    assert(text);
    snprintf(text, 8192 + (avoid ? strlen(avoid) : 0),
             "{\"mode\":\"major\",\"meter\":\"4/4\",\"count\":%d,\"seed\":%lu%s%s%s%s%s}", COUNT, seed,
             candidates ? ",\"preferences\":{\"candidates\":" : "", candidates ? candidates : "", candidates ? "}" : "",
             avoid ? ",\"avoid\":" : "", avoid ? avoid : "");
    return text;
}

/* The motives of a response, parsed (count of them); the response is destroyed. */
static size_t read_motives(ComposerResponse *response, Motive *out, size_t capacity) {
    cJSON *document = cJSON_Parse(response->json);
    assert(document);
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(document, "motives");
    size_t count = (size_t)cJSON_GetArraySize(list);
    assert(count <= capacity);
    for (size_t i = 0; i < count; i++) {
        char *text = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(list, i), "motive"));
        MotiveError error;
        assert(text && motive_from_json(text, test_motive_options(), &out[i], &error) == MOTIVE_OK);
        cJSON_free(text);
    }
    cJSON_Delete(document);
    composer_response_destroy(response);
    return count;
}

static size_t draw(unsigned long seed, const char *candidates, const char *avoid, Motive *out, size_t capacity) {
    ComposerResponse response;
    char *request = random_request(seed, candidates, avoid);
    assert(composer_api_random_motive(test_context(), request, NULL, &response) == URSATZ_OK);
    free(request);
    return read_motives(&response, out, capacity);
}

/* The smallest distance between two of the motives. */
static double smallest_distance(const Motive *motives, size_t count) {
    double smallest = 1.0;
    for (size_t i = 0; i < count; i++) {
        for (size_t k = i + 1; k < count; k++) {
            double apart = motive_random_distance(&motives[i], &motives[k]);
            smallest = apart < smallest ? apart : smallest;
        }
    }
    return smallest;
}

static void refused(const char *request, const char *path) {
    ComposerResponse response;
    UrsatzStatus status = composer_api_random_motive(test_context(), request, NULL, &response);
    if (status == URSATZ_OK || strcmp(response.error.path, path) != 0 || response.error.message[0] == '\0') {
        fprintf(stderr, "%s: expected a refusal at %s, got %d at \"%s\"\n", request, path, (int)status, response.error.path);
        assert(false);
    }
    composer_response_destroy(&response);
}

static void variety_zero_changes_nothing(void) {
    Motive plain[COUNT];
    Motive explicit_zero[COUNT];
    assert(draw(1, NULL, NULL, plain, COUNT) == COUNT);
    assert(draw(1, "{\"variety\":0}", NULL, explicit_zero, COUNT) == COUNT);
    for (size_t i = 0; i < COUNT; i++) {
        assert(motive_equal(&plain[i], &explicit_zero[i]));
    }
    /* The pool alone does nothing either, and a given avoid is not read at variety 0. */
    Motive pooled[COUNT];
    assert(draw(1, "{\"pool\":8}", NULL, pooled, COUNT) == COUNT);
    for (size_t i = 0; i < COUNT; i++) {
        assert(motive_equal(&plain[i], &pooled[i]));
    }
}

static void variety_spreads_the_motives(void) {
    /* Over seeds 1 to 8 the smallest pairwise distance of the 4 motives with variety 20 is never below variety 0's (measured:
       seed 2 has two alike at variety 0, seed 7 two close), and some motive is not the plain draw's. */
    size_t changed = 0;
    for (unsigned long seed = 1; seed <= 8; seed++) {
        Motive plain[COUNT];
        Motive varied[COUNT];
        assert(draw(seed, NULL, NULL, plain, COUNT) == COUNT);
        assert(draw(seed, "{\"variety\":20,\"pool\":8}", NULL, varied, COUNT) == COUNT);
        double plain_smallest = smallest_distance(plain, COUNT);
        double varied_smallest = smallest_distance(varied, COUNT);
        fprintf(stderr, "seed %lu smallest pairwise distance: variety 0 %.3f, variety 20 %.3f\n", seed, plain_smallest, varied_smallest);
        assert(varied_smallest >= plain_smallest);
        for (size_t i = 0; i < COUNT; i++) {
            changed += !motive_equal(&plain[i], &varied[i]);
        }
        /* The same request twice gives the same motives. */
        Motive again[COUNT];
        assert(draw(seed, "{\"variety\":20,\"pool\":8}", NULL, again, COUNT) == COUNT);
        for (size_t i = 0; i < COUNT; i++) {
            assert(motive_equal(&varied[i], &again[i]));
        }
    }
    assert(changed > 0);
}

static void avoided_motives_are_not_kept(void) {
    Motive plain[COUNT];
    assert(draw(1, "{\"variety\":20}", NULL, plain, COUNT) == COUNT);
    /* Avoid the first two motives a varied draw gives: none of the new four equals either, nor any of the four. */
    ComposerResponse response;
    char *request = random_request(1, "{\"variety\":20}", NULL);
    assert(composer_api_random_motive(test_context(), request, NULL, &response) == URSATZ_OK);
    free(request);
    cJSON *document = cJSON_Parse(response.json);
    assert(document);
    cJSON *list = cJSON_GetObjectItemCaseSensitive(document, "motives");
    cJSON *avoid = cJSON_CreateArray();
    for (int i = 0; i < 2; i++) {
        cJSON_AddItemToArray(avoid, cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(list, i), "motive"), true));
    }
    char *avoid_text = cJSON_PrintUnformatted(avoid);
    cJSON_Delete(avoid);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    Motive kept[COUNT];
    assert(draw(1, "{\"variety\":20}", avoid_text, kept, COUNT) == COUNT);
    free(avoid_text);
    for (size_t i = 0; i < COUNT; i++) {
        assert(!motive_equal(&kept[i], &plain[0]) && !motive_equal(&kept[i], &plain[1]));
    }
}

static void bad_requests_are_refused(void) {
    refused("{\"mode\":\"major\",\"meter\":\"4/4\",\"preferences\":{\"candidates\":{\"pool\":1}}}", "/preferences/candidates/pool");
    refused("{\"mode\":\"major\",\"meter\":\"4/4\",\"preferences\":{\"candidates\":{\"pool\":9}}}", "/preferences/candidates/pool");
    refused("{\"mode\":\"major\",\"meter\":\"4/4\",\"preferences\":{\"candidates\":{\"variety\":21}}}",
            "/preferences/candidates/variety");
    refused("{\"mode\":\"major\",\"meter\":\"4/4\",\"avoid\":\"nothing\"}", "/avoid");
    refused("{\"mode\":\"major\",\"meter\":\"4/4\",\"avoid\":[{\"notes\":\"not a motive\"}]}", "/avoid/0");
    /* 65 entries are too many. */
    char many[2048] = "[";
    for (int i = 0; i < 65; i++) {
        strcat(many, i ? ",{}" : "{}");
    }
    strcat(many, "]");
    char request[4096];
    snprintf(request, sizeof request, "{\"mode\":\"major\",\"meter\":\"4/4\",\"avoid\":%s}", many);
    refused(request, "/avoid");
}

int main(void) {
    variety_zero_changes_nothing();
    variety_spreads_the_motives();
    avoided_motives_are_not_kept();
    bad_requests_are_refused();
    printf("test_motif_variety: ok\n");
    return 0;
}
