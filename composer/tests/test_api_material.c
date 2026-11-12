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
 * The material entries (docs/api.md): progression, counterpoint, develop and grow, each run from one request with the
 * document, the plan and the MIDI file equal to the snapshots of the CLI cases of the same name (tests/cli_snapshots.cmake);
 * the requests they refuse, with their paths; grow's corpus seed; and progress and cancel in grow. The requests and responses
 * are written beside the test, for the schema test. With the argument "quick" only the requests that need no hearing run.
 */

#define FIXTURE(name) COMPOSER_FIXTURE_DIR "/" name
#define SNAPSHOT(name) COMPOSER_SNAPSHOT_DIR "/" name

typedef ComposerStatus (*Entry)(const char *, const ComposerJob *, ComposerResponse *);

static bool quick;

/* --- requests and responses ----------------------------------------------------------------------- */

static cJSON *load_json(const char *path) {
    char *text = test_read_file(path, NULL);
    cJSON *json = cJSON_Parse(text);
    assert(json);
    free(text);
    return json;
}

static char *print(cJSON *request) {
    char *printed = cJSON_PrintUnformatted(request);
    assert(printed);
    cJSON_Delete(request);
    return printed;
}

/* The text of a fixture, one line. */
static char *line_of(const char *path) {
    char *text = test_read_file(path, NULL);
    for (char *c = text; *c; c++) {
        if (*c == '\n') {
            *c = ' ';
        }
    }
    return text;
}

/* {"input": {"text": the fixture's text}, ...}: the members are added by the caller. */
static cJSON *motive_request(const char *fixture) {
    char *text = line_of(fixture);
    cJSON *request = cJSON_CreateObject();
    cJSON *input = request ? cJSON_AddObjectToObject(request, "input") : NULL;
    assert(input && cJSON_AddStringToObject(input, "text", text));
    free(text);
    return request;
}

static void write_pair(const char *kind, const char *tag, const char *request, const ComposerResponse *response) {
    char path[160];
    snprintf(path, sizeof path, "api_%s_request_%s.json", kind, tag);
    test_write_file(path, request);
    snprintf(path, sizeof path, "api_%s_response_%s.json", kind, tag);
    test_write_file(path, response->json);
}

static ComposerStatus call(Entry entry, cJSON *request, const ComposerJob *job, ComposerResponse *response, char **printed) {
    *printed = print(request);
    ComposerStatus status = entry(*printed, job, response);
    if (status != COMPOSER_OK && !job) {
        fprintf(stderr, "%s: %s\n", composer_status_name(status), response->error.message);
    }
    return status;
}

/* Runs a request that must succeed and writes the pair for the schema test. */
static void succeed(Entry entry, const char *kind, const char *tag, cJSON *request, ComposerResponse *response) {
    char *printed;
    assert(call(entry, request, NULL, response, &printed) == COMPOSER_OK);
    assert(response->json);
    write_pair(kind, tag, printed, response);
    free(printed);
}

/* A request the entry refuses: the status, the path of the member at fault and a part of the message. */
static void expect_refused(Entry entry, cJSON *request, ComposerStatus status, const char *path, const char *message) {
    ComposerResponse response;
    char *printed = print(request);
    ComposerStatus got = entry(printed, NULL, &response);
    if (got != status || strcmp(response.error.path, path) != 0 || !strstr(response.error.message, message)) {
        fprintf(stderr, "%s: wanted %s at '%s' with '%s', got %s at '%s': %s\n", printed, composer_status_name(status), path,
                message, composer_status_name(got), response.error.path, response.error.message);
    }
    assert(got == status && strcmp(response.error.path, path) == 0 && strstr(response.error.message, message));
    assert(!response.json && !response.plan_json && !response.midi && !response.musicxml);
    composer_response_destroy(&response);
    free(printed);
}

/* --- comparisons ---------------------------------------------------------------------------------- */

