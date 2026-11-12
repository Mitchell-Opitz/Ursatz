#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_support.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Validating a theme request (B3 of P33) builds its plan without seeding and refuses what Compose refuses before seeding:
 * each refusal of the plan has the path of its field and a message, and Compose gives the same path and message. A valid
 * request validates quickly.
 */

#define THEME(name) COMPOSER_FIXTURE_DIR "/theme/" name

#define MOTIVE_A "{\"id\": \"A\", \"form\": \"full\", \"degrees\": \"%s\", \"pickup\": \"0\", \"harmony\": null, \"features\": [], \"role\": \"melodic\"}"

typedef struct {
    const char *fixture;           /* the request to start from */
    const char *const (*edits)[2]; /* {member, JSON text or NULL to remove} */
    size_t count;
} Request;

static char *request_text(const char *fixture, const char *const edits[][2], size_t count) {
    char *text = test_read_file(fixture, NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request);
    for (size_t i = 0; i < count; i++) {
        cJSON_DeleteItemFromObjectCaseSensitive(request, edits[i][0]);
        if (edits[i][1]) {
            cJSON *item = cJSON_Parse(edits[i][1]);
            assert(item && cJSON_AddItemToObject(request, edits[i][0], item));
        }
    }
    assert(cJSON_AddNumberToObject(request, "seed", 1));
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    assert(printed);
    return printed;
}

static void expect_refused_as(ComposerStatus expected, const char *fixture, const char *const edits[][2], size_t count,
                              const char *path, const char *message);

/* The request is refused by validate at `path` with `message` in it, and composing it gives the same path and message. */
static void expect_refused(const char *fixture, const char *const edits[][2], size_t count, const char *path,
                           const char *message) {
    expect_refused_as(COMPOSER_ERR_UNSATISFIABLE, fixture, edits, count, path, message);
}

static void expect_refused_as(ComposerStatus expected, const char *fixture, const char *const edits[][2], size_t count,
                              const char *path, const char *message) {
    char *text = request_text(fixture, edits, count);
    ComposerResponse validated;
    ComposerStatus status = composer_api_validate("theme", text, &validated);
    if (status != expected || strcmp(validated.error.path, path) != 0 || !strstr(validated.error.message, message)) {
        fprintf(stderr, "validate: %s '%s' at '%s', expected '%s' at '%s'\n", composer_status_name(status),
                validated.error.message, validated.error.path, message, path);
    }
    assert(status == expected);
    assert(strcmp(validated.error.path, path) == 0);
    assert(strstr(validated.error.message, message));
    assert(!validated.json);

    ComposerResponse composed;
    ComposerStatus compose_status = composer_api_theme(text, NULL, &composed);
    if (compose_status != status || strcmp(composed.error.path, validated.error.path) != 0 ||
        strcmp(composed.error.message, validated.error.message) != 0) {
        fprintf(stderr, "compose: %s '%s' at '%s'\n", composer_status_name(compose_status), composed.error.message,
                composed.error.path);
    }
    assert(compose_status == status);
    assert(strcmp(composed.error.path, validated.error.path) == 0);
    assert(strcmp(composed.error.message, validated.error.message) == 0);
    composer_response_destroy(&validated);
    composer_response_destroy(&composed);
    free(text);
}

static void expect_valid(const char *fixture, const char *const edits[][2], size_t count) {
    char *text = request_text(fixture, edits, count);
    ComposerResponse response;
    ComposerStatus status = composer_api_validate("theme", text, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "validate: %s '%s' at '%s'\n", composer_status_name(status), response.error.message, response.error.path);
    }
    assert(status == COMPOSER_OK);
    composer_response_destroy(&response);
    free(text);
}

static char *motives_of(const char *degrees_a, const char *degrees_b) {
    char one[512];
    char two[512];
    static char out[1100];
    snprintf(one, sizeof one, MOTIVE_A, degrees_a);
    if (!degrees_b) {
        snprintf(out, sizeof out, "[%s]", one);
    } else {
        snprintf(two, sizeof two,
                 "{\"id\": \"B\", \"form\": \"full\", \"degrees\": \"%s\", \"pickup\": \"0\", \"harmony\": null, "
                 "\"features\": [], \"role\": \"melodic\"}",
                 degrees_b);
        snprintf(out, sizeof out, "[%s, %s]", one, two);
    }
    return out;
}

