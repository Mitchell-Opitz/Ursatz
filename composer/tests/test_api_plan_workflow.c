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
 * The plan workflow entries (docs/api.md): plan, fill, lock, regenerate, variations and harmonize, each run from its request
 * with the document, the plan, the MIDI file and the MusicXML equal to the snapshots of the CLI cases of the same name
 * (tests/cli_snapshots.cmake); the targets the entries refuse, with their paths; the report of a plan no seed completes; and
 * a cancel. The requests and responses are written beside the test, for the schema test. With the argument "quick" only the
 * requests that stop before a plan is heard run, because hearing leaks in the Analyzer (notes.md).
 */

#define FIXTURE(name) COMPOSER_FIXTURE_DIR "/" name
#define SNAPSHOT(name) COMPOSER_SNAPSHOT_DIR "/" name

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

/* {"plan": the snapshot plan, ...}: the members are added by the caller. */
static cJSON *plan_request(const char *snapshot_plan) {
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "plan", load_json(snapshot_plan)));
    return request;
}

static void write_pair(const char *kind, const char *tag, const char *request, const ComposerResponse *response) {
    char path[160];
    snprintf(path, sizeof path, "api_%s_request_%s.json", kind, tag);
    test_write_file(path, request);
    snprintf(path, sizeof path, "api_%s_response_%s.json", kind, tag);
    test_write_file(path, response->json);
}

static ComposerStatus call(ComposerStatus (*entry)(const char *, const ComposerJob *, ComposerResponse *), cJSON *request,
                           const ComposerJob *job, ComposerResponse *response, char **printed) {
    *printed = print(request);
    ComposerStatus status = entry(*printed, job, response);
    if (status != COMPOSER_OK && !job && !response->json) {
        fprintf(stderr, "%s: %s\n", composer_status_name(status), response->error.message);
    }
    return status;
}

/* --- comparisons ---------------------------------------------------------------------------------- */