/* The document equals the stdout snapshot of the CLI case, which also lists the files the command wrote. */
static void expect_document(const char *json, const char *snapshot) {
    cJSON *actual = cJSON_Parse(json);
    cJSON *expected = load_json(snapshot);
    assert(actual && expected);
    cJSON_DeleteItemFromObjectCaseSensitive(expected, "files");
    if (!cJSON_Compare(actual, expected, true)) {
        char *left = cJSON_PrintUnformatted(actual);
        char *right = cJSON_PrintUnformatted(expected);
        fprintf(stderr, "the document differs from %s:\n%s\n%s\n", snapshot, left, right);
        free(left);
        free(right);
    }
    assert(cJSON_Compare(actual, expected, true));
    cJSON_Delete(actual);
    cJSON_Delete(expected);
}

/* A plan equals a snapshot plan: the versions the plan records are not part of the snapshot (cli_snapshot_compare.cmake). */
static void expect_plan(const char *plan_text, const char *snapshot) {
    cJSON *left = cJSON_Parse(plan_text);
    cJSON *right = load_json(snapshot);
    assert(left);
    cJSON *both[2] = {left, right};
    for (int i = 0; i < 2; i++) {
        cJSON *metadata = cJSON_GetObjectItemCaseSensitive(both[i], "metadata");
        cJSON_DeleteItemFromObjectCaseSensitive(metadata, "composer_version");
        cJSON_DeleteItemFromObjectCaseSensitive(metadata, "ursatz_commit");
    }
    if (!cJSON_Compare(left, right, true)) {
        fprintf(stderr, "a plan differs from %s\n", snapshot);
    }
    assert(cJSON_Compare(left, right, true));
    cJSON_Delete(left);
    cJSON_Delete(right);
}

static void expect_file(const unsigned char *data, size_t size, const char *snapshot) {
    size_t expected_size = 0;
    char *expected = test_read_file(snapshot, &expected_size);
    if (!data || size != expected_size || memcmp(data, expected, size) != 0) {
        fprintf(stderr, "a file differs from %s\n", snapshot);
    }
    assert(data && size == expected_size && memcmp(data, expected, size) == 0);
    free(expected);
}

/* --- progression ---------------------------------------------------------------------------------- */

static void test_progression(void) {
    ComposerResponse response;
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddStringToObject(request, "key", "D minor") && cJSON_AddStringToObject(request, "meter", "4/4") &&
           cJSON_AddNumberToObject(request, "bars", 8) && cJSON_AddStringToObject(request, "cadence", "PAC") &&
           cJSON_AddNumberToObject(request, "seed", 7));
    succeed(composer_api_progression, "progression", "snapshot", request, &response);
    expect_document(response.json, SNAPSHOT("progression.stdout.json"));
    assert(response.plan_json && !response.midi && !response.musicxml);
    composer_response_destroy(&response);

    /* Alternatives, their plans and the files of the first. */
    request = cJSON_CreateObject();
    assert(request && cJSON_AddStringToObject(request, "key", "D minor") && cJSON_AddStringToObject(request, "meter", "4/4") &&
           cJSON_AddStringToObject(request, "bars", "8") && cJSON_AddStringToObject(request, "cadence", "PAC") &&
           cJSON_AddNumberToObject(request, "seed", 7) && cJSON_AddStringToObject(request, "embellish", "medium") &&
           cJSON_AddNumberToObject(request, "suggest", 3) && cJSON_AddTrueToObject(request, "plans") &&
           cJSON_AddTrueToObject(request, "midi") && cJSON_AddTrueToObject(request, "musicxml"));
    succeed(composer_api_progression, "progression", "alternatives", request, &response);
    cJSON *document = cJSON_Parse(response.json);
    cJSON *alternatives = cJSON_GetObjectItemCaseSensitive(document, "alternatives");
    assert(cJSON_GetArraySize(alternatives) == 3);
    cJSON *first = cJSON_GetArrayItem(alternatives, 0);
    assert(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(first, "plan")) &&
           cJSON_IsString(cJSON_GetObjectItemCaseSensitive(first, "musicxml")));
    cJSON *plan = cJSON_Parse(response.plan_json);
    assert(cJSON_Compare(plan, cJSON_GetObjectItemCaseSensitive(first, "plan"), true));
    cJSON_Delete(plan);
    cJSON_Delete(document);
    assert(response.midi_size > 14 && memcmp(response.midi, "MThd", 4) == 0 && response.musicxml);
    composer_response_destroy(&response);

    /* The names of the schemas and sequences. */
    request = cJSON_CreateObject();
    assert(request && cJSON_AddTrueToObject(request, "list_schemas"));
    succeed(composer_api_progression, "progression", "names", request, &response);
    document = cJSON_Parse(response.json);
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(document, "schemas")) > 0 &&
           cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(document, "sequences")) > 0);
    assert(strstr(response.json, "\"romanesca\""));
    cJSON_Delete(document);
    composer_response_destroy(&response);
}