/* A valid request validates, in under 100 ms for 8 bars; a 2-bar motif in 3/4 is valid. */
static void test_valid(void) {
    clock_t start = clock();
    expect_valid(THEME("one-motive.json"), NULL, 0);
    double ms = (double)(clock() - start) * 1000.0 / CLOCKS_PER_SEC;
    if (ms >= 100.0) {
        fprintf(stderr, "validate took %.1f ms\n", ms);
    }
    assert(ms < 100.0);
    const char *const edits[][2] = {{"motives", "[{\"id\": \"A\", \"form\": \"full\", \"degrees\": \"1:q 3:q 5:q 5:q 3:q 1:q\", "
                                                "\"pickup\": \"0\", \"harmony\": null, \"features\": [], \"role\": \"melodic\"}]"},
                                    {"meter", "\"3/4\""}};
    expect_valid(THEME("one-motive.json"), edits, 2);
}

/* Each refusal of the plan, with the path of its field. */
static void test_plan_refusals(void) {
    {
        const char *const edits[][2] = {{"bars", "2"}};
        /* The reader refuses a bar count outside 3 to 16 before the plan is built, at the same path. */
        expect_refused_as(COMPOSER_ERR_REQUEST, THEME("one-motive.json"), edits, 1, "/bars", "bars must be 3 to 16");
    }
    {
        const char *const edits[][2] = {{"into", "\"period\""}, {"cadence", "\"HC\""}, {"first_cadence", "\"IAC\""}};
        expect_refused(THEME("one-motive.json"), edits, 3, "/first_cadence", "weaker");
    }
    {
        const char *const edits[][2] = {{"motives", "[{\"id\": \"A\", \"form\": \"full\", \"degrees\": \"1:w\", \"pickup\": \"0\", "
                                                    "\"harmony\": null, \"features\": [], \"role\": \"melodic\"}]"}};
        expect_refused(THEME("one-motive.json"), edits, 1, "/slots/basic", "at least two notes");
    }
    {
        char *motives = motives_of("1:q 3:e 5:e 1':h", "5:e 4:e 3:e 2:e 1:h");
        const char *const edits[][2] = {{"motives", motives}, {"slots", "{\"basic\": \"A\", \"contrasting\": \"B\"}"}};
        expect_refused(THEME("one-motive.json"), edits, 2, "/slots/contrasting", "no place in this pairing");
    }
    {
        char *motives = motives_of("1:q 3:e 5:e 1':h", "1:w 3:w 5:w");
        const char *const edits[][2] = {{"motives", motives}, {"ideas", "{\"repetition\": \"B\"}"}};
        expect_refused(THEME("idea-repetition.json"), edits, 2, "/ideas/repetition", "longer than the idea it fills");
    }
    {
        const char *const edits[][2] = {{"meter", "\"6/8\""}, {"accompaniment", "{\"pattern\": \"alberti\"}"}};
        expect_refused_as(COMPOSER_ERR_REQUEST, THEME("one-motive.json"), edits, 2, "/accompaniment", "needs a simple meter");
    }
    {
        const char *const edits[][2] = {{"accompaniment", "{\"pattern\": \"block\"}"}, {"voicing", "\"satb\""}};
        expect_refused_as(COMPOSER_ERR_REQUEST, THEME("one-motive.json"), edits, 2, "/voicing", "cannot be combined");
    }
    {
        char *motives = motives_of("1:w 3:w 5:w", NULL);
        const char *const edits[][2] = {{"motives", motives}};
        expect_refused(THEME("one-motive.json"), edits, 1, "/slots/basic",
                       "the motif is 3 bars; a 8-bar theme takes a motif of up to 2 bars");
    }
}

