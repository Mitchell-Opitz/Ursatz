#include <assert.h>
#include <pthread.h>
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
 * The theme entry (docs/api.md): every theme-* case of the CLI snapshots (tests/cli_snapshots.cmake) run from its request,
 * with the document, the plan, the MIDI file and the MusicXML equal to the snapshots; a cancel at the first step; two
 * threads running two cases each at once, equal to the sequential results; the requests it refuses, with their paths.
 * The requests and responses are written beside the test, for the schema test.
 */

#define THEME(name) COMPOSER_FIXTURE_DIR "/theme/" name
#define SNAPSHOT(name) COMPOSER_SNAPSHOT_DIR "/" name

typedef struct {
    const char *name;     /* the snapshot case: theme-<name> */
    const char *fixture;  /* the request fixture */
    unsigned seed;
    bool check;
} Case;

static const Case CASES[] = {{"one-motive", THEME("one-motive.json"), 1, false},
                             {"period-contrast", THEME("period-contrast.json"), 1, false},
                             {"chords", THEME("chords.json"), 1, false},
                             {"fast-minor", THEME("fast-minor.json"), 1, false}};
#define CASE_COUNT (sizeof CASES / sizeof CASES[0])

static char *replace(char *text, const char *placeholder, const char *value) {
    char *at = strstr(text, placeholder);
    if (!at) {
        return text;
    }
    size_t length = strlen(text) - strlen(placeholder) + strlen(value) + 1;
    char *out = malloc(length);
    assert(out);
    snprintf(out, length, "%.*s%s%s", (int)(at - text), text, value, at + strlen(placeholder));
    free(text);
    return out;
}

/* The request of a case: the fixture with its placeholders filled as cli_snapshots.cmake fills them, the seed, and the files asked for. */
static char *case_request(const Case *item) {
    char *text = test_read_file(item->fixture, NULL);
    text = replace(text, "@CHORDS@",
                   "[{\"bar\": 0, \"text\": \"I\"}, {\"bar\": 1, \"text\": \"IV V@2\"}, {\"bar\": 2, \"text\": \"VI\"}, "
                   "{\"bar\": 3, \"text\": \"V\"}]");
    text = replace(text, "@STYLE@", COMPOSER_FIXTURE_DIR "/style-fast-minor.json");
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request);
    assert(cJSON_AddNumberToObject(request, "seed", item->seed));
    assert(cJSON_AddBoolToObject(request, "midi", true) && cJSON_AddBoolToObject(request, "musicxml", true));
    if (item->check) {
        assert(cJSON_AddBoolToObject(request, "check", true));
    }
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    assert(printed);
    return printed;
}

static char *snapshot_path(const char *name, const char *suffix, char *out, size_t size) {
    snprintf(out, size, COMPOSER_SNAPSHOT_DIR "/theme-%s.%s", name, suffix);
    return out;
}

static void expect_json_equal(const char *actual, const char *expected_path, bool drop_files, const char *what) {
    char *expected_text = test_read_file(expected_path, NULL);
    cJSON *actual_json = cJSON_Parse(actual);
    cJSON *expected_json = cJSON_Parse(expected_text);
    assert(actual_json && expected_json);
    if (drop_files) {
        cJSON_DeleteItemFromObjectCaseSensitive(expected_json, "files");
    } else {
        /* The versions the plan records are not part of the snapshot (cli_snapshot_compare.cmake). */
        cJSON *both[2] = {actual_json, expected_json};
        for (int i = 0; i < 2; i++) {
            cJSON *metadata = cJSON_GetObjectItemCaseSensitive(both[i], "metadata");
            cJSON_DeleteItemFromObjectCaseSensitive(metadata, "composer_version");
            cJSON_DeleteItemFromObjectCaseSensitive(metadata, "ursatz_commit");
        }
    }
    if (!cJSON_Compare(actual_json, expected_json, true)) {
        fprintf(stderr, "%s differs from %s:\n%s\n", what, expected_path, actual);
    }
    assert(cJSON_Compare(actual_json, expected_json, true));
    cJSON_Delete(actual_json);
    cJSON_Delete(expected_json);
    free(expected_text);
}

/* The response of a case equals its snapshots. */
static void expect_case(const Case *item, const ComposerResponse *response) {
    char path[256];
    expect_json_equal(response->json, snapshot_path(item->name, "stdout.json", path, sizeof path), true, "the document");
    expect_json_equal(response->plan_json, snapshot_path(item->name, "plan.json", path, sizeof path), false, "the plan");
    size_t size = 0;
    char *midi = test_read_file(snapshot_path(item->name, "mid", path, sizeof path), &size);
    assert(response->midi && response->midi_size == size && memcmp(response->midi, midi, size) == 0);
    free(midi);
    char *musicxml = test_read_file(snapshot_path(item->name, "musicxml", path, sizeof path), NULL);
    assert(response->musicxml && strcmp(response->musicxml, musicxml) == 0);
    free(musicxml);
}

static void write_pair(const char *tag, const char *request, const ComposerResponse *response) {
    char path[160];
    snprintf(path, sizeof path, "api_theme_request_%s.json", tag);
    test_write_file(path, request);
    snprintf(path, sizeof path, "api_theme_response_%s.json", tag);
    test_write_file(path, response->json);
}