static cJSON *progression_base(const char *key, const char *meter, int bars, const char *cadence) {
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddStringToObject(request, "key", key) && cJSON_AddStringToObject(request, "meter", meter) &&
           cJSON_AddNumberToObject(request, "bars", bars) && cJSON_AddStringToObject(request, "cadence", cadence));
    return request;
}

static void test_progression_refusals(void) {
    expect_refused(composer_api_progression, cJSON_CreateObject(), COMPOSER_ERR_REQUEST, "", "needs key, meter, bars and cadence");
    expect_refused(composer_api_progression, progression_base("H major", "4/4", 8, "PAC"), COMPOSER_ERR_REQUEST, "/key",
                   "unknown key 'H major'");
    expect_refused(composer_api_progression, progression_base("C major", "5/4", 8, "PAC"), COMPOSER_ERR_REQUEST, "/meter",
                   "not supported");
    expect_refused(composer_api_progression, progression_base("C major", "4/4", 65, "PAC"), COMPOSER_ERR_REQUEST, "/bars",
                   "bars must be a whole number from 1 to 64");
    expect_refused(composer_api_progression, progression_base("C major", "4/4", 8, "XYZ"), COMPOSER_ERR_REQUEST, "/cadence",
                   "unknown cadence 'XYZ'");
    cJSON *request = progression_base("C major", "4/4", 8, "PAC");
    assert(cJSON_AddStringToObject(request, "embellish", "loud"));
    expect_refused(composer_api_progression, request, COMPOSER_ERR_REQUEST, "/embellish", "embellish must be none, low");
    request = progression_base("C major", "4/4", 8, "PAC");
    assert(cJSON_AddStringToObject(request, "schema", "nonsense"));
    expect_refused(composer_api_progression, request, COMPOSER_ERR_REQUEST, "/schema", "unknown schema 'nonsense'");
    request = progression_base("C major", "4/4", 8, "PAC");
    assert(cJSON_AddStringToObject(request, "schema", "romanesca") && cJSON_AddFalseToObject(request, "bass"));
    expect_refused(composer_api_progression, request, COMPOSER_ERR_REQUEST, "/bass", "write their own bass");
    request = progression_base("C major", "4/4", 8, "PAC");
    assert(cJSON_AddFalseToObject(request, "bass") && cJSON_AddTrueToObject(request, "midi"));
    expect_refused(composer_api_progression, request, COMPOSER_ERR_REQUEST, "/bass", "write the bass line");
    request = progression_base("C major", "4/4", 8, "PAC");
    assert(cJSON_AddNumberToObject(request, "suggest", 9));
    expect_refused(composer_api_progression, request, COMPOSER_ERR_REQUEST, "/suggest", "suggest must be a whole number from 1 to 8");
    expect_refused(composer_api_progression, progression_base("C major", "4/4", 1, "PAC"), COMPOSER_ERR_UNSATISFIABLE, "/bars",
                   "too short");
    request = progression_base("C major", "3/4", 8, "PAC");
    assert(cJSON_AddStringToObject(request, "schema", "romanesca"));
    expect_refused(composer_api_progression, request, COMPOSER_ERR_UNSATISFIABLE, "", "could not be used for C major, 3/4");
}

/* --- counterpoint --------------------------------------------------------------------------------- */

static cJSON *counterpoint_request(int species) {
    cJSON *request = motive_request(FIXTURE("cantus_d_dorian.txt"));
    assert(cJSON_AddNumberToObject(request, "species", species) && cJSON_AddStringToObject(request, "key", "D") &&
           cJSON_AddStringToObject(request, "mode", "minor") && cJSON_AddStringToObject(request, "meter", "4/4") &&
           cJSON_AddNumberToObject(request, "seed", 1));
    return request;
}