/* Given chords that cannot arrive the cadence, at the entry of the list that holds them. */
static void test_chord_refusals(void) {
    {
        const char *const edits[][2] = {{"chords", "[{\"bar\": 7, \"text\": \"V\"}]"}};
        expect_refused(THEME("one-motive.json"), edits, 1, "/chords/0", "chords bar 7: V cannot end a PAC");
    }
    {
        const char *const edits[][2] = {{"chords", "[{\"bar\": 0, \"text\": \"I\"}, {\"bar\": 7, \"text\": \"I6\"}]"}};
        expect_refused(THEME("one-motive.json"), edits, 1, "/chords/1", "i6 cannot end a PAC");
    }
    {
        const char *const edits[][2] = {{"cadence", "\"HC\""}, {"chords", "[{\"bar\": 7, \"text\": \"IV\"}]"}};
        expect_refused(THEME("one-motive.json"), edits, 2, "/chords/0", "iv cannot end a HC; the cadence needs the dominant");
    }
    {
        const char *const edits[][2] = {{"cadence", "\"HC\""}, {"chords", "[{\"bar\": 7, \"text\": \"V\"}]"}};
        expect_valid(THEME("one-motive.json"), edits, 2);
    }
}

/* A construction is read and checked by validate as compose does, without growing, at its own paths (B5 of P47). */
static void test_construction(void) {
    const char *const ideas = "\"ideas\": {\"basic idea\": 2, \"contrasting idea\": 2, \"consequent basic idea\": 2, "
                             "\"consequent contrasting idea\": 4}";
    char construction[512];
    snprintf(construction, sizeof construction, "{%s}", ideas);
    {
        /* With no bars the theme is as long as its construction; with them they must be the same. */
        const char *const edits[][2] = {{"bars", NULL}, {"construction", construction}};
        expect_valid(THEME("period-contrast.json"), edits, 2);
        const char *const same[][2] = {{"bars", "10"}, {"construction", construction}};
        expect_valid(THEME("period-contrast.json"), same, 2);
    }
    {
        const char *const edits[][2] = {{"bars", "8"}, {"construction", construction}};
        expect_refused(THEME("period-contrast.json"), edits, 2, "/construction/ideas", "the ideas are 10 bars; the theme has 8");
    }
    {
        const char *const edits[][2] = {{"bars", "9"}, {"construction", "{\"ideas\": {\"basic idea\": 2, \"repetition\": 2, \"contrasting idea\": 1, "
                                                                         "\"fragment 1\": 1, \"fragment 2\": 1, \"cadential\": 2}}"}};
        expect_refused(THEME("one-motive.json"), edits, 2, "/construction/ideas", "has no contrasting idea");
    }
    {
        const char *const edits[][2] = {{"construction", "{\"ideas\": {\"basic idea\": 2, \"bogus\": 1}}"}};
        expect_refused_as(COMPOSER_ERR_REQUEST, THEME("one-motive.json"), edits, 1, "/construction/ideas/bogus", "unknown idea");
    }
    {
        const char *const edits[][2] = {{"construction", "[]"}};
        expect_refused_as(COMPOSER_ERR_REQUEST, THEME("one-motive.json"), edits, 1, "/construction", "must be null or");
    }
    {
        const char *const edits[][2] = {{"bars", NULL},
                                        {"construction", "{\"ideas\": {\"basic idea\": 2, \"contrasting idea\": 2, \"consequent basic idea\": 2, "
                                                         "\"consequent contrasting idea\": 2}, \"extensions\": [{\"kind\": \"repeat\"}]}"}};
        expect_refused_as(COMPOSER_ERR_REQUEST, THEME("period-contrast.json"), edits, 2, "/construction/extensions/0/idea",
                          "needs the idea it follows");
    }
    {
        const char *const edits[][2] = {{"bars", NULL},
                                        {"construction", "{\"ideas\": {\"basic idea\": 2, \"contrasting idea\": 2, \"consequent basic idea\": 2, "
                                                         "\"consequent contrasting idea\": 2}, \"extensions\": [{\"kind\": \"stretch\", "
                                                         "\"idea\": \"basic idea\", \"bars\": 1}]}"}};
        expect_refused(THEME("period-contrast.json"), edits, 2, "/construction/extensions/0", "cannot follow the basic idea");
    }
    {
        const char *const edits[][2] = {{"bars", "2"}, {"construction", NULL}};
        expect_refused_as(COMPOSER_ERR_REQUEST, THEME("period-contrast.json"), edits, 2, "/bars", "bars must be 3 to 16");
    }
}

