#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <plan/plan.h>
#include <plan/plan_json.h>
#include <plan/plan_lanes.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/harmony_phrase.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_modal_theme.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The chord text of a blueprint (R-744), through the theme API: a one-motive sentence with given chords in one bar. A lower-case numeral
 * names a minor triad and is read like a borrowed chord where the key's own differs, an applied chord is the applied numeral in the plan, maj7 is
 * the seventh 7 where the key's seventh is major, and sus2 and sus4 chords sound no third. The refusals name the bar.
 */

#define MAX_CHORDS 64

typedef struct {
    UrsatzStatus status;
    char path[128];
    char message[512];
    Plan *plan;
} Composed;

/* Composes a theme in `key` and `mode` ("major", "minor") or, when `material` is not NULL, in that church mode, with the chord text on one bar. */
static Composed compose(const char *key, const char *mode, const char *material, long bar, const char *text_of_bar) {
    Composed result;
    memset(&result, 0, sizeof(result));
    cJSON *request = fixture_json("theme/one-motive.json");
    cJSON_ReplaceItemInObject(request, "key", cJSON_CreateString(key));
    if (material != NULL) {
        cJSON_DeleteItemFromObject(request, "mode");
        cJSON_AddStringToObject(request, "material", material);
    } else {
        cJSON_ReplaceItemInObject(request, "mode", cJSON_CreateString(mode));
    }
    cJSON *chords = cJSON_AddArrayToObject(request, "chords");
    cJSON *chord = cJSON_CreateObject();
    cJSON_AddNumberToObject(chord, "bar", bar);
    cJSON_AddStringToObject(chord, "text", text_of_bar);
    cJSON_AddItemToArray(chords, chord);
    cJSON_AddNumberToObject(request, "seed", 1);
    char *text = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    result.status = composer_api_theme(test_context(), text, NULL, &response);
    free(text);
    if (result.status == URSATZ_OK) {
        PlanJsonError error;
        assert(response.plan_json != NULL);
        assert(plan_from_json(test_knowledge(), response.plan_json, composer_plan_read_hooks(), &result.plan, &error) == PLAN_OK);
    } else {
        snprintf(result.path, sizeof(result.path), "%s", response.error.path);
        snprintf(result.message, sizeof(result.message), "%s", response.error.message);
    }
    composer_response_destroy(&response);
    return result;
}

/* The numeral of the plan's harmony span that starts at `beat` of bar 0 (4/4, beats from 0), or NULL. */
static void expect_ok(const Composed *result) {
    if (result->status != URSATZ_OK) {
        fprintf(stderr, "refused (%d) at '%s': %s\n", (int)result->status, result->path, result->message);
    }
    assert(result->status == URSATZ_OK);
}

/* The numeral of the plan's harmony span that starts at `beat` of `bar` (4/4, both from 0), or NULL. */
static const char *numeral_at(const Plan *plan, long bar, long beat) {
    static LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, plan_root(plan)), ids, MAX_CHORDS);
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        if (span.start.num * 4 == (bar * 4 + beat) * span.start.den) {
            return plan_lane_span_payload(plan, ids[i])->harmony.roman_numeral;
        }
    }
    return NULL;
}

/* The intervals above the root of the chord a numeral names in the key, and its root's pitch class. */
static size_t chord_intervals(const SpelledKey *key, const char *numeral, int *root, int intervals[COMPOSER_CHORD_TONES_MAX]) {
    HarmonyKind kind = harmony_kind(test_composer_chords(), key);
    HarmonyChord chord;
    size_t count = 0;
    assert(harmony_roman_parse_in(&kind, numeral, &chord));
    assert(composer_chord_tones(test_composer_chords(), key, &chord, root, intervals, &count));
    return count;
}

static void expect_refused(Composed result, const char *message) {
    if (result.status != URSATZ_ERR_REQUEST || strcmp(result.path, "/chords/0") != 0 || !strstr(result.message, message)) {
        fprintf(stderr, "expected '%s', got status %d at '%s': '%s'\n", message, (int)result.status, result.path, result.message);
    }
    assert(result.status == URSATZ_ERR_REQUEST);
    assert(strcmp(result.path, "/chords/0") == 0);
    assert(strstr(result.message, message));
}