static void test_counterpoint(void) {
    ComposerResponse response;
    const char *tags[] = {"species_1", "species_3"};
    const int species[] = {1, 3};
    const char *snapshots[] = {SNAPSHOT("counterpoint-species-1.stdout.json"), SNAPSHOT("counterpoint-species-3.stdout.json")};
    for (int i = 0; i < (quick ? 1 : 2); i++) {
        succeed(composer_api_counterpoint, "counterpoint", tags[i], counterpoint_request(species[i]), &response);
        expect_document(response.json, snapshots[i]);
        composer_response_destroy(&response);
    }

    /* Several counter-melodies with their plans and the files of the first. */
    cJSON *request = counterpoint_request(1);
    assert(cJSON_AddNumberToObject(request, "suggest", 2) && cJSON_AddStringToObject(request, "position", "above") &&
           cJSON_AddTrueToObject(request, "plans") && cJSON_AddTrueToObject(request, "midi") &&
           cJSON_AddTrueToObject(request, "musicxml"));
    succeed(composer_api_counterpoint, "counterpoint", "suggest", request, &response);
    cJSON *document = cJSON_Parse(response.json);
    cJSON *results = cJSON_GetObjectItemCaseSensitive(document, "results");
    assert(cJSON_GetArraySize(results) == 2 && strcmp(cJSON_GetObjectItemCaseSensitive(document, "position")->valuestring, "above") == 0);
    assert(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(results, 1), "plan")));
    assert(response.plan_json && response.midi_size > 14 && response.musicxml);
    cJSON_Delete(document);
    composer_response_destroy(&response);
}

static void test_counterpoint_refusals(void) {
    cJSON *request = counterpoint_request(5);
    expect_refused(composer_api_counterpoint, request, COMPOSER_ERR_REQUEST, "/species", "species must be 1, 2, 3 or 4");
    request = counterpoint_request(1);
    assert(cJSON_AddStringToObject(request, "position", "beside"));
    expect_refused(composer_api_counterpoint, request, COMPOSER_ERR_REQUEST, "/position", "position must be above or below");
    request = counterpoint_request(1);
    cJSON_DeleteItemFromObjectCaseSensitive(request, "mode");
    expect_refused(composer_api_counterpoint, request, COMPOSER_ERR_REQUEST, "/key", "unknown key 'D'");
    request = counterpoint_request(1);
    cJSON_ReplaceItemInObject(request, "mode", cJSON_CreateString("dorian"));
    expect_refused(composer_api_counterpoint, request, COMPOSER_ERR_REQUEST, "/mode", "mode must be major or minor");
    request = counterpoint_request(1);
    cJSON_DeleteItemFromObjectCaseSensitive(request, "meter");
    assert(cJSON_AddStringToObject(request, "meter", "4-4"));
    expect_refused(composer_api_counterpoint, request, COMPOSER_ERR_REQUEST, "/meter", "is not a meter");
    request = counterpoint_request(1);
    assert(cJSON_AddNumberToObject(request, "suggest", 17));
    expect_refused(composer_api_counterpoint, request, COMPOSER_ERR_REQUEST, "/suggest", "suggest must be a whole number from 1 to 16");
    /* A species 3 line needs four beats to a bar. */
    request = counterpoint_request(3);
    cJSON_DeleteItemFromObjectCaseSensitive(request, "meter");
    assert(cJSON_AddStringToObject(request, "meter", "3/4"));
    expect_refused(composer_api_counterpoint, request, COMPOSER_ERR_UNSATISFIABLE, "", "cannot be written as asked");
    /* A line that is not a motive. */
    request = cJSON_CreateObject();
    cJSON *input = cJSON_AddObjectToObject(request, "input");
    assert(input && cJSON_AddStringToObject(input, "text", "not a line") && cJSON_AddNumberToObject(request, "species", 1) &&
           cJSON_AddStringToObject(request, "key", "D minor") && cJSON_AddStringToObject(request, "meter", "4/4"));
    expect_refused(composer_api_counterpoint, request, COMPOSER_ERR_UNSATISFIABLE, "/input/text", "cannot load the input");
}

/* --- develop -------------------------------------------------------------------------------------- */

