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

/* The developments of a theme carry the segment each idea is made from (or null) and a one-line summary (B2 of P34). */
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
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, developments) {
        const cJSON *idea = cJSON_GetObjectItemCaseSensitive(item, "idea");
        const cJSON *segment = cJSON_GetObjectItemCaseSensitive(item, "segment");
        const cJSON *summary = cJSON_GetObjectItemCaseSensitive(item, "summary");
        assert(cJSON_IsString(idea) && (cJSON_IsString(segment) || cJSON_IsNull(segment)));
        assert(cJSON_IsString(summary) && strncmp(summary->valuestring, idea->valuestring, strlen(idea->valuestring)) == 0);
        assert(strstr(summary->valuestring, " = ") != NULL);
    }
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
int main(int argc, char **argv) {
    bool quick = argc > 1 && strcmp(argv[1], "quick") == 0;
    test_cancel_first();
    test_refused();
    test_motif_too_long();
    test_part_length_motif();
    if (!quick) {
        test_two_bar_motif();
        test_snapshots();
        test_suggest_plans_explain();
        test_developments();
        test_development_setting();
        test_progress();
        test_threads();
    }
    printf("test_api_theme: ok\n");
    return 0;
}