/* A copy of the JSON with the fixtures directory written as the snapshots write it (cli_snapshots.cmake). */
static cJSON *normalized(const cJSON *json) {
    char *text = cJSON_PrintUnformatted(json);
    assert(text);
    const char *directory = COMPOSER_FIXTURE_DIR;
    size_t length = strlen(directory);
    char *out = malloc(strlen(text) + 1);
    assert(out);
    size_t used = 0;
    for (const char *c = text; *c;) {
        if (strncmp(c, directory, length) == 0) {
            memcpy(out + used, "<FIXTURES>", 10);
            used += 10;
            c += length;
        } else {
            out[used++] = *c++;
        }
    }
    out[used] = '\0';
    cJSON *copy = cJSON_Parse(out);
    assert(copy);
    free(text);
    free(out);
    return copy;
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
    cJSON *actual = normalized(left);
    if (!cJSON_Compare(actual, right, true)) {
        fprintf(stderr, "a plan differs from %s\n", snapshot);
    }
    assert(cJSON_Compare(actual, right, true));
    cJSON_Delete(actual);
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

static void expect_same(const cJSON *actual, const cJSON *expected, const char *what) {
    if (!cJSON_Compare(actual, expected, true)) {
        char *left = cJSON_PrintUnformatted(actual);
        char *right = cJSON_PrintUnformatted(expected);
        fprintf(stderr, "%s differs:\n%s\n%s\n", what, left, right);
        free(left);
        free(right);
    }
    assert(cJSON_Compare(actual, expected, true));
}

/*
 * The document equals the snapshot of the case's stdout without the members the CLI adds (the files written) or only the
 * text output reads; remove names them.
 */
static void expect_document(const char *json, const char *snapshot, const char *const *remove_actual,
                            const char *const *remove_expected) {
    cJSON *parsed = cJSON_Parse(json);
    assert(parsed);
    cJSON *actual = normalized(parsed);
    cJSON_Delete(parsed);
    cJSON *expected = load_json(snapshot);
    for (; remove_actual && *remove_actual; remove_actual++) {
        cJSON_DeleteItemFromObjectCaseSensitive(actual, *remove_actual);
    }
    for (; remove_expected && *remove_expected; remove_expected++) {
        cJSON_DeleteItemFromObjectCaseSensitive(expected, *remove_expected);
    }
    expect_same(actual, expected, snapshot);
    cJSON_Delete(actual);
    cJSON_Delete(expected);
}

/* The response of a composed plan equals the snapshots of the case: document, plan, MIDI and MusicXML. */
static void expect_case(const char *name, const ComposerResponse *response, const char *const *remove_actual,
                        const char *const *remove_expected, bool midi, bool musicxml) {
    char path[256];
    snprintf(path, sizeof path, SNAPSHOT("%s.stdout.json"), name);
    expect_document(response->json, path, remove_actual, remove_expected);
    snprintf(path, sizeof path, SNAPSHOT("%s.plan.json"), name);
    expect_plan(response->plan_json, path);
    if (midi) {
        snprintf(path, sizeof path, SNAPSHOT("%s.mid"), name);
        expect_file(response->midi, response->midi_size, path);
    } else {
        assert(!response->midi);
    }
    if (musicxml) {
        snprintf(path, sizeof path, SNAPSHOT("%s.musicxml"), name);
        expect_file((const unsigned char *)response->musicxml, response->musicxml ? strlen(response->musicxml) : 0, path);
    } else {
        assert(!response->musicxml);
    }
}

static const char *const FILES_ONLY[] = {"files", NULL};
static const char *const FILE_ONLY[] = {"file", NULL};

/* --- plan ----------------------------------------------------------------------------------------- */

static void test_plan(void) {
    ComposerResponse response;
    char *printed;
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddStringToObject(request, "form", "binary") && cJSON_AddStringToObject(request, "key", "C") &&
           cJSON_AddStringToObject(request, "meter", "4/4") && cJSON_AddNumberToObject(request, "seed", 1));
    assert(call(composer_api_plan, request, NULL, &response, &printed) == COMPOSER_OK);
    expect_case("plan-binary", &response, NULL, FILE_ONLY, false, false);
    if (!quick) {
        write_pair("plan", "binary", printed, &response);
    }
    composer_response_destroy(&response);
    free(printed);

    /* Given material with no form: the form is a ternary form of 24 bars, the melody goes in B. The name to show is the
       motive's own "source". */
    char *melody = test_read_file(FIXTURE("melody_c_major_8_bars.txt"), NULL);
    request = cJSON_CreateObject();
    cJSON *given = cJSON_CreateArray();
    cJSON *item = cJSON_CreateObject();
    cJSON *motive = cJSON_CreateObject();
    cJSON *input = cJSON_CreateObject();
    assert(request && given && item && motive && input);
    assert(cJSON_AddStringToObject(input, "text", melody) && cJSON_AddItemToObject(motive, "input", input) &&
           cJSON_AddStringToObject(motive, "source", FIXTURE("melody_c_major_8_bars.txt")));
    assert(cJSON_AddStringToObject(item, "kind", "melody") && cJSON_AddItemToObject(item, "motive", motive) &&
           cJSON_AddItemToArray(given, item) && cJSON_AddItemToObject(request, "given", given));
    assert(cJSON_AddStringToObject(request, "key", "C") && cJSON_AddStringToObject(request, "meter", "4/4") &&
           cJSON_AddNumberToObject(request, "seed", 1));
    free(melody);
    assert(call(composer_api_plan, request, NULL, &response, &printed) == COMPOSER_OK);
    {
        const char *const defaulted[] = {"defaulted_form", NULL};
        expect_case("plan-given", &response, defaulted, FILE_ONLY, false, false);
        cJSON *document = cJSON_Parse(response.json);
        assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(document, "defaulted_form")));
        cJSON_Delete(document);
    }
    if (!quick) {
        write_pair("plan", "given", printed, &response);
    }
    composer_response_destroy(&response);
    free(printed);
}