static cJSON *develop_request(const char *op) {
    cJSON *request = motive_request(FIXTURE("motive_d_minor.txt"));
    assert(cJSON_AddStringToObject(request, "op", op) && cJSON_AddStringToObject(request, "meter", "4/4") &&
           cJSON_AddStringToObject(request, "key", "D minor"));
    return request;
}

static void test_develop(void) {
    ComposerResponse response;
    cJSON *request = develop_request("restate");
    assert(cJSON_AddTrueToObject(request, "tonal") && cJSON_AddNumberToObject(request, "suggest", 2) &&
           cJSON_AddTrueToObject(request, "midi") && cJSON_AddTrueToObject(request, "musicxml"));
    succeed(composer_api_develop, "develop", "restate", request, &response);
    cJSON *document = cJSON_Parse(response.json);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(document, "source")->valuestring, "D4:q F4:e A4:e D5:h") == 0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(document, "operation")->valuestring, "restate") == 0);
    assert(cJSON_GetObjectItemCaseSensitive(document, "seed")->valuedouble == 0);
    cJSON *candidates = cJSON_GetObjectItemCaseSensitive(document, "candidates");
    assert(cJSON_GetArraySize(candidates) == 2);
    cJSON *best = cJSON_GetArrayItem(candidates, 0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(best, "label")->valuestring, "restate exact") == 0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(best, "text")->valuestring, "D4:q F4:e A4:e D5:h") == 0);
    assert(cJSON_GetObjectItemCaseSensitive(best, "score_numerator")->valuedouble == 1 &&
           cJSON_GetObjectItemCaseSensitive(best, "score_denominator")->valuedouble == 1);
    cJSON *second = cJSON_GetArrayItem(candidates, 1);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(second, "text")->valuestring, "A3:q C4:e E4:e A4:h") == 0);
    cJSON_Delete(document);
    /* The files hold the source and the best candidate. */
    assert(response.plan_json && response.midi_size > 14 && memcmp(response.midi, "MThd", 4) == 0 && response.musicxml);
    composer_response_destroy(&response);

    request = develop_request("retrograde");
    assert(cJSON_AddNumberToObject(request, "seed", 3));
    succeed(composer_api_develop, "develop", "retrograde", request, &response);
    assert(!response.midi && strstr(response.json, "\"D5:h"));
    composer_response_destroy(&response);
}

static void test_develop_refusals(void) {
    cJSON *request = motive_request(FIXTURE("motive_d_minor.txt"));
    expect_refused(composer_api_develop, request, COMPOSER_ERR_REQUEST, "/op", "the request needs \"op\"");
    expect_refused(composer_api_develop, develop_request("sing"), COMPOSER_ERR_REQUEST, "/op", "unknown operation 'sing'");
    request = develop_request("retrograde");
    assert(cJSON_AddTrueToObject(request, "tonal"));
    expect_refused(composer_api_develop, request, COMPOSER_ERR_REQUEST, "/tonal", "tonal does not apply to retrograde");
    request = develop_request("fragment");
    assert(cJSON_AddStringToObject(request, "ratio", "2:1"));
    expect_refused(composer_api_develop, request, COMPOSER_ERR_REQUEST, "/ratio", "ratio does not apply to fragment");
    request = develop_request("restate");
    assert(cJSON_AddNumberToObject(request, "by", 0));
    expect_refused(composer_api_develop, request, COMPOSER_ERR_REQUEST, "", "by must be a whole number");
    request = develop_request("restate");
    assert(cJSON_AddNumberToObject(request, "degree", 1));
    expect_refused(composer_api_develop, request, COMPOSER_ERR_REQUEST, "", "already on degree 1");
    request = develop_request("retrograde");
    assert(cJSON_AddStringToObject(request, "seed", "x"));
    expect_refused(composer_api_develop, request, COMPOSER_ERR_REQUEST, "/seed", "seed must be a whole number");
    request = develop_request("augment");
    assert(cJSON_AddStringToObject(request, "ratio", "3"));
    expect_refused(composer_api_develop, request, COMPOSER_ERR_REQUEST, "", "ratio must be n:m");
    request = develop_request("fragment");
    assert(cJSON_AddNumberToObject(request, "length", 4));
    expect_refused(composer_api_develop, request, COMPOSER_ERR_UNSATISFIABLE, "", "no fragment development");
    /* A key is needed to move by degrees. */
    request = motive_request(FIXTURE("motive_d_minor.txt"));
    assert(cJSON_AddStringToObject(request, "op", "restate") && cJSON_AddStringToObject(request, "meter", "4/4") &&
           cJSON_AddTrueToObject(request, "tonal"));
    expect_refused(composer_api_develop, request, COMPOSER_ERR_REQUEST, "", "tonal needs a key");
}