/* --- the snapshots ------------------------------------------------------------------------------ */

static void test_snapshots(void) {
    for (size_t i = 0; i < CASE_COUNT; i++) {
        char *request = case_request(&CASES[i]);
        ComposerResponse response;
        ComposerStatus status = composer_api_theme(request, NULL, &response);
        if (status != COMPOSER_OK) {
            fprintf(stderr, "theme-%s: %s: %s\n", CASES[i].name, composer_status_name(status), response.error.message);
        }
        assert(status == COMPOSER_OK);
        expect_case(&CASES[i], &response);
        write_pair(CASES[i].name, request, &response);
        composer_response_destroy(&response);
        free(request);
    }
}

/* --- uneven themes (B5 of P47) -------------------------------------------------------------------- */

typedef struct {
    const char *tag;      /* the written response: api_theme_response_<tag>.json */
    const char *fixture;
    size_t first_half;
    size_t second_half;
    const char *text;     /* what construction.text says, the words of B4 */
    const char *extension; /* the kind of its one extension */
} Uneven;

static const Uneven UNEVEN[] = {
    {"uneven-period", THEME("uneven-period.json"), 4, 6,
     "4+6 period: the section is 10 bars, so the consequent repeats its contrasting idea, varied (bars 9 and 10).", "repeat"},
    {"uneven-sentence", THEME("uneven-sentence.json"), 4, 5,
     "4+5 sentence: the section is 9 bars, so fragment 2 is repeated a step lower (bar 7).", "repeat"},
    {"evaded-sentence", THEME("evaded-sentence.json"), 4, 6,
     "4+6 sentence: the section is 10 bars, so the cadence in bar 8 is evaded (V to vi) and the cadential idea returns to close "
     "in bar 10.",
     "evaded"}};

static const cJSON *member(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    assert(item);
    return item;
}

/* The request of a case with seed `seed`, composed: the status, and the response. */
static ComposerStatus compose_uneven(const char *fixture, unsigned seed, cJSON *edits, char **request, ComposerResponse *response) {
    char *text = test_read_file(fixture, NULL);
    cJSON *json = cJSON_Parse(text);
    free(text);
    assert(json);
    for (cJSON *edit = edits ? edits->child : NULL; edit; edit = edit->next) {
        cJSON_DeleteItemFromObjectCaseSensitive(json, edit->string);
        assert(cJSON_AddItemToObject(json, edit->string, cJSON_Duplicate(edit, true)));
    }
    assert(cJSON_AddNumberToObject(json, "seed", seed));
    *request = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    assert(*request);
    return composer_api_theme(*request, NULL, response);
}

/* A 4+6 period, a 4+5 sentence and a 10-bar sentence with an evaded cadence, from a motif of one bar: each composes at the first of
   16 seeds that does, with the construction named, its halves and its extension, and its text as docs/grow-construction.md has it. */
static void test_uneven(void) {
    for (size_t i = 0; i < sizeof UNEVEN / sizeof UNEVEN[0]; i++) {
        const Uneven *item = &UNEVEN[i];
        ComposerResponse response;
        char *request = NULL;
        ComposerStatus status = COMPOSER_ERR_UNSATISFIABLE;
        for (unsigned seed = 1; seed <= 16 && status != COMPOSER_OK; seed++) {
            if (request) {
                free(request);
            }
            status = compose_uneven(item->fixture, seed, NULL, &request, &response);
            if (status != COMPOSER_OK) {
                composer_response_destroy(&response);
            }
        }
        if (status != COMPOSER_OK) {
            fprintf(stderr, "%s: no seed from 1 to 16 composes\n", item->tag);
        }
        assert(status == COMPOSER_OK);
        cJSON *document = cJSON_Parse(response.json);
        assert(document);
        assert(!cJSON_GetObjectItemCaseSensitive(document, "construction")); /* none was given: nothing to echo */
        const cJSON *construction = member(cJSON_GetArrayItem(member(document, "themes"), 0), "construction");
        assert(cJSON_IsObject(construction));
        const cJSON *text = member(construction, "text");
        if (strcmp(text->valuestring, item->text) != 0) {
            fprintf(stderr, "%s: '%s', expected '%s'\n", item->tag, text->valuestring, item->text);
        }
        assert(strcmp(text->valuestring, item->text) == 0);
        assert(strcmp(member(construction, "reason")->valuestring, "length") == 0);
        const cJSON *halves = member(construction, "halves");
        assert(cJSON_GetArraySize(halves) == 2);
        assert(cJSON_GetArrayItem(halves, 0)->valuedouble == (double)item->first_half);
        assert(cJSON_GetArrayItem(halves, 1)->valuedouble == (double)item->second_half);
        const cJSON *extensions = member(construction, "extensions");
        assert(cJSON_GetArraySize(extensions) == 1);
        assert(strcmp(member(cJSON_GetArrayItem(extensions, 0), "kind")->valuestring, item->extension) == 0);
        /* The ideas follow one another from bar 1 (a repeat sits between two of them), and with the extension they are the bars. */
        const cJSON *ideas = member(construction, "ideas");
        double total = member(cJSON_GetArrayItem(extensions, 0), "bars")->valuedouble;
        double previous_end = 1;
        const cJSON *idea = NULL;
        cJSON_ArrayForEach(idea, ideas) {
            assert(member(idea, "first_bar")->valuedouble >= previous_end);
            previous_end = member(idea, "first_bar")->valuedouble + member(idea, "bars")->valuedouble;
            total += member(idea, "bars")->valuedouble;
        }
        assert(total == member(document, "bars")->valuedouble);
        write_pair(item->tag, request, &response);
        cJSON_Delete(document);
        composer_response_destroy(&response);
        free(request);
    }
}