static void expect_refused(ComposerStatus (*entry)(const char *, const ComposerJob *, ComposerResponse *), cJSON *request,
                           ComposerStatus status, const char *path, const char *message) {
    ComposerResponse response;
    char *printed = print(request);
    ComposerStatus got = entry(printed, NULL, &response);
    if (got != status || strcmp(response.error.path, path) != 0 || !strstr(response.error.message, message)) {
        fprintf(stderr, "expected %s at '%s' with '%s', got %s at '%s': %s\n", composer_status_name(status), path, message,
                composer_status_name(got), response.error.path, response.error.message);
    }
    assert(got == status && strcmp(response.error.path, path) == 0 && strstr(response.error.message, message));
    assert(!response.plan_json && !response.midi && !response.musicxml);
    composer_response_destroy(&response);
    free(printed);
}

static cJSON *plan_members(const char *form, const char *key) {
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddStringToObject(request, "key", key) && cJSON_AddStringToObject(request, "meter", "4/4"));
    assert(!form || cJSON_AddStringToObject(request, "form", form));
    return request;
}

static void test_plan_refusals(void) {
    expect_refused(composer_api_plan, plan_members("nope", "C"), COMPOSER_ERR_REQUEST, "/form", "unknown form 'nope'");
    cJSON *request = plan_members("binary", "C");
    assert(cJSON_AddNumberToObject(request, "bars", 12));
    expect_refused(composer_api_plan, request, COMPOSER_ERR_REQUEST, "/bars", "binary has no size of 12 bars; it has 8, 16 bars");
    request = plan_members("binary", "C minor");
    assert(cJSON_AddStringToObject(request, "mode", "minor"));
    expect_refused(composer_api_plan, request, COMPOSER_ERR_REQUEST, "/mode", "names a mode already");
    request = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(request, "key", "C") && cJSON_AddStringToObject(request, "meter", "5/4") &&
           cJSON_AddStringToObject(request, "form", "binary"));
    expect_refused(composer_api_plan, request, COMPOSER_ERR_REQUEST, "/meter", "meter '5/4' is not supported");
    request = plan_members("binary", "C");
    assert(cJSON_AddStringToObject(request, "style_weight", "1/2"));
    expect_refused(composer_api_plan, request, COMPOSER_ERR_REQUEST, "/style_weight", "style_weight needs style");
    request = plan_members("binary", "C");
    assert(cJSON_AddStringToObject(request, "style", "no-such-style.json"));
    expect_refused(composer_api_plan, request, COMPOSER_ERR_UNSATISFIABLE, "/style", "cannot read style 'no-such-style.json'");

    /* Given material: where it goes is checked with its index in the path. */
    const char *const bad_at[] = {"Z", "B:0"};
    for (int i = 0; i < 2; i++) {
        request = plan_members("binary", "C");
        cJSON *given = cJSON_CreateArray();
        cJSON *item = cJSON_CreateObject();
        assert(request && given && item && cJSON_AddStringToObject(item, "kind", "progression") &&
               cJSON_AddStringToObject(item, "numerals", "I V") && cJSON_AddStringToObject(item, "at", bad_at[i]));
        assert(cJSON_AddItemToArray(given, item) && cJSON_AddItemToObject(request, "given", given));
        expect_refused(composer_api_plan, request, COMPOSER_ERR_REQUEST, i == 0 ? "/given/0" : "/given/0/at",
                       i == 0 ? "the piece has no section 'Z'; its sections are A, B" : "the bar must be a whole number from 1");
    }
    request = plan_members("binary", "C");
    {
        cJSON *given = cJSON_CreateArray();
        cJSON *item = cJSON_CreateObject();
        assert(request && given && item && cJSON_AddStringToObject(item, "kind", "progression") &&
               cJSON_AddStringToObject(item, "numerals", "I IV X I") && cJSON_AddStringToObject(item, "at", "A"));
        assert(cJSON_AddItemToArray(given, item) && cJSON_AddItemToObject(request, "given", given));
        expect_refused(composer_api_plan, request, COMPOSER_ERR_UNSATISFIABLE, "/given/0", "numeral that cannot be read");
    }
    /* A corpus is needed for a passage to look for. */
    request = plan_members("binary", "C");
    assert(cJSON_AddStringToObject(request, "like", FIXTURE("mozart545-bar1.mid")));
    expect_refused(composer_api_plan, request, COMPOSER_ERR_REQUEST, "", "like needs corpus <dir>");
}