/* --- grow ----------------------------------------------------------------------------------------- */

static cJSON *grow_request(const char *into) {
    cJSON *request = motive_request(FIXTURE("motive_d_minor.txt"));
    assert(cJSON_AddStringToObject(request, "into", into) && cJSON_AddStringToObject(request, "key", "D minor") &&
           cJSON_AddStringToObject(request, "meter", "4/4") && cJSON_AddNumberToObject(request, "seed", 7));
    return request;
}

static void test_grow(void) {
    const char *intos[] = {"sentence", "period"};
    for (int i = 0; i < (quick ? 0 : 2); i++) {
        ComposerResponse response;
        char name[80];
        cJSON *request = grow_request(intos[i]);
        assert(cJSON_AddTrueToObject(request, "midi") && cJSON_AddTrueToObject(request, "plans"));
        succeed(composer_api_grow, "grow", intos[i], request, &response);
        /* The CLI case of the same name: the document, the plan and the MIDI file. */
        cJSON *document = cJSON_Parse(response.json);
        cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
        assert(cJSON_GetArraySize(themes) == 1);
        cJSON *plan = cJSON_DetachItemFromObjectCaseSensitive(cJSON_GetArrayItem(themes, 0), "plan");
        assert(plan);
        char *document_text = cJSON_PrintUnformatted(document);
        snprintf(name, sizeof name, "grow-%s.stdout.json", intos[i]);
        char path[200];
        snprintf(path, sizeof path, "%s/%s", COMPOSER_SNAPSHOT_DIR, name);
        expect_document(document_text, path);
        free(document_text);
        char *plan_text = cJSON_PrintUnformatted(plan);
        snprintf(path, sizeof path, "%s/grow-%s.plan.json", COMPOSER_SNAPSHOT_DIR, intos[i]);
        expect_plan(plan_text, path);
        expect_plan(response.plan_json, path);
        free(plan_text);
        snprintf(path, sizeof path, "%s/grow-%s.mid", COMPOSER_SNAPSHOT_DIR, intos[i]);
        expect_file(response.midi, response.midi_size, path);
        cJSON_Delete(plan);
        cJSON_Delete(document);
        composer_response_destroy(&response);
    }
    if (quick) {
        return;
    }

    /* Several themes: the plan, the explanation and the MusicXML of each, and a notice when fewer were found. */
    ComposerResponse response;
    cJSON *request = grow_request("period");
    assert(cJSON_AddNumberToObject(request, "suggest", 2) && cJSON_AddTrueToObject(request, "plans") &&
           cJSON_AddTrueToObject(request, "musicxml") && cJSON_AddStringToObject(request, "explain", "text") &&
           cJSON_AddStringToObject(request, "accompaniment", "block"));
    succeed(composer_api_grow, "grow", "suggest", request, &response);
    cJSON *document = cJSON_Parse(response.json);
    cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
    assert(cJSON_GetArraySize(themes) == 2);
    cJSON *theme = NULL;
    cJSON_ArrayForEach(theme, themes) {
        assert(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(theme, "plan")) &&
               cJSON_IsString(cJSON_GetObjectItemCaseSensitive(theme, "explanation")) &&
               cJSON_IsString(cJSON_GetObjectItemCaseSensitive(theme, "musicxml")));
    }
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(document, "accompaniment")->valuestring, "block") == 0);
    assert(response.plan_json && response.musicxml && !response.midi);
    cJSON_Delete(document);
    composer_response_destroy(&response);
}