static void test_applied(void) {
    Composed result = compose("C", "major", NULL, 1, "I V7/V@2");
    expect_ok(&result);
    assert(strcmp(numeral_at(result.plan, 1, 0), "I") == 0);
    assert(strcmp(numeral_at(result.plan, 1, 2), "V7/V") == 0);
    plan_destroy(result.plan);
    /* A target the library does not tonicize, an applied chord in a modal key. */
    expect_refused(compose("C", "major", NULL, 1, "I V7/I@2"), "is not an applied chord the Composer writes");
    expect_refused(compose("C", NULL, "dorian", 1, "I V7/V@2"), "applied chords are not given in a modal key");
}

static void test_minor_triads(void) {
    SpelledKey c_minor = test_key(0, "minor");
    int root;
    int intervals[COMPOSER_CHORD_TONES_MAX];
    /* v in C minor is G Bb D, with no B natural. */
    Composed result = compose("C", "minor", NULL, 1, "v");
    expect_ok(&result);
    const char *numeral = numeral_at(result.plan, 1, 0);
    assert(numeral != NULL && strcmp(numeral, "v") == 0);
    assert(chord_intervals(&c_minor, numeral, &root, intervals) == 3 && root == 7 && intervals[1] == 3 && intervals[2] == 7);
    plan_destroy(result.plan);
    /* #vii in C minor is B D F#. */
    result = compose("C", "minor", NULL, 0, "i #vii@2");
    expect_ok(&result);
    numeral = numeral_at(result.plan, 0, 2);
    assert(numeral != NULL && strcmp(numeral, "#vii") == 0);
    assert(chord_intervals(&c_minor, numeral, &root, intervals) == 3 && root == 11 && intervals[1] == 3 && intervals[2] == 7);
    plan_destroy(result.plan);
    /* The key's own quality is the plain chord: iv in C minor. */
    result = compose("C", "minor", NULL, 0, "i iv@2");
    expect_ok(&result);
    assert(strcmp(numeral_at(result.plan, 0, 2), "iv") == 0);
    plan_destroy(result.plan);
}

static void test_major_seventh(void) {
    Composed result = compose("E", "minor", NULL, 1, "i VImaj7@2");
    expect_ok(&result);
    assert(strcmp(numeral_at(result.plan, 1, 2), "VI7") == 0);
    plan_destroy(result.plan);
    expect_refused(compose("C", "minor", NULL, 1, "i IVmaj7@2"), "is not this key's seventh chord");
}

static void test_sus(void) {
    SpelledKey d_minor = test_key(2, "minor");
    int root;
    int intervals[COMPOSER_CHORD_TONES_MAX];
    Composed result = compose("D", "minor", NULL, 1, "Vsus4 V@2");
    expect_ok(&result);
    const char *numeral = numeral_at(result.plan, 1, 0);
    assert(numeral != NULL && strcmp(numeral, "Vsus4") == 0);
    /* A fourth and a fifth above the root, no third. */
    assert(chord_intervals(&d_minor, numeral, &root, intervals) == 3 && intervals[0] == 0 && intervals[1] == 5 && intervals[2] == 7);
    plan_destroy(result.plan);
    /* A lower-case color chord on the key's own minor triad composes; the major type on it does not fit, as it never did. */
    result = compose("D", "minor", NULL, 0, "iadd9");
    expect_ok(&result);
    assert(strcmp(numeral_at(result.plan, 0, 0), "iadd9") == 0);
    plan_destroy(result.plan);
    expect_refused(compose("D", "minor", NULL, 0, "Iadd9"), "does not fit the key");
    /* sus2 on the second degree of D major: a second and a fifth above the root. */
    SpelledKey d_major = test_key(2, "major");
    result = compose("D", "major", NULL, 1, "iisus2");
    expect_ok(&result);
    numeral = numeral_at(result.plan, 1, 0);
    assert(numeral != NULL && strcmp(numeral, "iisus2") == 0);
    assert(chord_intervals(&d_major, numeral, &root, intervals) == 3 && intervals[1] == 2 && intervals[2] == 7);
    plan_destroy(result.plan);
}

int main(void) {
    test_applied();
    test_minor_triads();
    test_major_seventh();
    test_sus();
    printf("test_blueprint_chords: ok\n");
    return 0;
}