/* --- fill, lock, regenerate, variations, harmonize ------------------------------------------------ */

static char *fill_plan_text; /* the plan of the fill case, the plan the cases below work on */

static void test_fill(void) {
    ComposerResponse response;
    char *printed;
    cJSON *request = plan_request(SNAPSHOT("plan-binary.plan.json"));
    assert(cJSON_AddNumberToObject(request, "seed", 5) && cJSON_AddTrueToObject(request, "midi") &&
           cJSON_AddTrueToObject(request, "musicxml") && cJSON_AddStringToObject(request, "tempo", "100"));
    assert(call(composer_api_fill, request, NULL, &response, &printed) == COMPOSER_OK);
    expect_case("fill", &response, NULL, FILES_ONLY, true, true);
    fill_plan_text = strdup(response.plan_json);
    assert(fill_plan_text);
    write_pair("fill", "plan", printed, &response);
    composer_response_destroy(&response);
    free(printed);

    /* A plan with a given melody: the document says it came through unchanged. */
    request = plan_request(SNAPSHOT("plan-given.plan.json"));
    assert(cJSON_AddNumberToObject(request, "seed", 5));
    assert(call(composer_api_fill, request, NULL, &response, &printed) == COMPOSER_OK);
    expect_case("fill-given", &response, NULL, FILES_ONLY, false, false);
    write_pair("fill", "given", printed, &response);
    composer_response_destroy(&response);
    free(printed);

    /* The records and the explanation arrive as text, the candidates as a list. */
    request = plan_request(SNAPSHOT("plan-binary.plan.json"));
    assert(cJSON_AddNumberToObject(request, "seed", 5) && cJSON_AddNumberToObject(request, "candidates", 2) &&
           cJSON_AddTrueToObject(request, "record") && cJSON_AddStringToObject(request, "explain", "json"));
    assert(call(composer_api_fill, request, NULL, &response, &printed) == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response.json);
    assert(document && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(document, "candidates")) >= 1);
    assert(cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(document, "candidates_tried")));
    assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(document, "record")));
    assert(strncmp(cJSON_GetObjectItemCaseSensitive(document, "record")->valuestring, "[\n{\"stage\":", 11) == 0);
    assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(document, "explanation")));
    cJSON_Delete(document);
    write_pair("fill", "candidates", printed, &response);
    composer_response_destroy(&response);
    free(printed);
}

static void test_lock(void) {
    ComposerResponse response;
    char *printed;
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "plan", cJSON_Parse(fill_plan_text)) &&
           cJSON_AddStringToObject(request, "section", "A"));
    assert(call(composer_api_lock, request, NULL, &response, &printed) == COMPOSER_OK);
    {
        cJSON *document = cJSON_Parse(response.json);
        cJSON *expected = load_json(SNAPSHOT("lock.stdout.json"));
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(document, "what")->valuestring, "section A") == 0);
        expect_same(cJSON_GetObjectItemCaseSensitive(document, "locked"), cJSON_GetObjectItemCaseSensitive(expected, "locked"),
                    "the locked counts");
        cJSON_Delete(document);
        cJSON_Delete(expected);
        expect_plan(response.plan_json, SNAPSHOT("lock.plan.json"));
    }
    write_pair("lock", "section", printed, &response);
    composer_response_destroy(&response);
    free(printed);
}