/* A motive seeded from a corpus: the passage is reported as "source". */
static void test_grow_corpus(void) {
#ifdef COMPOSER_SONATINAS_CORPUS
    ComposerResponse response;
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddStringToObject(request, "corpus", COMPOSER_SONATINAS_CORPUS) &&
           cJSON_AddStringToObject(request, "like", FIXTURE("mozart545-bar1.mid")) &&
           cJSON_AddStringToObject(request, "into", "sentence") && cJSON_AddStringToObject(request, "key", "D major") &&
           cJSON_AddStringToObject(request, "meter", "4/4") && cJSON_AddNumberToObject(request, "seed", 1));
    succeed(composer_api_grow, "grow", "corpus", request, &response);
    cJSON *document = cJSON_Parse(response.json);
    cJSON *source = cJSON_GetObjectItemCaseSensitive(document, "source");
    assert(cJSON_IsObject(source) && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(source, "piece")) &&
           cJSON_IsString(cJSON_GetObjectItemCaseSensitive(source, "score")));
    cJSON_Delete(document);
    composer_response_destroy(&response);

    request = cJSON_CreateObject();
    assert(request && cJSON_AddStringToObject(request, "corpus", COMPOSER_EMPTY_CORPUS) &&
           cJSON_AddStringToObject(request, "like", FIXTURE("mozart545-bar1.mid")) &&
           cJSON_AddStringToObject(request, "into", "sentence") && cJSON_AddStringToObject(request, "key", "D major") &&
           cJSON_AddStringToObject(request, "meter", "4/4"));
    expect_refused(composer_api_grow, request, COMPOSER_ERR_UNSATISFIABLE, "/like", "is similar to");
#endif
}

static void test_grow_refusals(void) {
    cJSON *request = grow_request("rondo");
    expect_refused(composer_api_grow, request, COMPOSER_ERR_REQUEST, "/into", "into must be sentence, period or hybrid");
    request = grow_request("hybrid");
    expect_refused(composer_api_grow, request, COMPOSER_ERR_REQUEST, "/hybrid", "needs hybrid");
    request = grow_request("sentence");
    assert(cJSON_AddStringToObject(request, "hybrid", "cbi+cadential"));
    expect_refused(composer_api_grow, request, COMPOSER_ERR_REQUEST, "/hybrid", "applies only to into hybrid");
    request = grow_request("sentence");
    assert(cJSON_AddStringToObject(request, "cadence", "DC"));
    expect_refused(composer_api_grow, request, COMPOSER_ERR_REQUEST, "/cadence", "cadence must be PAC, IAC or HC");
    request = grow_request("sentence");
    assert(cJSON_AddNumberToObject(request, "bars", 2));
    expect_refused(composer_api_grow, request, COMPOSER_ERR_REQUEST, "/bars", "bars must be 3 to 16");
    request = grow_request("sentence");
    assert(cJSON_AddNumberToObject(request, "suggest", 17));
    expect_refused(composer_api_grow, request, COMPOSER_ERR_REQUEST, "/suggest", "suggest must be a whole number from 1 to 16");
    request = grow_request("sentence");
    cJSON *without = cJSON_AddArrayToObject(request, "style_without");
    assert(without && cJSON_AddItemToArray(without, cJSON_CreateString("motive-degrees")));
    expect_refused(composer_api_grow, request, COMPOSER_ERR_REQUEST, "/without", "style_without needs style");
    request = grow_request("sentence");
    without = cJSON_AddArrayToObject(request, "style_without");
    assert(without && cJSON_AddItemToArray(without, cJSON_CreateString("no-such-category")));
    expect_refused(composer_api_grow, request, COMPOSER_ERR_REQUEST, "/style_without", "is not a style category");
    request = grow_request("sentence");
    assert(cJSON_AddStringToObject(request, "mode", "style"));
    expect_refused(composer_api_grow, request, COMPOSER_ERR_REQUEST, "/mode", "mode style needs style");
    request = grow_request("sentence");
    assert(cJSON_AddStringToObject(request, "like", "passage.mid"));
    expect_refused(composer_api_grow, request, COMPOSER_ERR_REQUEST, "/like", "like needs corpus");
    request = grow_request("sentence");
    assert(cJSON_AddStringToObject(request, "corpus", "corpus") && cJSON_AddStringToObject(request, "like", "passage.mid"));
    expect_refused(composer_api_grow, request, COMPOSER_ERR_REQUEST, "/corpus", "not both");
    request = cJSON_CreateObject();
    assert(request && cJSON_AddStringToObject(request, "into", "sentence") && cJSON_AddStringToObject(request, "key", "D minor") &&
           cJSON_AddStringToObject(request, "meter", "4/4"));
    expect_refused(composer_api_grow, request, COMPOSER_ERR_REQUEST, "/input", "needs an input");
    /* A motive longer than the basic idea (two bars of eight). */
    request = grow_request("sentence");
    cJSON_ReplaceItemInObject(request, "meter", cJSON_CreateString("3/4"));
    cJSON_ReplaceItemInObject(cJSON_GetObjectItemCaseSensitive(request, "input"), "text", cJSON_CreateString("D4:w D4:w D4:q"));
    expect_refused(composer_api_grow, request, COMPOSER_ERR_UNSATISFIABLE, "", "the motif is 3 bars; a 8-bar theme takes a motif of up to 2 bars, or of 4 for its presentation or antecedent");
}