/* A construction given with no bars: the theme is as long as it, the document echoes it, and the reason is "given". */
static void test_given_construction(void) {
    cJSON *edits = cJSON_CreateObject();
    cJSON *given = cJSON_Parse("{\"ideas\": {\"basic idea\": 2, \"contrasting idea\": 2, \"consequent basic idea\": 2, "
                              "\"consequent contrasting idea\": 4}, \"extensions\": []}");
    assert(edits && given && cJSON_AddItemToObject(edits, "construction", given) && cJSON_AddNullToObject(edits, "bars"));
    ComposerResponse response;
    char *request = NULL;
    ComposerStatus status = compose_uneven(THEME("uneven-period.json"), 1, edits, &request, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "given construction: %s: %s\n", composer_status_name(status), response.error.message);
    }
    assert(status == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response.json);
    assert(document && member(document, "bars")->valuedouble == 10);
    const cJSON *echoed = member(document, "construction");
    assert(member(member(echoed, "ideas"), "consequent contrasting idea")->valuedouble == 4);
    const cJSON *construction = member(cJSON_GetArrayItem(member(document, "themes"), 0), "construction");
    assert(strcmp(member(construction, "reason")->valuestring, "given") == 0);
    assert(strcmp(member(construction, "text")->valuestring, "4+6 period as given.") == 0);
    assert(cJSON_GetArraySize(member(construction, "extensions")) == 0);
    write_pair("given-construction", request, &response);
    cJSON_Delete(document);
    cJSON_Delete(edits);
    composer_response_destroy(&response);
    free(request);
}

/* A theme without a construction says so: its construction is null. */
static void test_plain_has_no_construction(void) {
    char *request = case_request(&CASES[0]);
    ComposerResponse response;
    assert(composer_api_theme(request, NULL, &response) == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response.json);
    assert(document && cJSON_IsNull(member(cJSON_GetArrayItem(member(document, "themes"), 0), "construction")));
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(request);
}

/* --- preferences (B3 of P43) --------------------------------------------------------------------- */

/* The one-motive request with seed `seed` and, when not NULL, the member "preferences" set to the JSON text. */
static char *preferences_request(unsigned seed, const char *preferences) {
    char *text = test_read_file(THEME("one-motive.json"), NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request);
    cJSON_DeleteItemFromObjectCaseSensitive(request, "seed");
    assert(cJSON_AddNumberToObject(request, "seed", seed));
    if (preferences) {
        cJSON *item = cJSON_Parse(preferences);
        assert(item && cJSON_AddItemToObject(request, "preferences", item));
    }
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    assert(printed);
    return printed;
}

static const cJSON *first_theme(const cJSON *document) {
    return cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
}

/*
 * A request without "preferences" has no member of that name in its response, and empty preferences change nothing but the
 * echo: the document without it equals the one without the member. Preferences are echoed in full (the defaults filled in),
 * the motif group is accepted and does nothing, and the melody and development groups reach the grow: at seed 2 the
 * attempts, keep and against_bass weight of the request make another theme (measured: the heard text differs), and the
 * minimum the development list shows for the repetition idea is the 1/1 asked for, 0.55 by default.
 */