static void test_regenerate(void) {
    ComposerResponse response;
    char *printed;
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "plan", cJSON_Parse(fill_plan_text)) &&
           cJSON_AddStringToObject(request, "section", "B") && cJSON_AddNumberToObject(request, "seed", 3) &&
           cJSON_AddTrueToObject(request, "midi") && cJSON_AddTrueToObject(request, "musicxml"));
    assert(call(composer_api_regenerate, request, NULL, &response, &printed) == COMPOSER_OK);
    expect_case("regenerate", &response, NULL, FILES_ONLY, true, true);
    write_pair("regenerate", "section", printed, &response);
    composer_response_destroy(&response);
    free(printed);

    /* One lane: the spans reopened are counted by lane. */
    request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "plan", cJSON_Parse(fill_plan_text)) &&
           cJSON_AddStringToObject(request, "section", "A") && cJSON_AddStringToObject(request, "lane", "bass") &&
           cJSON_AddNumberToObject(request, "seed", 3));
    assert(call(composer_api_regenerate, request, NULL, &response, &printed) == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response.json);
    const cJSON *spans = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "reopened"), "spans");
    assert(cJSON_GetObjectItemCaseSensitive(spans, "bass")->valuedouble > 0);
    assert(cJSON_GetObjectItemCaseSensitive(spans, "harmony")->valuedouble == 0);
    cJSON_Delete(document);
    write_pair("regenerate", "lane", printed, &response);
    composer_response_destroy(&response);
    free(printed);
}

/* The versions of a command: each entry's plan, MusicXML and (through the export entry) MIDI file equal the snapshots. */
static void expect_versions(const char *name, const ComposerResponse *response) {
    char path[256];
    snprintf(path, sizeof path, SNAPSHOT("%s.stdout.json"), name);
    cJSON *expected = load_json(path);
    cJSON *actual = cJSON_Parse(response->json);
    assert(actual && !response->midi && !response->musicxml);
    assert(cJSON_GetObjectItemCaseSensitive(actual, "kept")->valuedouble == cJSON_GetObjectItemCaseSensitive(expected, "kept")->valuedouble);
    assert(cJSON_GetObjectItemCaseSensitive(actual, "requested")->valuedouble ==
           cJSON_GetObjectItemCaseSensitive(expected, "requested")->valuedouble);
    assert(cJSON_GetObjectItemCaseSensitive(actual, "seeds_tried")->valuedouble ==
           cJSON_GetObjectItemCaseSensitive(expected, "seeds_tried")->valuedouble);
    int index = 0;
    const cJSON *entry = NULL;
    const cJSON *expected_entry = cJSON_GetObjectItemCaseSensitive(expected, "variations")->child;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(actual, "variations")) {
        index++;
        assert(expected_entry);
        assert(cJSON_GetObjectItemCaseSensitive(entry, "index")->valuedouble == index);
        assert(cJSON_GetObjectItemCaseSensitive(entry, "seed")->valuedouble == cJSON_GetObjectItemCaseSensitive(expected_entry, "seed")->valuedouble);
        expect_same(cJSON_GetObjectItemCaseSensitive(entry, "sections"), cJSON_GetObjectItemCaseSensitive(expected_entry, "sections"),
                    "the sections heard");
        char *plan = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(entry, "plan"));
        snprintf(path, sizeof path, SNAPSHOT("%s.variation-%d.json"), name, index);
        expect_plan(plan, path);
        free(plan);
        snprintf(path, sizeof path, SNAPSHOT("%s.variation-%d.musicxml"), name, index);
        char *musicxml = test_read_file(path, NULL);
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(entry, "musicxml")->valuestring, musicxml) == 0);
        free(musicxml);
        /* The MIDI file of a version: the export entry over its plan. */
        cJSON *export_request = cJSON_CreateObject();
        assert(export_request && cJSON_AddItemToObject(export_request, "plan",
                                                       cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(entry, "plan"), true)));
        assert(cJSON_AddBoolToObject(export_request, "midi", true));
        char *export_text = print(export_request);
        ComposerResponse exported;
        assert(composer_api_export(export_text, NULL, &exported) == COMPOSER_OK);
        snprintf(path, sizeof path, SNAPSHOT("%s.variation-%d.mid"), name, index);
        expect_file(exported.midi, exported.midi_size, path);
        composer_response_destroy(&exported);
        free(export_text);
        expected_entry = expected_entry->next;
    }
    assert(index == (int)cJSON_GetObjectItemCaseSensitive(expected, "kept")->valuedouble);
    cJSON_Delete(actual);
    cJSON_Delete(expected);
}