/* --- progress and cancel -------------------------------------------------------------------------- */

typedef struct {
    unsigned reports;
    bool saw_grow;
    bool cancel;
} Watch;

static void progress(void *user, const char *stage, unsigned done, unsigned total) {
    Watch *watch = user;
    (void)done;
    (void)total;
    watch->reports++;
    watch->saw_grow = watch->saw_grow || strcmp(stage, "grow") == 0;
}

static bool cancelled(void *user) {
    return ((Watch *)user)->cancel;
}

/* Grow reports its stages, and a job that says cancel stops it with COMPOSER_ERR_CANCELLED and no document. */
static void test_cancel(void) {
    ComposerResponse response;
    char *printed;
    Watch quiet = {0, false, false};
    ComposerJob watching = {progress, cancelled, &quiet};
    if (!quick) {
        assert(call(composer_api_grow, grow_request("sentence"), &watching, &response, &printed) == COMPOSER_OK);
        assert(quiet.reports >= 1 && quiet.saw_grow);
        composer_response_destroy(&response);
        free(printed);
    }

    Watch watch = {0, false, true};
    ComposerJob job = {progress, cancelled, &watch};
    ComposerStatus status = call(composer_api_grow, grow_request("sentence"), &job, &response, &printed);
    if (status != COMPOSER_ERR_CANCELLED) {
        fprintf(stderr, "grow: %s: %s\n", composer_status_name(status), response.error.message);
    }
    assert(status == COMPOSER_ERR_CANCELLED && !response.json && !response.plan_json && !response.midi);
    assert(watch.reports >= 1);
    composer_response_destroy(&response);
    free(printed);

    /* The others stop at their first report. */
    struct {
        Entry entry;
        cJSON *request;
        const char *stage;
    } others[3];
    others[0].entry = composer_api_progression;
    others[0].request = progression_base("D minor", "4/4", 8, "PAC");
    others[0].stage = "progression";
    others[1].entry = composer_api_counterpoint;
    others[1].request = counterpoint_request(1);
    others[1].stage = "counterpoint";
    others[2].entry = composer_api_develop;
    others[2].request = develop_request("retrograde");
    others[2].stage = "develop";
    for (int i = 0; i < 3; i++) {
        Watch stop = {0, false, true};
        ComposerJob stopping = {progress, cancelled, &stop};
        status = call(others[i].entry, others[i].request, &stopping, &response, &printed);
        if (status != COMPOSER_ERR_CANCELLED) {
            fprintf(stderr, "%s: %s: %s\n", others[i].stage, composer_status_name(status), response.error.message);
        }
        assert(status == COMPOSER_ERR_CANCELLED && !response.json && !response.plan_json && !response.midi);
        assert(stop.reports >= 1);
        composer_response_destroy(&response);
        free(printed);
    }
}

static void test_version(void) {
    assert(strcmp(composer_api_version(), "1.17") == 0);
}

int main(int argc, char **argv) {
    quick = argc > 1 && strcmp(argv[1], "quick") == 0;
    test_version();
    test_progression();
    test_progression_refusals();
    test_counterpoint();
    test_counterpoint_refusals();
    test_develop();
    test_develop_refusals();
    test_grow();
    test_grow_corpus();
    test_grow_refusals();
    test_cancel();
    return 0;
}