static void test_preferences(void) {
    ComposerResponse plain;
    char *request = preferences_request(2, NULL);
    assert(composer_api_theme(request, NULL, &plain) == COMPOSER_OK);
    free(request);
    cJSON *plain_document = cJSON_Parse(plain.json);
    assert(plain_document && !cJSON_GetObjectItemCaseSensitive(plain_document, "preferences"));

    ComposerResponse empty;
    request = preferences_request(2, "{}");
    assert(composer_api_theme(request, NULL, &empty) == COMPOSER_OK);
    free(request);
    cJSON *empty_document = cJSON_Parse(empty.json);
    assert(empty_document && cJSON_GetObjectItemCaseSensitive(empty_document, "preferences"));
    cJSON_DeleteItemFromObjectCaseSensitive(empty_document, "preferences");
    assert(cJSON_Compare(plain_document, empty_document, true));
    assert(empty.midi_size == plain.midi_size && memcmp(empty.midi, plain.midi, plain.midi_size) == 0);
    cJSON_Delete(empty_document);
    composer_response_destroy(&empty);

    const char *given =
        "{\"melody\": {\"attempts\": 1, \"keep\": 1, \"weights\": {\"against_bass\": 20}}, "
        "\"motif\": {\"bars\": {\"one\": 0, \"two\": 1}}, "
        "\"development\": {\"fidelity_minimum\": {\"balanced\": {\"presentation\": \"1/1\"}}}}";
    ComposerResponse set;
    request = preferences_request(2, given);
    assert(composer_api_theme(request, NULL, &set) == COMPOSER_OK);
    write_pair("preferences", request, &set);
    free(request);
    cJSON *document = cJSON_Parse(set.json);
    assert(document);
    const cJSON *echoed = cJSON_GetObjectItemCaseSensitive(document, "preferences");
    const cJSON *melody = cJSON_GetObjectItemCaseSensitive(echoed, "melody");
    assert(cJSON_GetObjectItemCaseSensitive(melody, "attempts")->valueint == 1);
    assert(cJSON_GetObjectItemCaseSensitive(melody, "keep")->valueint == 1);
    assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(melody, "weights"), "against_bass")->valueint == 20);
    assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(melody, "weights"), "seam")->valueint == 1);
    const cJSON *bars = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(echoed, "motif"), "bars");
    assert(cJSON_GetObjectItemCaseSensitive(bars, "one")->valueint == 0 && cJSON_GetObjectItemCaseSensitive(bars, "two")->valueint == 1);
    assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(echoed, "development"), "fidelity_minimum"));

    const cJSON *theme = first_theme(document);
    const cJSON *plain_theme = first_theme(plain_document);
    const cJSON *heard = cJSON_GetObjectItemCaseSensitive(theme, "heard_text");
    const cJSON *plain_heard = cJSON_GetObjectItemCaseSensitive(plain_theme, "heard_text");
    assert(cJSON_IsString(heard) && cJSON_IsString(plain_heard) && strcmp(heard->valuestring, plain_heard->valuestring) != 0);
    const cJSON *repetition = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(theme, "developments"), 0);
    const cJSON *plain_repetition = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(plain_theme, "developments"), 0);
    assert(cJSON_GetObjectItemCaseSensitive(repetition, "minimum")->valuedouble == 1.0);
    assert(cJSON_GetObjectItemCaseSensitive(plain_repetition, "minimum")->valuedouble == 0.55);
    cJSON_Delete(document);
    cJSON_Delete(plain_document);
    composer_response_destroy(&set);
    composer_response_destroy(&plain);
}

/* --- suggest, plans and explanations ------------------------------------------------------------- */

static void test_suggest_plans_explain(void) {
    char *text = test_read_file(THEME("one-motive.json"), NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request);
    assert(cJSON_AddNumberToObject(request, "seed", 1) && cJSON_AddNumberToObject(request, "suggest", 2) &&
           cJSON_AddBoolToObject(request, "plans", true) && cJSON_AddStringToObject(request, "explain", "json"));
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    ComposerResponse response;
    assert(composer_api_theme(printed, NULL, &response) == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response.json);
    assert(document);
    const cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
    assert(cJSON_GetArraySize(themes) == 2);
    for (int k = 0; k < 2; k++) {
        const cJSON *theme = cJSON_GetArrayItem(themes, k);
        assert(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(theme, "plan")));
        const cJSON *explanation = cJSON_GetObjectItemCaseSensitive(theme, "explanation");
        assert(cJSON_IsString(explanation));
        cJSON *parsed = cJSON_Parse(explanation->valuestring);
        assert(cJSON_IsObject(parsed));
        cJSON_Delete(parsed);
    }
    /* The plan of the first theme is the plan in the document. */
    cJSON *plan = cJSON_Parse(response.plan_json);
    assert(cJSON_Compare(plan, cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(themes, 0), "plan"), true));
    cJSON_Delete(plan);
    assert(response.midi == NULL && response.musicxml == NULL);
    write_pair("suggest", printed, &response);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(printed);
}

/* The developments of a theme carry the segment each idea is made from (or null) and a one-line summary (B2 of P34), and
 * each idea's fidelity to its source, the minimum its role asks, the relation heard and whether it is the one intended
 * (API 1.10, R-332). */
static void test_developments(void) {
    char *text = test_read_file(THEME("one-motive.json"), NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request && cJSON_AddNumberToObject(request, "seed", 1));
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    ComposerResponse response;
    assert(composer_api_theme(printed, NULL, &response) == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response.json);
    assert(document);
    const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
    const cJSON *developments = cJSON_GetObjectItemCaseSensitive(theme, "developments");
    assert(cJSON_GetArraySize(developments) == 4);
    int measured = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, developments) {
        const cJSON *idea = cJSON_GetObjectItemCaseSensitive(item, "idea");
        const cJSON *segment = cJSON_GetObjectItemCaseSensitive(item, "segment");
        const cJSON *summary = cJSON_GetObjectItemCaseSensitive(item, "summary");
        assert(cJSON_IsString(idea) && (cJSON_IsString(segment) || cJSON_IsNull(segment)));
        assert(cJSON_IsString(summary) && strncmp(summary->valuestring, idea->valuestring, strlen(idea->valuestring)) == 0);
        assert(strstr(summary->valuestring, " = ") != NULL);
        const cJSON *fidelity = cJSON_GetObjectItemCaseSensitive(item, "fidelity");
        const cJSON *minimum = cJSON_GetObjectItemCaseSensitive(item, "minimum");
        const cJSON *heard = cJSON_GetObjectItemCaseSensitive(item, "heard");
        const cJSON *agrees = cJSON_GetObjectItemCaseSensitive(item, "heard_agrees");
        assert(fidelity && minimum && heard && agrees); /* every item has the four members */
        if (cJSON_IsNull(fidelity)) {
            assert(cJSON_IsNull(minimum) && cJSON_IsNull(heard) && cJSON_IsNull(agrees));
            continue;
        }
        assert(cJSON_IsNumber(fidelity) && cJSON_IsNumber(minimum) && cJSON_IsString(heard) && cJSON_IsBool(agrees));
        assert(fidelity->valuedouble >= 0 && fidelity->valuedouble <= 1 && minimum->valuedouble >= 0);
        assert(fidelity->valuedouble >= minimum->valuedouble); /* a departing idea's minimum is 0 */
        assert(heard->valuestring[0] != '\0');
        measured++;
    }
    assert(measured > 0);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(printed);
}