static void test_variations(void) {
    ComposerResponse response;
    char *printed;
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "plan", cJSON_Parse(fill_plan_text)) &&
           cJSON_AddStringToObject(request, "section", "B") && cJSON_AddNumberToObject(request, "count", 2) &&
           cJSON_AddNumberToObject(request, "seed", 3) && cJSON_AddTrueToObject(request, "musicxml"));
    assert(call(composer_api_variations, request, NULL, &response, &printed) == COMPOSER_OK);
    expect_versions("variations", &response);
    write_pair("variations", "section", printed, &response);
    composer_response_destroy(&response);
    free(printed);

    /* harmonize: the section whose melody is given; a section with no given melody is refused. */
    request = plan_request(SNAPSHOT("fill-given.plan.json"));
    assert(cJSON_AddStringToObject(request, "section", "B") && cJSON_AddNumberToObject(request, "count", 2) &&
           cJSON_AddNumberToObject(request, "seed", 3) && cJSON_AddTrueToObject(request, "musicxml"));
    assert(call(composer_api_harmonize, request, NULL, &response, &printed) == COMPOSER_OK);
    expect_versions("harmonize", &response);
    write_pair("variations", "harmonize", printed, &response);
    composer_response_destroy(&response);
    free(printed);

    request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "plan", cJSON_Parse(fill_plan_text)) &&
           cJSON_AddStringToObject(request, "section", "A") && cJSON_AddNumberToObject(request, "count", 2));
    expect_refused(composer_api_harmonize, request, COMPOSER_ERR_UNSATISFIABLE, "", "section A has no given melody over all its bars");
}

/* A plan no seed completes: the report says where it is stuck. */
static void test_stuck(void) {
    ComposerResponse response;
    char *printed;
    /* A binary form in A minor in 3/4 from a motive whose inversion gets no structure (a known limit of theme development). */
    cJSON *request = cJSON_CreateObject();
    cJSON *motive = cJSON_CreateObject();
    cJSON *input = cJSON_CreateObject();
    assert(request && motive && input && cJSON_AddStringToObject(input, "format", "notes") &&
           cJSON_AddStringToObject(input, "text", "D4:q F4:e A4:e D5:q") && cJSON_AddItemToObject(motive, "input", input) &&
           cJSON_AddStringToObject(request, "form", "binary") && cJSON_AddStringToObject(request, "key", "A minor") &&
           cJSON_AddStringToObject(request, "meter", "3/4") && cJSON_AddItemToObject(request, "motive", motive) &&
           cJSON_AddNumberToObject(request, "seed", 1));
    assert(call(composer_api_plan, request, NULL, &response, &printed) == COMPOSER_OK);
    request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "plan", cJSON_Parse(response.plan_json)) && cJSON_AddNumberToObject(request, "seed", 1));
    composer_response_destroy(&response);
    free(printed);
    ComposerStatus status = call(composer_api_fill, request, NULL, &response, &printed);
    assert(status == COMPOSER_ERR_UNSATISFIABLE && !response.plan_json);
    assert(strstr(response.error.message, "the plan could not be completed from seeds 1 to 16: stuck at the "));
    cJSON *document = cJSON_Parse(response.json);
    const cJSON *stuck = cJSON_GetObjectItemCaseSensitive(document, "stuck");
    assert(stuck && cJSON_GetObjectItemCaseSensitive(stuck, "first")->valuedouble == 1);
    assert(cJSON_GetObjectItemCaseSensitive(stuck, "last")->valuedouble == 16);
    assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(stuck, "stage")));
    const cJSON *open = cJSON_GetObjectItemCaseSensitive(stuck, "open");
    assert(cJSON_GetArraySize(open) > 0 && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(open->child, "reason")));
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(printed);
}