/* A bad "preferences" member is refused by validate, and by compose, with its path under /preferences (B3 of P43). */
static void test_preferences(void) {
    const char *const keep[][2] = {{"preferences", "{\"melody\": {\"keep\": 17}}"}};
    expect_refused_as(COMPOSER_ERR_REQUEST, THEME("one-motive.json"), keep, 1, "/preferences/melody/keep", "1 to 16");
    const char *const range[][2] = {{"preferences", "{\"melody\": {\"range\": {\"low\": 60, \"high\": 65}}}"}};
    expect_refused_as(COMPOSER_ERR_REQUEST, THEME("one-motive.json"), range, 1, "/preferences/melody/range/high", "");
    const char *const unknown[][2] = {{"preferences", "{\"bogus\": 1}"}};
    expect_refused_as(COMPOSER_ERR_REQUEST, THEME("one-motive.json"), unknown, 1, "/preferences/bogus", "");
    const char *const wrong[][2] = {{"preferences", "[]"}};
    expect_refused_as(COMPOSER_ERR_REQUEST, THEME("one-motive.json"), wrong, 1, "/preferences", "preferences must be an object");
    const char *const leaps[][2] = {{"preferences", "{\"motif\": {\"leaps_per_motif\": 12}}"}};
    expect_refused_as(COMPOSER_ERR_REQUEST, THEME("one-motive.json"), leaps, 1, "/preferences/motif/leaps_per_motif", "0 to 11");
    const char *const long_nct[][2] = {{"preferences", "{\"motif\": {\"long_nct_one_in\": 65}}"}};
    expect_refused_as(COMPOSER_ERR_REQUEST, THEME("one-motive.json"), long_nct, 1, "/preferences/motif/long_nct_one_in", "0 to 64");
    const char *const plan[][2] = {{"preferences", "{\"motif\": {\"plan\": [\"VIII\"]}}"}};
    expect_refused_as(COMPOSER_ERR_REQUEST, THEME("one-motive.json"), plan, 1, "/preferences/motif/plan/0", "Roman numeral");
    const char *const fragments[][2] = {
        {"preferences", "{\"development\": {\"continuation\": {\"fragment_lengths\": {\"segment\": 0}}}}"}};
    expect_refused_as(COMPOSER_ERR_REQUEST, THEME("one-motive.json"), fragments, 1,
                      "/preferences/development/continuation/fragment_lengths", "more than 0");
    const char *const fine[][2] = {{"preferences", "{\"melody\": {\"keep\": 2}, \"motif\": {\"pickup_one_in\": 0}}"}};
    expect_valid(THEME("one-motive.json"), fine, 1);
}

/* A piece request of a form with sizes comes back with the size it chose, and no section defaults (P45 B2). */
static void test_piece_of_a_form(void) {
    char *text = test_read_file(COMPOSER_FIXTURE_DIR "/piece/rounded-binary.json", NULL);
    ComposerResponse response;
    ComposerStatus status = composer_api_validate("piece", text, &response);
    assert(status == COMPOSER_OK && response.json);
    free(text);
    cJSON *document = cJSON_Parse(response.json);
    const cJSON *request = cJSON_GetObjectItemCaseSensitive(document, "request");
    assert(request);
    assert(cJSON_GetObjectItemCaseSensitive(request, "bars") && cJSON_GetObjectItemCaseSensitive(request, "bars")->valuedouble == 16);
    assert(cJSON_GetObjectItemCaseSensitive(request, "count") && cJSON_GetObjectItemCaseSensitive(request, "count")->valuedouble == 0);
    assert(!cJSON_GetObjectItemCaseSensitive(request, "sections"));
    cJSON_Delete(document);
    composer_response_destroy(&response);
}

int main(void) {
    test_valid();
    test_piece_of_a_form();
    test_plan_refusals();
    test_chord_refusals();
    test_construction();
    test_preferences();
    printf("test_api_validate: ok\n");
    return 0;
}