/* --- cancel ---------------------------------------------------------------------------------------- */

typedef struct {
    unsigned steps;
    bool grow_reported;
    bool compose_reported;
} Watch;

static void on_progress(void *user, const char *stage, unsigned done, unsigned total) {
    Watch *watch = user;
    (void)done;
    (void)total;
    watch->grow_reported = watch->grow_reported || strcmp(stage, "grow") == 0;
    watch->compose_reported = watch->compose_reported || strcmp(stage, "grow") != 0;
}

static bool cancel_at_first(void *user) {
    Watch *watch = user;
    watch->steps++;
    return true;
}

static bool never_cancel(void *user) {
    (void)user;
    return false;
}

/* A cancel at the first step: no document, no plan, nothing left allocated (the valgrind run). */
static void test_cancel_first(void) {
    char *request = case_request(&CASES[0]);
    Watch watch;
    memset(&watch, 0, sizeof(watch));
    ComposerJob job = {on_progress, cancel_at_first, &watch};
    ComposerResponse response;
    assert(composer_api_theme(request, &job, &response) == COMPOSER_ERR_CANCELLED);
    assert(watch.steps >= 1 && watch.grow_reported);
    assert(!response.json && !response.plan_json && !response.midi && !response.musicxml);
    composer_response_destroy(&response);
    free(request);
}

static void test_progress(void) {
    char *request = case_request(&CASES[0]);
    Watch watch;
    ComposerResponse response;
    /* A job that never cancels reports the stages and gives the snapshot. */
    memset(&watch, 0, sizeof(watch));
    ComposerJob quiet = {on_progress, never_cancel, &watch};
    assert(composer_api_theme(request, &quiet, &response) == COMPOSER_OK);
    assert(watch.grow_reported && watch.compose_reported);
    expect_case(&CASES[0], &response);
    composer_response_destroy(&response);
    free(request);
}

/* --- two threads --------------------------------------------------------------------------------- */

typedef struct {
    const Case *cases[2];
    char *json[2];
    char *plan[2];
    unsigned char *midi[2];
    size_t midi_size[2];
} Worker;

static void *run_worker(void *argument) {
    Worker *worker = argument;
    for (int i = 0; i < 2; i++) {
        char *request = case_request(worker->cases[i]);
        ComposerResponse response;
        assert(composer_api_theme(request, NULL, &response) == COMPOSER_OK);
        worker->json[i] = strdup(response.json);
        worker->plan[i] = strdup(response.plan_json);
        worker->midi_size[i] = response.midi_size;
        worker->midi[i] = malloc(response.midi_size);
        assert(worker->json[i] && worker->plan[i] && worker->midi[i]);
        memcpy(worker->midi[i], response.midi, response.midi_size);
        composer_response_destroy(&response);
        free(request);
    }
    return NULL;
}

static void test_threads(void) {
    /* Sequential results first. */
    Worker sequential;
    memset(&sequential, 0, sizeof(sequential));
    sequential.cases[0] = &CASES[0];
    sequential.cases[1] = &CASES[1];
    run_worker(&sequential);

    Worker workers[2];
    memset(workers, 0, sizeof(workers));
    pthread_t threads[2];
    for (int t = 0; t < 2; t++) {
        /* Each thread runs the same two cases, in the other order for the second. */
        workers[t].cases[0] = &CASES[t == 0 ? 0 : 1];
        workers[t].cases[1] = &CASES[t == 0 ? 1 : 0];
        assert(pthread_create(&threads[t], NULL, run_worker, &workers[t]) == 0);
    }
    for (int t = 0; t < 2; t++) {
        assert(pthread_join(threads[t], NULL) == 0);
        for (int i = 0; i < 2; i++) {
            int which = workers[t].cases[i] == &CASES[0] ? 0 : 1;
            assert(strcmp(workers[t].json[i], sequential.json[which]) == 0);
            assert(strcmp(workers[t].plan[i], sequential.plan[which]) == 0);
            assert(workers[t].midi_size[i] == sequential.midi_size[which] &&
                   memcmp(workers[t].midi[i], sequential.midi[which], sequential.midi_size[which]) == 0);
            free(workers[t].json[i]);
            free(workers[t].plan[i]);
            free(workers[t].midi[i]);
        }
    }
    for (int i = 0; i < 2; i++) {
        free(sequential.json[i]);
        free(sequential.plan[i]);
        free(sequential.midi[i]);
    }
}