/* --- refusals ------------------------------------------------------------------------------------- */

static void test_refusals(void) {
    const char *plan_path = SNAPSHOT("plan-binary.plan.json");
    /* A plan that is not realized has nothing to lock yet. */
    cJSON *request = plan_request(plan_path);
    assert(cJSON_AddStringToObject(request, "section", "A"));
    expect_refused(composer_api_lock, request, COMPOSER_ERR_UNSATISFIABLE, "", "cannot lock section A: some of it is not written yet");
    request = plan_request(plan_path);
    assert(cJSON_AddStringToObject(request, "section", "Q"));
    expect_refused(composer_api_lock, request, COMPOSER_ERR_UNSATISFIABLE, "/section", "the plan has no section 'Q'");
    request = plan_request(plan_path);
    assert(cJSON_AddNumberToObject(request, "node", 9999));
    expect_refused(composer_api_lock, request, COMPOSER_ERR_UNSATISFIABLE, "/node", "the plan has no node 9999");
    request = plan_request(plan_path);
    assert(cJSON_AddNumberToObject(request, "node", 1) && cJSON_AddStringToObject(request, "section", "A"));
    expect_refused(composer_api_lock, request, COMPOSER_ERR_REQUEST, "", "give \"node\" (an id) or \"section\" (a letter), not both");
    request = plan_request(plan_path);
    assert(cJSON_AddStringToObject(request, "section", "A") && cJSON_AddStringToObject(request, "lane", "bogus"));
    expect_refused(composer_api_lock, request, COMPOSER_ERR_REQUEST, "/lane", "lane must be harmony, bass, melody or inner");
    request = cJSON_CreateObject();
    assert(request && cJSON_AddStringToObject(request, "section", "A"));
    expect_refused(composer_api_lock, request, COMPOSER_ERR_REQUEST, "/plan", "the request needs \"plan\"");

    /* A plan that is an analysis of the Analyzer is not a plan. */
    request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "plan", load_json(FIXTURE("analysis_small.json"))));
    expect_refused(composer_api_fill, request, COMPOSER_ERR_UNSATISFIABLE, "/plan", "plan_from_analyzer_result");
    request = plan_request(plan_path);
    assert(cJSON_AddStringToObject(request, "seed", "x"));
    expect_refused(composer_api_fill, request, COMPOSER_ERR_REQUEST, "/seed", "seed must be a whole number from 0 to 4294967295");
    request = plan_request(plan_path);
    assert(cJSON_AddNumberToObject(request, "candidates", 33));
    expect_refused(composer_api_fill, request, COMPOSER_ERR_REQUEST, "/candidates", "candidates must be a whole number from 1 to 32");
    request = plan_request(plan_path);
    assert(cJSON_AddStringToObject(request, "style_weight", "1/2"));
    expect_refused(composer_api_fill, request, COMPOSER_ERR_REQUEST, "/style_weight", "style_weight needs a style");
    request = plan_request(plan_path);
    assert(cJSON_AddStringToObject(request, "tempo", "5") && cJSON_AddTrueToObject(request, "midi"));
    expect_refused(composer_api_fill, request, COMPOSER_ERR_REQUEST, "/tempo", "tempo must be from 20 to 400");

    request = plan_request(plan_path);
    assert(cJSON_AddStringToObject(request, "section", "A") && cJSON_AddNumberToObject(request, "count", 0));
    expect_refused(composer_api_variations, request, COMPOSER_ERR_REQUEST, "/count", "count must be a whole number from 1 to 16");
    request = plan_request(plan_path);
    assert(cJSON_AddStringToObject(request, "section", "A"));
    expect_refused(composer_api_variations, request, COMPOSER_ERR_REQUEST, "/count", "the request needs \"count\"");
    request = plan_request(plan_path);
    assert(cJSON_AddStringToObject(request, "section", "A") && cJSON_AddStringToObject(request, "seed", "-1"));
    expect_refused(composer_api_regenerate, request, COMPOSER_ERR_REQUEST, "/seed", "seed must be a whole number");
}