/* --- refused requests ---------------------------------------------------------------------------- */

/* The one-motive request with each member of `edits` ({name, JSON text or NULL to remove}) set. */
static ComposerStatus refuse_with(const char *const edits[][2], size_t count, ComposerResponse *response) {
    char *text = test_read_file(THEME("one-motive.json"), NULL);
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
    ComposerStatus status = composer_api_theme(printed, NULL, response);
    free(printed);
    return status;
}

/* The one-motive request with member `name` set to the JSON text `value` (NULL removes it). */
static ComposerStatus refuse(const char *name, const char *value, ComposerResponse *response) {
    const char *const edits[][2] = {{name, value}};
    return refuse_with(edits, 1, response);
}

/* The development setting (B3 of P34): echoed in the brief, balanced when absent, named in the explanation. */
static void expect_development(const char *value, const char *expected) {
    const char *const edits[][2] = {{"development", value}, {"explain", "\"text\""}};
    ComposerResponse response;
    assert(refuse_with(edits, 2, &response) == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response.json);
    assert(document);
    const cJSON *brief = cJSON_GetObjectItemCaseSensitive(document, "brief");
    const cJSON *echoed = cJSON_GetObjectItemCaseSensitive(brief, "development");
    assert(cJSON_IsString(echoed) && strcmp(echoed->valuestring, expected) == 0);
    const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
    const cJSON *explanation = cJSON_GetObjectItemCaseSensitive(theme, "explanation");
    char line[48];
    snprintf(line, sizeof(line), "development: %s", expected);
    assert(cJSON_IsString(explanation) && strstr(explanation->valuestring, line));
    cJSON_Delete(document);
    composer_response_destroy(&response);
}

static void test_development_setting(void) {
    expect_development(NULL, "balanced");
    expect_development("\"balanced\"", "balanced");
    expect_development("\"melody\"", "melody");
    expect_development("\"theme\"", "theme");
}

static void expect_refused(const char *name, const char *value, ComposerStatus status, const char *path, const char *message) {
    ComposerResponse response;
    ComposerStatus got = refuse(name, value, &response);
    if (got != status || strcmp(response.error.path, path) != 0 || !strstr(response.error.message, message)) {
        fprintf(stderr, "%s: %s '%s' at '%s', expected %s '%s' at '%s'\n", name, composer_status_name(got),
                response.error.message, response.error.path, composer_status_name(status), message, path);
    }
    assert(got == status);
    assert(strcmp(response.error.path, path) == 0);
    assert(strstr(response.error.message, message));
    assert(!response.json && !response.plan_json && !response.midi && !response.musicxml);
    composer_response_destroy(&response);
}

/* A motive of one 4/4 motive text (degrees) in a theme of `bars` bars, refused with the length of the motif and of its idea. */
static void expect_motif_refused(const char *meter, const char *degrees, const char *bars, const char *message) {
    char motives[200];
    snprintf(motives, sizeof motives,
             "[{\"id\": \"A\", \"form\": \"full\", \"degrees\": \"%s\", \"pickup\": \"0\", \"harmony\": null, "
             "\"features\": [], \"role\": \"melodic\"}]",
             degrees);
    const char *const edits[][2] = {{"motives", motives}, {"meter", meter}, {"bars", bars}};
    ComposerResponse response;
    ComposerStatus status = refuse_with(edits, 3, &response);
    if (status != COMPOSER_ERR_UNSATISFIABLE || strcmp(response.error.path, "/slots/basic") != 0 ||
        !strstr(response.error.message, message)) {
        fprintf(stderr, "%s: %s '%s' at '%s'\n", message, composer_status_name(status), response.error.message,
                response.error.path);
    }
    assert(status == COMPOSER_ERR_UNSATISFIABLE);
    assert(strcmp(response.error.path, "/slots/basic") == 0);
    assert(strstr(response.error.message, message));
    composer_response_destroy(&response);
}

/* A motif longer than the basic idea of its theme is refused, with the bars of both and the path of the slot. */
static void test_motif_too_long(void) {
    expect_motif_refused("\"4/4\"", "1:w 3:w 5:w", "8", "the motif is 3 bars; a 8-bar theme takes a motif of up to 2 bars, or of 4 for its presentation or antecedent");
    expect_motif_refused("\"4/4\"", "1:w 3:w", "4", "the motif is 2 bars; a 4-bar theme takes a motif of up to 1 bars, or of 2 for its presentation or antecedent");
}

/* A 2-bar motif in 3/4 is the basic idea of an 8-bar theme. */
static void test_two_bar_motif(void) {
    const char *motives = "[{\"id\": \"A\", \"form\": \"full\", \"degrees\": \"1:q 3:q 5:q 5:q 3:q 1:q\", \"pickup\": \"0\", "
                          "\"harmony\": null, \"features\": [], \"role\": \"melodic\"}]";
    const char *const edits[][2] = {{"motives", motives}, {"meter", "\"3/4\""}};
    ComposerResponse response;
    ComposerStatus status = refuse_with(edits, 2, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "2-bar 3/4: %s: %s at '%s'\n", composer_status_name(status), response.error.message, response.error.path);
    }
    assert(status == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response.json);
    assert(document && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(document, "themes")) == 1);
    cJSON_Delete(document);
    composer_response_destroy(&response);
}

/*
 * A 4-bar motif fills the presentation of an 8-bar sentence; a contrasting slot beside it is refused with its path. Its
 * first bar is two notes: a motif whose segments are each one note has no liquidated residue (R-345), so no sentence ends.
 */
static void test_part_length_motif(void) {
    const char *motives = "[{\"id\": \"A\", \"form\": \"full\", \"degrees\": \"1:h 2:h 3:w 5:w 3:w\", \"pickup\": \"0\", "
                          "\"harmony\": null, \"features\": [], \"role\": \"melodic\"}]";
    const char *const edits[][2] = {{"motives", motives}};
    ComposerResponse response;
    ComposerStatus status = refuse_with(edits, 1, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "4-bar motif: %s: %s at '%s'\n", composer_status_name(status), response.error.message, response.error.path);
    }
    assert(status == COMPOSER_OK);
    composer_response_destroy(&response);

    const char *two = "[{\"id\": \"A\", \"form\": \"full\", \"degrees\": \"1:w 3:w 5:w 3:w\", \"pickup\": \"0\", "
                      "\"harmony\": null, \"features\": [], \"role\": \"melodic\"}, "
                      "{\"id\": \"B\", \"form\": \"full\", \"degrees\": \"5:q 3:q 1:h\", \"pickup\": \"0\", "
                      "\"harmony\": null, \"features\": [], \"role\": \"melodic\"}]";
    const char *const slot_edits[][2] = {{"motives", two}, {"slots", "{\"basic\": \"A\", \"contrasting\": \"B\"}"}};
    status = refuse_with(slot_edits, 2, &response);
    assert(status == COMPOSER_ERR_REQUEST);
    assert(strcmp(response.error.path, "/slots/contrasting") == 0);
    assert(strstr(response.error.message, "the basic motif fills the whole presentation; leave the contrasting idea out"));
    composer_response_destroy(&response);

    const char *const idea_edits[][2] = {{"motives", two}, {"ideas", "{\"repetition\": \"B\"}"}};
    status = refuse_with(idea_edits, 2, &response);
    assert(status == COMPOSER_ERR_REQUEST);
    assert(strcmp(response.error.path, "/ideas/repetition") == 0);
    composer_response_destroy(&response);
}