/* Refusals that read a filled plan, which hearing leaks (notes.md), so not "quick". */
static void test_filled_refusals(void) {
    /* A locked section is never written again. */
    ComposerResponse locked;
    char *printed;
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "plan", cJSON_Parse(fill_plan_text)) &&
           cJSON_AddStringToObject(request, "section", "A"));
    assert(call(composer_api_lock, request, NULL, &locked, &printed) == COMPOSER_OK);
    free(printed);
    request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "plan", cJSON_Parse(locked.plan_json)) &&
           cJSON_AddStringToObject(request, "section", "A") && cJSON_AddNumberToObject(request, "seed", 1));
    expect_refused(composer_api_regenerate, request, COMPOSER_ERR_UNSATISFIABLE, "", "section A is Given (locked)");
    request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "plan", cJSON_Parse(locked.plan_json)) &&
           cJSON_AddStringToObject(request, "section", "A") && cJSON_AddNumberToObject(request, "count", 1));
    expect_refused(composer_api_variations, request, COMPOSER_ERR_UNSATISFIABLE, "", "section A is Given (locked)");
    composer_response_destroy(&locked);
}

/* --- cancel --------------------------------------------------------------------------------------- */

typedef struct {
    unsigned reports;
    bool saw_seed;
    bool cancel;
} Watch;

static void progress(void *user, const char *stage, unsigned done, unsigned total) {
    Watch *watch = user;
    (void)done;
    (void)total;
    watch->reports++;
    watch->saw_seed = watch->saw_seed || strcmp(stage, "seed") == 0;
}

static bool cancelled(void *user) {
    return ((Watch *)user)->cancel;
}

/* A job that says cancel at the first report: fill, regenerate and variations stop with COMPOSER_ERR_CANCELLED and no plan. */
static void test_cancel(void) {
    ComposerStatus (*const entries[])(const char *, const ComposerJob *, ComposerResponse *) = {
        composer_api_fill, composer_api_regenerate, composer_api_variations};
    const char *const names[] = {"fill", "regenerate", "variations"};
    for (int i = 0; i < 3; i++) {
        Watch watch = {0, false, true};
        ComposerJob job = {progress, cancelled, &watch};
        ComposerResponse response;
        char *printed;
        cJSON *request = plan_request(i == 0 ? SNAPSHOT("plan-binary.plan.json") : SNAPSHOT("fill.plan.json"));
        assert(cJSON_AddNumberToObject(request, "seed", 1));
        if (i > 0) {
            assert(cJSON_AddStringToObject(request, "section", "B") && cJSON_AddNumberToObject(request, "count", 2));
        }
        ComposerStatus status = call(entries[i], request, &job, &response, &printed);
        if (status != COMPOSER_ERR_CANCELLED) {
            fprintf(stderr, "%s: %s: %s\n", names[i], composer_status_name(status), response.error.message);
        }
        assert(status == COMPOSER_ERR_CANCELLED && !response.plan_json && !response.json && !response.midi);
        assert(watch.reports >= 1);
        assert(i != 0 || watch.saw_seed);
        composer_response_destroy(&response);
        free(printed);
    }
}

int main(int argc, char **argv) {
    quick = argc > 1 && strcmp(argv[1], "quick") == 0;
    test_plan();
    test_plan_refusals();
    test_refusals();
    test_cancel();
    if (!quick) {
        test_fill();
        test_lock();
        test_regenerate();
        test_variations();
        test_stuck();
        test_filled_refusals();
    }
    free(fill_plan_text);
    return 0;
}