static void test_refused(void) {
    /* A motive that names a file in place of its content. */
    expect_refused("motives",
                   "[{\"id\": \"A\", \"form\": \"full\", \"path\": \"motive.mid\", \"pickup\": \"0\"}]", COMPOSER_ERR_REQUEST,
                   "/motives/0/path", "a motive carries its content");
    expect_refused("motives", "[{\"id\": \"A\", \"degrees\": \"1:q 3:e 5:e 1':h\", \"forms\": []}]", COMPOSER_ERR_REQUEST,
                   "/motives/0/forms", "saved forms are no longer read");
    expect_refused("schema_version", "1", COMPOSER_ERR_REQUEST, "/schema_version", "\"schema_version\": 2");
    expect_refused("schema", "\"other\"", COMPOSER_ERR_REQUEST, "/schema", "ursatz-theme-request");
    expect_refused("into", NULL, COMPOSER_ERR_REQUEST, "/into", "the request needs \"into\"");
    expect_refused("into", "\"rondo\"", COMPOSER_ERR_REQUEST, "/into", "into must be sentence, period or hybrid");
    expect_refused("key", "\"H\"", COMPOSER_ERR_REQUEST, "/key", "unknown key 'H'");
    expect_refused("ideas", "{\"first idea\": \"A\"}", COMPOSER_ERR_REQUEST, "/ideas", "unknown idea 'first idea'");
    expect_refused("ideas", "{\"repetition\": \"B\"}", COMPOSER_ERR_REQUEST, "/ideas/repetition", "which the request does not have");
    expect_refused("chords", "[{\"bar\": 9, \"text\": \"I\"}]", COMPOSER_ERR_REQUEST, "/chords/0", "the theme has bars 0 to 7");
    expect_refused("style", "\"no-such-style.json\"", COMPOSER_ERR_UNSATISFIABLE, "/style", "cannot read style");
    expect_refused("development", "\"wild\"", COMPOSER_ERR_REQUEST, "/development", "development must be melody, balanced or theme");
    expect_refused("development", "3", COMPOSER_ERR_REQUEST, "/development", "development");
    expect_refused("tempo", "500", COMPOSER_ERR_REQUEST, "/tempo", "tempo must be from 20 to 400");
    expect_refused("explain", "\"xml\"", COMPOSER_ERR_REQUEST, "/explain", "explain must be");
    /* A setting out of its limit, one that is unknown, and one of the wrong type, each at its path under /preferences. */
    expect_refused("preferences", "{\"melody\": {\"keep\": 17}}", COMPOSER_ERR_REQUEST, "/preferences/melody/keep", "1 to 16");
    expect_refused("preferences", "{\"motif\": {\"bars\": {\"one\": 0, \"two\": 0}}}", COMPOSER_ERR_REQUEST, "/preferences/motif/bars",
                   "");
    expect_refused("preferences", "{\"bogus\": 1}", COMPOSER_ERR_REQUEST, "/preferences/bogus", "");
    expect_refused("preferences", "3", COMPOSER_ERR_REQUEST, "/preferences", "preferences must be an object");
    /* A length and a construction (B5 of P47), each at its path. */
    expect_refused("bars", "2", COMPOSER_ERR_REQUEST, "/bars", "bars must be 3 to 16, not '2'");
    expect_refused("bars", "17", COMPOSER_ERR_REQUEST, "/bars", "bars must be 3 to 16, not '17'");
    expect_refused("bars", "15", COMPOSER_ERR_UNSATISFIABLE, "/bars", "no construction makes a 15-bar sentence from this motif");
    expect_refused("construction", "[]", COMPOSER_ERR_REQUEST, "/construction", "must be null or");
    expect_refused("construction", "{\"ideas\": {\"bogus\": 1}}", COMPOSER_ERR_REQUEST, "/construction/ideas/bogus", "unknown idea");
    expect_refused("construction", "{\"ideas\": {\"basic idea\": 2, \"repetition\": 2, \"fragment 1\": 1, \"fragment 2\": 1, "
                                  "\"cadential\": 3}}",
                   COMPOSER_ERR_UNSATISFIABLE, "/construction/ideas", "the ideas are 9 bars; the theme has 8");
    expect_refused("construction",
                   "{\"ideas\": {\"basic idea\": 2, \"repetition\": 2, \"fragment 1\": 1, \"fragment 2\": 1, \"cadential\": 2}, "
                   "\"extensions\": [{\"kind\": \"echo\"}]}",
                   COMPOSER_ERR_REQUEST, "/construction/extensions/0/kind", "kind must be repeat, stretch, addition or evaded");
    expect_refused("construction",
                   "{\"ideas\": {\"basic idea\": 2, \"repetition\": 2, \"fragment 1\": 1, \"fragment 2\": 1, \"cadential\": 2}, "
                   "\"extensions\": [{\"kind\": \"stretch\", \"idea\": \"fragment 1\", \"bars\": 1}]}",
                   COMPOSER_ERR_UNSATISFIABLE, "/construction/extensions/0", "cannot follow the fragment 1");
    /* A theme no seed can write: a last bar V under a perfect cadence. */
    expect_refused("chords", "[{\"bar\": 7, \"text\": \"V\"}]", COMPOSER_ERR_UNSATISFIABLE, "/chords/0", "chords bar 7: V cannot end a PAC");
    expect_refused("chords", "[{\"bar\": 0, \"text\": \"I\"}, {\"bar\": 7, \"text\": \"I6\"}]", COMPOSER_ERR_UNSATISFIABLE,
                   "/chords/1", "chords bar 7: i6 cannot end a PAC; the cadence needs a tonic chord in root position");

    ComposerResponse response;
    assert(composer_api_theme("{\"api_version\": 7}", NULL, &response) == COMPOSER_ERR_REQUEST);
    assert(strcmp(response.error.path, "/api_version") == 0);
    composer_response_destroy(&response);
    assert(composer_api_theme("not json", NULL, &response) == COMPOSER_ERR_REQUEST);
    composer_response_destroy(&response);
}

/*
 * With the argument "quick" only the cancel and the refused requests run, which stop before a theme is heard: the valgrind
 * run uses it, because hearing a theme leaks in the Analyzer's make_kernel_window (notes.md).
 */
/*
 * No argument runs everything. "quick" runs the refusals only (the valgrind run). "responses" runs the refusals and the cases
 * that write the responses the schema test reads; "runs" runs the setting, progress and thread cases. The two halves are
 * separate tests because every theme measures the fidelity of its ideas (R-332), which takes about 1 s a theme.
 */
int main(int argc, char **argv) {
    const char *part = argc > 1 ? argv[1] : "";
    bool quick = strcmp(part, "quick") == 0;
    bool responses = strcmp(part, "") == 0 || strcmp(part, "responses") == 0;
    bool runs = strcmp(part, "") == 0 || strcmp(part, "runs") == 0;
    if (!quick && !responses && !runs) {
        fprintf(stderr, "test_api_theme: unknown part \"%s\"\n", part);
        return 2;
    }
    if (!runs) {
        test_cancel_first();
        test_refused();
        test_motif_too_long();
        test_part_length_motif();
    }
    if (responses) {
        test_two_bar_motif();
        test_snapshots();
        test_plain_has_no_construction();
        test_uneven();
        test_given_construction();
        test_preferences();
        test_suggest_plans_explain();
        test_developments();
    }
    if (runs) {
        test_development_setting();
        test_progress();
        test_threads();
    }
    printf("test_api_theme: ok\n");
    return 0;
}
