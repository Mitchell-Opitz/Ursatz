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
 * The piece entries (docs/api.md): every piece-* case of the CLI snapshots (tests/cli_snapshots.cmake) run from its request,
 * with the document, the plans, the MIDI file and the MusicXML equal to the snapshots; the ending version's MusicXML, the
 * explanation and the lanes of a regenerate; a cancel; the requests the entries refuse, with their paths. The requests and
 * responses are written beside the test, for the schema test.
 */

#define FIXTURE(name) COMPOSER_FIXTURE_DIR "/" name
#define SNAPSHOT(name) COMPOSER_SNAPSHOT_DIR "/" name
#define SOURCE_LABEL "<WORK>/source-theme.plan.json"

/* --- requests ------------------------------------------------------------------------------------- */

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

/* The plan of the one-motive theme at seed 1: what the snapshot cases give a theme fill. */
static char *theme_plan(void) {
    char *text = test_read_file(FIXTURE("theme/one-motive.json"), NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request && cJSON_AddNumberToObject(request, "seed", 1));
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    ComposerResponse response;
    assert(composer_api_theme(printed, NULL, &response) == COMPOSER_OK);
    free(printed);
    char *plan = strdup(response.plan_json);
    composer_response_destroy(&response);
    assert(plan);
    return plan;
}

/* A piece request fixture: the style placeholder filled, and the plan of a theme fill given inline with its label. */
static cJSON *piece_fixture(const char *name, const char *plan) {
    char path[256];
    snprintf(path, sizeof path, FIXTURE("piece/%s"), name);
    char *text = replace(test_read_file(path, NULL), "@STYLE@", FIXTURE("style-fast-minor.json"));
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request);
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(request, "sections")) {
        cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        const cJSON *kind = cJSON_GetObjectItemCaseSensitive(fill, "kind");
        if (cJSON_IsString(kind) && strcmp(kind->valuestring, "theme") == 0) {
            cJSON *inline_plan = cJSON_Parse(plan);
            assert(inline_plan && cJSON_ReplaceItemInObject(fill, "plan", inline_plan));
            assert(cJSON_AddStringToObject(fill, "source", SOURCE_LABEL));
        }
    }
    return request;
}

/* {"request": piece, "seed": seed, ...}: the piece request is taken. */
static cJSON *api_request(cJSON *piece, unsigned seed) {
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "request", piece) && cJSON_AddNumberToObject(request, "seed", seed));
    return request;
}

static void add_edit(cJSON *request, const char *plan, const char *section, const char *lane) {
    cJSON *from = cJSON_Parse(plan);
    cJSON *edit = cJSON_CreateObject();
    cJSON *lanes = cJSON_CreateArray();
    assert(from && edit && lanes && cJSON_AddItemToObject(request, "from", from));
    assert(section ? cJSON_AddStringToObject(edit, "section", section) != NULL : cJSON_AddNullToObject(edit, "section") != NULL);
    assert(!lane || cJSON_AddItemToArray(lanes, cJSON_CreateString(lane)));
    assert(cJSON_AddItemToObject(edit, "lanes", lanes) && cJSON_AddItemToObject(request, "regenerate", edit));
}

static char *print(cJSON *request) {
    char *printed = cJSON_PrintUnformatted(request);
    assert(printed);
    cJSON_Delete(request);
    return printed;
}

static void write_pair(const char *kind, const char *tag, const char *request, const ComposerResponse *response) {
    char path[160];
    snprintf(path, sizeof path, "api_%s_request_%s.json", kind, tag);
    test_write_file(path, request);
    snprintf(path, sizeof path, "api_%s_response_%s.json", kind, tag);
    test_write_file(path, response->json);
}

/* --- comparisons ---------------------------------------------------------------------------------- */

static cJSON *load_json(const char *path) {
    char *text = test_read_file(path, NULL);
    cJSON *json = cJSON_Parse(text);
    assert(json);
    free(text);
    return json;
}

/* A copy of the JSON with the fixtures directory written as the snapshots write it (cli_snapshots.cmake). */
static cJSON *normalized(const cJSON *json) {
    char *text = cJSON_PrintUnformatted(json);
    assert(text);
    const char *directory = COMPOSER_FIXTURE_DIR;
    size_t length = strlen(directory);
    size_t capacity = strlen(text) + 1;
    char *out = malloc(capacity);
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
static void expect_plan(const cJSON *actual, const char *snapshot) {
    cJSON *left = normalized(actual);
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

static void expect_same(const cJSON *actual, const cJSON *expected, const char *snapshot) {
    if (!cJSON_Compare(actual, expected, true)) {
        char *left = cJSON_PrintUnformatted(actual);
        char *right = cJSON_PrintUnformatted(expected);
        fprintf(stderr, "the document differs from %s:\n%s\n%s\n", snapshot, left, right);
        free(left);
        free(right);
    }
    assert(cJSON_Compare(actual, expected, true));
}

static void set_null(cJSON *object, const char *name) {
    assert(cJSON_ReplaceItemInObject(object, name, cJSON_CreateNull()));
}

/* The document of a piece equals the snapshot of `piece --format json`: the plans are in "plans", not files. */
static void expect_document(const char *json, const char *snapshot) {
    cJSON *parsed = cJSON_Parse(json);
    assert(parsed);
    cJSON *actual = normalized(parsed);
    cJSON_Delete(parsed);
    cJSON *expected = load_json(snapshot);
    cJSON_DeleteItemFromObjectCaseSensitive(expected, "files");
    cJSON *versions = cJSON_GetObjectItemCaseSensitive(expected, "versions");
    set_null(cJSON_GetObjectItemCaseSensitive(versions, "loop"), "plan");
    set_null(cJSON_GetObjectItemCaseSensitive(versions, "ending"), "plan");
    cJSON_DeleteItemFromObjectCaseSensitive(actual, "plans");
    expect_same(actual, expected, snapshot);
    cJSON_Delete(actual);
    cJSON_Delete(expected);
}

static void expect_file(const unsigned char *data, size_t size, const char *snapshot) {
    size_t expected_size = 0;
    char *expected = test_read_file(snapshot, &expected_size);
    assert(data && size == expected_size && memcmp(data, expected, size) == 0);
    free(expected);
}

/* The response of a composed or regenerated piece equals the snapshots of its case. */
static void expect_case(const char *name, const ComposerResponse *response, bool midi) {
    char path[256];
    snprintf(path, sizeof path, SNAPSHOT("%s.stdout.json"), name);
    expect_document(response->json, path);
    snprintf(path, sizeof path, SNAPSHOT("%s.plan.json"), name);
    cJSON *plan = cJSON_Parse(response->plan_json);
    assert(plan);
    expect_plan(plan, path);
    /* The plan of the loop version is the one in the document. */
    cJSON *document = cJSON_Parse(response->json);
    assert(cJSON_Compare(plan, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "plans"), "loop"), true));
    assert(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "plans"), "ending")));
    cJSON_Delete(document);
    cJSON_Delete(plan);
    if (midi) {
        snprintf(path, sizeof path, SNAPSHOT("%s.mid"), name);
        expect_file(response->midi, response->midi_size, path);
    } else {
        assert(!response->midi);
    }
}

/* --- the snapshot cases --------------------------------------------------------------------------- */

static char *loop_plan_of_theme_fill; /* the loop plan of piece-theme-fill, the plan the regenerate cases start from */

static void run_case(const char *name, const char *fixture, const char *plan, bool keep_plan) {
    cJSON *request = api_request(piece_fixture(fixture, plan), 1);
    assert(cJSON_AddBoolToObject(request, "midi", true));
    char *printed = print(request);
    ComposerResponse response;
    ComposerStatus status = composer_api_piece(printed, NULL, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "%s: %s: %s\n", name, composer_status_name(status), response.error.message);
    }
    assert(status == COMPOSER_OK);
    expect_case(name, &response, true);
    if (keep_plan) {
        loop_plan_of_theme_fill = strdup(response.plan_json);
        assert(loop_plan_of_theme_fill);
    }
    write_pair("piece", name, printed, &response);
    composer_response_destroy(&response);
    free(printed);
}

static void run_regenerate(const char *name, const char *plan, const char *section, const char *lane, const char *expected_lane) {
    cJSON *request = api_request(piece_fixture("theme-fill.json", plan), 2);
    add_edit(request, loop_plan_of_theme_fill, section, lane);
    char *printed = print(request);
    ComposerResponse response;
    ComposerStatus status = composer_api_piece(printed, NULL, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "%s: %s: %s\n", name, composer_status_name(status), response.error.message);
    }
    assert(status == COMPOSER_OK);
    expect_case(name, &response, false);
    cJSON *document = cJSON_Parse(response.json);
    const cJSON *regenerated = cJSON_GetObjectItemCaseSensitive(document, "regenerated");
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(regenerated, "section")->valuestring, section) == 0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(regenerated, "lane")->valuestring, expected_lane) == 0);
    cJSON_Delete(document);
    write_pair("piece", name, printed, &response);
    composer_response_destroy(&response);
    free(printed);
}

static void test_snapshots(char *plan) {
    run_case("piece-theme-fill", "theme-fill.json", plan, true);
    run_case("piece-loop-and-ending", "loop-and-ending.json", plan, false);
    run_case("piece-progression-fill", "progression-fill.json", plan, false);
    run_regenerate("piece-regenerate", plan, "intro", NULL, "all");
    run_regenerate("piece-regenerate-lane", plan, "a", "bass", "bass");
}

/* piece-variations: the plans, the MusicXML, and the MIDI files of composer_api_export with the loop markers. */
static void test_variations(char *plan) {
    cJSON *request = api_request(piece_fixture("theme-fill.json", plan), 2);
    cJSON *from = cJSON_Parse(loop_plan_of_theme_fill);
    cJSON *variations = cJSON_CreateObject();
    cJSON *lanes = cJSON_CreateArray();
    assert(from && variations && lanes && cJSON_AddItemToObject(request, "from", from));
    assert(cJSON_AddItemToArray(lanes, cJSON_CreateString("harmony")) && cJSON_AddItemToObject(variations, "lanes", lanes));
    assert(cJSON_AddStringToObject(variations, "section", "a") && cJSON_AddNumberToObject(variations, "count", 2));
    assert(cJSON_AddItemToObject(request, "variations", variations) && cJSON_AddBoolToObject(request, "musicxml", true));
    char *printed = print(request);
    ComposerResponse response;
    ComposerStatus status = composer_api_piece_variations(printed, NULL, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "piece-variations: %s: %s\n", composer_status_name(status), response.error.message);
    }
    assert(status == COMPOSER_OK && response.json && !response.midi && !response.musicxml);
    cJSON *actual = cJSON_Parse(response.json);
    cJSON *expected = load_json(SNAPSHOT("piece-variations.result.json"));
    assert(actual);
    cJSON_DeleteItemFromObjectCaseSensitive(actual, "brief");
    int index = 0;
    const cJSON *item = NULL;
    cJSON *expected_item = cJSON_GetObjectItemCaseSensitive(expected, "variations")->child;
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(actual, "variations")) {
        char path[256];
        cJSON *loop = cJSON_GetObjectItemCaseSensitive(item, "loop");
        index++;
        snprintf(path, sizeof path, SNAPSHOT("piece-variations.variation-%d.json"), index);
        expect_plan(cJSON_GetObjectItemCaseSensitive(loop, "plan"), path);
        snprintf(path, sizeof path, SNAPSHOT("piece-variations.variation-%d.musicxml"), index);
        char *musicxml = test_read_file(path, NULL);
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(item, "musicxml")->valuestring, musicxml) == 0);
        free(musicxml);
        /* The MIDI file of a variation: the export entry with the loop markers of the entry. */
        cJSON *export_request = cJSON_CreateObject();
        assert(export_request && cJSON_AddItemToObject(export_request, "plan", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(loop, "plan"), true)));
        assert(cJSON_AddBoolToObject(export_request, "midi", true));
        assert(cJSON_AddItemToObject(export_request, "loop",
                                     cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(item, "loop_markers"), true)));
        char *export_text = print(export_request);
        ComposerResponse exported;
        assert(composer_api_export(export_text, NULL, &exported) == COMPOSER_OK);
        snprintf(path, sizeof path, SNAPSHOT("piece-variations.variation-%d.mid"), index);
        expect_file(exported.midi, exported.midi_size, path);
        composer_response_destroy(&exported);
        free(export_text);
        /* What is left is the snapshot's result.json without its file names. */
        set_null(loop, "plan");
        cJSON_DeleteItemFromObjectCaseSensitive((cJSON *)item, "musicxml");
        cJSON_DeleteItemFromObjectCaseSensitive((cJSON *)item, "loop_markers");
        cJSON_DeleteItemFromObjectCaseSensitive(expected_item, "midi");
        cJSON_DeleteItemFromObjectCaseSensitive(expected_item, "musicxml");
        set_null(cJSON_GetObjectItemCaseSensitive(expected_item, "loop"), "plan");
        expected_item = expected_item->next;
    }
    assert(index == 2);
    expect_same(actual, expected, "piece-variations.result.json");
    cJSON_Delete(actual);
    cJSON_Delete(expected);
    write_pair("piece_variations", "snapshot", printed, &response);
    composer_response_destroy(&response);
    free(printed);
}

/* --- outputs the CLI writes beside the plan ------------------------------------------------------ */

static void test_outputs(char *plan) {
    /* The ending version's MusicXML and the explanation of both versions. */
    cJSON *request = api_request(piece_fixture("progression-fill.json", plan), 1);
    assert(cJSON_AddBoolToObject(request, "ending_musicxml", true) && cJSON_AddBoolToObject(request, "musicxml", true) &&
           cJSON_AddStringToObject(request, "explain", "text"));
    char *printed = print(request);
    ComposerResponse response;
    assert(composer_api_piece(printed, NULL, &response) == COMPOSER_OK);
    free(printed);
    cJSON *document = cJSON_Parse(response.json);
    assert(document);
    const cJSON *ending = cJSON_GetObjectItemCaseSensitive(document, "ending_musicxml");
    assert(cJSON_IsString(ending) && strstr(ending->valuestring, "<score-partwise"));
    assert(response.musicxml && strcmp(response.musicxml, ending->valuestring) != 0);
    const cJSON *explanation = cJSON_GetObjectItemCaseSensitive(document, "explanation");
    assert(cJSON_IsString(explanation) && strstr(explanation->valuestring, "== loop version ==") &&
           strstr(explanation->valuestring, "== ending version =="));
    cJSON_Delete(document);
    composer_response_destroy(&response);

    /* A regenerate of two lanes: the lanes in the order asked, a section left alone is none. */
    request = api_request(piece_fixture("progression-fill.json", plan), 2);
    cJSON *first = api_request(piece_fixture("progression-fill.json", plan), 1);
    printed = print(first);
    assert(composer_api_piece(printed, NULL, &response) == COMPOSER_OK);
    free(printed);
    char *loop = strdup(response.plan_json);
    composer_response_destroy(&response);
    add_edit(request, loop, "intro", "bass");
    cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(request, "regenerate"), "lanes"),
                         cJSON_CreateString("harmony"));
    printed = print(request);
    assert(composer_api_piece(printed, NULL, &response) == COMPOSER_OK);
    free(printed);
    document = cJSON_Parse(response.json);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "regenerated"), "lane")->valuestring,
                  "bass+harmony") == 0);
    cJSON_Delete(document);
    composer_response_destroy(&response);

    request = api_request(piece_fixture("progression-fill.json", plan), 2);
    add_edit(request, loop, NULL, NULL);
    printed = print(request);
    assert(composer_api_piece(printed, NULL, &response) == COMPOSER_OK);
    free(printed);
    document = cJSON_Parse(response.json);
    assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(document, "regenerated")));
    /* The loop plan is kept as it was: the ending version is built again. */
    cJSON *kept = cJSON_Parse(response.plan_json);
    cJSON *original = cJSON_Parse(loop);
    assert(kept && original);
    expect_same(kept, original, "the plan the regenerate kept");
    cJSON_Delete(kept);
    cJSON_Delete(original);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(loop);
}

/* A regenerate names what it wrote again: no section reports itself kept unless it was left alone. */
static void test_kept_sections(char *plan) {
    cJSON *first = api_request(piece_fixture("loop-and-ending.json", plan), 1);
    char *printed = print(first);
    ComposerResponse response;
    assert(composer_api_piece(printed, NULL, &response) == COMPOSER_OK);
    free(printed);
    char *loop = strdup(response.plan_json);
    composer_response_destroy(&response);
    cJSON *request = api_request(piece_fixture("loop-and-ending.json", plan), 2);
    add_edit(request, loop, "b", "harmony");
    printed = print(request);
    assert(composer_api_piece(printed, NULL, &response) == COMPOSER_OK);
    free(printed);
    cJSON *document = cJSON_Parse(response.json);
    assert(document);
    const cJSON *versions = cJSON_GetObjectItemCaseSensitive(document, "versions");
    for (int v = 0; v < 2; v++) {
        const cJSON *section = NULL;
        cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(versions, v ? "ending" : "loop"),
                                                                      "sections")) {
            const cJSON *in_scope = cJSON_GetObjectItemCaseSensitive(section, "in_scope");
            /* A section out of scope says so with false; the one regenerated never does. */
            assert(!in_scope || cJSON_IsFalse(in_scope));
            assert(!in_scope || strcmp(cJSON_GetObjectItemCaseSensitive(section, "id")->valuestring, "b") != 0);
        }
    }
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(loop);
}

/* --- cancel ----------------------------------------------------------------------------------------- */

typedef struct {
    unsigned steps;
    unsigned cancel_at; /* the step from which the job says cancelled; 0 never */
    bool seed_reported;
    bool stage_reported;
} Watch;

static void on_progress(void *user, const char *stage, unsigned done, unsigned total) {
    Watch *watch = user;
    (void)done;
    (void)total;
    watch->seed_reported = watch->seed_reported || strcmp(stage, "seed") == 0 || strcmp(stage, "variations") == 0;
    watch->stage_reported = watch->stage_reported || (strcmp(stage, "seed") != 0 && strcmp(stage, "variations") != 0);
}

static bool on_cancelled(void *user) {
    Watch *watch = user;
    watch->steps++;
    return watch->cancel_at && watch->steps >= watch->cancel_at;
}

static void expect_nothing(const ComposerResponse *response) {
    assert(!response->json && !response->plan_json && !response->midi && !response->musicxml);
}

/* A cancel at the first step, and at a later one: no document, no plan, nothing left allocated (the valgrind run). */
static void test_cancel(char *plan, bool quick) {
    for (unsigned cancel_at = 1; cancel_at <= (quick ? 1 : 4); cancel_at += 3) {
        cJSON *request = api_request(piece_fixture("theme-fill.json", plan), 1);
        char *printed = print(request);
        Watch watch;
        memset(&watch, 0, sizeof(watch));
        watch.cancel_at = cancel_at;
        ComposerJob job = {on_progress, on_cancelled, &watch};
        ComposerResponse response;
        assert(composer_api_piece(printed, &job, &response) == COMPOSER_ERR_CANCELLED);
        assert(watch.steps >= cancel_at && watch.seed_reported);
        assert(response.error.message[0]);
        expect_nothing(&response);
        composer_response_destroy(&response);
        free(printed);
    }
}

/* A cancel while a section is written again, and while variations are made. */
static void test_cancel_edit(char *plan) {
    cJSON *request = api_request(piece_fixture("theme-fill.json", plan), 2);
    add_edit(request, loop_plan_of_theme_fill, "a", "bass");
    char *printed = print(request);
    Watch watch;
    memset(&watch, 0, sizeof(watch));
    watch.cancel_at = 2;
    ComposerJob job = {on_progress, on_cancelled, &watch};
    ComposerResponse response;
    assert(composer_api_piece(printed, &job, &response) == COMPOSER_ERR_CANCELLED);
    expect_nothing(&response);
    composer_response_destroy(&response);
    free(printed);

    request = api_request(piece_fixture("theme-fill.json", plan), 2);
    cJSON *from = cJSON_Parse(loop_plan_of_theme_fill);
    cJSON *variations = cJSON_CreateObject();
    assert(from && variations && cJSON_AddItemToObject(request, "from", from));
    assert(cJSON_AddStringToObject(variations, "section", "a") && cJSON_AddNumberToObject(variations, "count", 2));
    assert(cJSON_AddItemToObject(request, "variations", variations));
    printed = print(request);
    memset(&watch, 0, sizeof(watch));
    watch.cancel_at = 1;
    assert(composer_api_piece_variations(printed, &job, &response) == COMPOSER_ERR_CANCELLED);
    assert(watch.seed_reported);
    expect_nothing(&response);
    composer_response_destroy(&response);
    free(printed);
}

/* --- refused requests ------------------------------------------------------------------------------- */

static ComposerStatus run_text(const char *text, ComposerResponse *response, bool variations) {
    return variations ? composer_api_piece_variations(text, NULL, response) : composer_api_piece(text, NULL, response);
}

static void expect_refusal(const char *text, bool variations, ComposerStatus status, const char *path, const char *message) {
    ComposerResponse response;
    ComposerStatus got = run_text(text, &response, variations);
    if (got != status || strcmp(response.error.path, path) != 0 || !strstr(response.error.message, message)) {
        fprintf(stderr, "%s '%s' at '%s', expected %s '%s' at '%s'\n", composer_status_name(got), response.error.message,
                response.error.path, composer_status_name(status), message, path);
    }
    assert(got == status);
    assert(strcmp(response.error.path, path) == 0);
    assert(strstr(response.error.message, message));
    expect_nothing(&response);
    composer_response_destroy(&response);
}

/* The theme-fill piece request with the JSON text value at the pointer-like route (section index, member) replaced. */
static char *with_section_member(const char *plan, size_t section, const char *member, const char *value) {
    cJSON *piece = piece_fixture("theme-fill.json", plan);
    cJSON *item = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(piece, "sections"), (int)section);
    cJSON_DeleteItemFromObjectCaseSensitive(item, member);
    cJSON *parsed = cJSON_Parse(value);
    assert(item && parsed && cJSON_AddItemToObject(item, member, parsed));
    return print(api_request(piece, 1));
}

static char *with_piece_member(const char *plan, const char *member, const char *value) {
    cJSON *piece = piece_fixture("theme-fill.json", plan);
    cJSON_DeleteItemFromObjectCaseSensitive(piece, member);
    cJSON *parsed = value ? cJSON_Parse(value) : NULL;
    assert(!value || (parsed && cJSON_AddItemToObject(piece, member, parsed)));
    return print(api_request(piece, 1));
}

static void test_refused(char *plan) {
    char *text;
    /* A theme fill that names a file in place of its plan. */
    cJSON *piece = piece_fixture("theme-fill.json", plan);
    cJSON *fill = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(piece, "sections"), 1), "fill");
    assert(cJSON_ReplaceItemInObject(fill, "plan", cJSON_CreateString("theme.plan.json")));
    text = print(api_request(piece, 1));
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/request/sections/1/fill/plan", "not a path");
    free(text);
    piece = piece_fixture("theme-fill.json", plan);
    fill = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(piece, "sections"), 1), "fill");
    assert(cJSON_ReplaceItemInObject(fill, "plan", cJSON_Parse("{\"format\": \"nothing\"}")));
    text = print(api_request(piece, 1));
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/request/sections/1/fill/plan", "cannot read the theme plan");
    free(text);

    text = with_section_member(plan, 0, "beats", "\"8\"");
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/request/sections/0/beats", "whole number of beats");
    free(text);
    text = with_section_member(plan, 0, "cadence", "\"XX\"");
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/request/sections/0/cadence", "the cadence must be");
    free(text);
    text = with_section_member(plan, 2, "part", "\"coda\"");
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/request/sections/2/part", "the part must be");
    free(text);
    text = with_section_member(plan, 0, "pickup", "\"half\"");
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/request/sections/0/pickup", "not a number of beats");
    free(text);
    text = with_piece_member(plan, "meter", "\"5/4\"");
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/request/meter", "is not supported");
    free(text);
    text = with_piece_member(plan, "key", "\"H\"");
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/request/key", "unknown key 'H'");
    free(text);
    text = with_piece_member(plan, "schema", "\"ursatz-theme-request\"");
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/request", "ursatz-piece-request");
    free(text);
    text = with_piece_member(plan, "style", "\"no-such-style.json\"");
    expect_refusal(text, false, COMPOSER_ERR_UNSATISFIABLE, "/request/style", "cannot read style");
    free(text);
    text = with_piece_member(plan, "tempo", "500");
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/request/tempo", "tempo must be from 20 to 400");
    free(text);
    text = with_piece_member(plan, "sections", "[]");
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/request/sections", "a list with a body");
    free(text);
    text = with_piece_member(plan, "voicing", "\"trio\"");
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/request/voicing", "satb or chorale");
    free(text);

    /* The members around the piece request. */
    cJSON *request = api_request(piece_fixture("theme-fill.json", plan), 1);
    assert(cJSON_AddStringToObject(request, "corpus", "corpus-dir"));
    text = print(request);
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/corpus", "takes no corpus");
    free(text);
    request = api_request(piece_fixture("theme-fill.json", plan), 1);
    assert(cJSON_AddStringToObject(request, "explain", "xml"));
    text = print(request);
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/explain", "explain must be");
    free(text);
    request = api_request(piece_fixture("theme-fill.json", plan), 1);
    cJSON_ReplaceItemInObject(request, "seed", cJSON_CreateString("many"));
    text = print(request);
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/seed", "seed must be a whole number");
    free(text);
    request = api_request(piece_fixture("theme-fill.json", plan), 1);
    cJSON_DeleteItemFromObjectCaseSensitive(request, "request");
    text = print(request);
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/request", "needs \"request\"");
    free(text);
    expect_refusal("{\"api_version\": 7}", false, COMPOSER_ERR_REQUEST, "/api_version", "api_version");
    expect_refusal("not json", false, COMPOSER_ERR_REQUEST, "", "not valid JSON");

    /* The edits: a section the plan lacks, a lane that is none, a theme section without a lane, a regenerate without a plan. */
    request = api_request(piece_fixture("theme-fill.json", plan), 1);
    add_edit(request, loop_plan_of_theme_fill, "intro", "tenor");
    text = print(request);
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/regenerate/lanes/0", "lane must be");
    free(text);
    request = api_request(piece_fixture("theme-fill.json", plan), 1);
    assert(cJSON_AddItemToObject(request, "regenerate", cJSON_Parse("{\"section\": \"intro\"}")));
    text = print(request);
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/regenerate", "goes with \"from\"");
    free(text);
    request = api_request(piece_fixture("theme-fill.json", plan), 1);
    assert(cJSON_AddStringToObject(request, "from", "loop.json"));
    text = print(request);
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/from", "plan object");
    free(text);

    /* The variations: no plan, no count, a count out of range. */
    request = api_request(piece_fixture("theme-fill.json", plan), 1);
    assert(cJSON_AddItemToObject(request, "variations", cJSON_Parse("{\"section\": \"intro\", \"count\": 2}")));
    text = print(request);
    expect_refusal(text, true, COMPOSER_ERR_REQUEST, "/from", "needs \"from\"");
    free(text);
    request = api_request(piece_fixture("theme-fill.json", plan), 1);
    assert(cJSON_AddItemToObject(request, "variations", cJSON_Parse("{\"section\": \"intro\", \"count\": 9}")));
    text = print(request);
    expect_refusal(text, true, COMPOSER_ERR_REQUEST, "/variations/count", "count must be a whole number from 1 to 8");
    free(text);
    request = api_request(piece_fixture("theme-fill.json", plan), 1);
    assert(cJSON_AddItemToObject(request, "variations", cJSON_Parse("{\"section\": \"intro\"}")));
    text = print(request);
    expect_refusal(text, true, COMPOSER_ERR_REQUEST, "/variations/count", "needs \"count\"");
    free(text);

    /* A loop of the export entry that is not a time. */
    ComposerResponse response;
    char *export_request = strdup("{\"plan\": {}, \"midi\": true, \"loop\": {\"start\": \"two\", \"end\": \"3/1\"}}");
    assert(composer_api_export(export_request, NULL, &response) == COMPOSER_ERR_REQUEST);
    assert(strcmp(response.error.path, "/loop/start") == 0);
    composer_response_destroy(&response);
    free(export_request);
}

/* The refusals that read a loop plan: it takes the plan of a composed piece, which hearing leaks (notes.md), so not "quick". */
static void test_refused_edits(char *plan) {
    char *text;
    cJSON *request;
    request = api_request(piece_fixture("theme-fill.json", plan), 1);
    add_edit(request, loop_plan_of_theme_fill, "nowhere", NULL);
    text = print(request);
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/regenerate/section", "has no section 'nowhere'");
    free(text);
    request = api_request(piece_fixture("theme-fill.json", plan), 1);
    add_edit(request, loop_plan_of_theme_fill, "a", NULL);
    text = print(request);
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/regenerate/lanes", "filled with a theme");
    free(text);
    request = api_request(piece_fixture("theme-fill.json", plan), 1);
    add_edit(request, loop_plan_of_theme_fill, "a", "melody");
    text = print(request);
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/regenerate/lanes", "filled with a theme");
    free(text);
    request = api_request(piece_fixture("theme-fill.json", plan), 1);
    add_edit(request, loop_plan_of_theme_fill, NULL, NULL);
    cJSON_DeleteItemFromObjectCaseSensitive(request, "regenerate");
    assert(cJSON_AddItemToObject(request, "variations", cJSON_Parse("{\"section\": \"nowhere\", \"count\": 2}")));
    text = print(request);
    expect_refusal(text, true, COMPOSER_ERR_REQUEST, "/variations/section", "has no section 'nowhere'");
    free(text);
}

/* The melody entries of a loop plan that lie inside [start, end) whole notes, as an array of {span, notes}. */
static cJSON *melody_between(const char *plan_json, long start, long end) {
    cJSON *plan = cJSON_Parse(plan_json);
    cJSON *found = cJSON_CreateArray();
    assert(plan && found);
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(plan, "melody")) {
        const cJSON *span = cJSON_GetObjectItemCaseSensitive(entry, "span");
        const cJSON *from = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(span, "start"), "num");
        const cJSON *to = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(span, "end"), "num");
        const cJSON *den = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(span, "end"), "den");
        if (from->valuedouble >= (double)start && to->valuedouble / den->valuedouble <= (double)end) {
            cJSON *copy = cJSON_CreateObject();
            assert(copy && cJSON_AddItemToObject(copy, "span", cJSON_Duplicate(span, 1)) &&
                   cJSON_AddItemToObject(copy, "notes", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(
                       cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(entry, "composer"), "payload"), "notes"), 1)));
            assert(cJSON_AddItemToArray(found, copy));
        }
    }
    cJSON_Delete(plan);
    return found;
}

/*
 * The harmony of a theme section is regenerated (the GUI's regenerate of a placed theme): a motive that carries its own
 * chords keeps them in the theme, but the placed theme's melody holds no motive refs, so the target is the phrase model
 * around a Given melody that stands on the tonic at every downbeat of the continuation. It is composed again, and the
 * section's melody is the one composed before.
 */
static void test_regenerate_theme_harmony(void) {
    static const unsigned theme_seeds[2] = {1, 6};
    for (size_t t = 0; t < 2; t++) {
        char *text = test_read_file(FIXTURE("theme/c-minor-arpeggio.json"), NULL);
        cJSON *theme_request = cJSON_Parse(text);
        free(text);
        assert(theme_request && cJSON_AddNumberToObject(theme_request, "seed", theme_seeds[t]));
        char *printed = cJSON_PrintUnformatted(theme_request);
        cJSON_Delete(theme_request);
        ComposerResponse theme;
        assert(composer_api_theme(printed, NULL, &theme) == COMPOSER_OK);
        free(printed);
        char *plan = strdup(theme.plan_json);
        composer_response_destroy(&theme);
        assert(plan);

        printed = print(api_request(piece_fixture("theme-fill.json", plan), 1));
        ComposerResponse composed;
        ComposerStatus status = composer_api_piece(printed, NULL, &composed);
        if (status != COMPOSER_OK) {
            fprintf(stderr, "theme seed %u: %s: %s\n", theme_seeds[t], composer_status_name(status), composed.error.message);
        }
        assert(status == COMPOSER_OK);
        free(printed);
        char *loop = strdup(composed.plan_json);
        composer_response_destroy(&composed);
        assert(loop);
        /* Section a runs from bar 2 (the intro's 8 beats) for 8 bars. */
        cJSON *melody = melody_between(loop, 2, 10);
        assert(cJSON_GetArraySize(melody) > 0);

        for (unsigned seed = 1; seed <= 4; seed++) {
            cJSON *request = api_request(piece_fixture("theme-fill.json", plan), seed);
            add_edit(request, loop, "a", "harmony");
            printed = print(request);
            ComposerResponse again;
            status = composer_api_piece(printed, NULL, &again);
            if (status != COMPOSER_OK) {
                fprintf(stderr, "theme seed %u, regenerate seed %u: %s: %s\n", theme_seeds[t], seed, composer_status_name(status),
                        again.error.message);
            }
            assert(status == COMPOSER_OK);
            free(printed);
            cJSON *document = cJSON_Parse(again.json);
            const cJSON *regenerated = cJSON_GetObjectItemCaseSensitive(document, "regenerated");
            assert(strcmp(cJSON_GetObjectItemCaseSensitive(regenerated, "section")->valuestring, "a") == 0);
            assert(strcmp(cJSON_GetObjectItemCaseSensitive(regenerated, "lane")->valuestring, "harmony") == 0);
            cJSON *now = melody_between(again.plan_json, 2, 10);
            assert(cJSON_Compare(melody, now, 1));
            cJSON_Delete(now);
            cJSON_Delete(document);
            composer_response_destroy(&again);
        }
        cJSON_Delete(melody);
        free(loop);
        free(plan);
    }
}

/* --- a piece of any form (schema version 2, B2 of P45) ------------------------------------------------- */

/* The piece request fixture with the members replaced: {name, JSON text}, a NULL text removing the member. */
static cJSON *form_piece(const char *fixture, const char *plan, const char *const edits[][2], size_t count) {
    cJSON *piece = piece_fixture(fixture, plan);
    for (size_t i = 0; i < count; i++) {
        cJSON_DeleteItemFromObjectCaseSensitive(piece, edits[i][0]);
        cJSON *item = edits[i][1] ? cJSON_Parse(edits[i][1]) : NULL;
        assert(!edits[i][1] || (item && cJSON_AddItemToObject(piece, edits[i][0], item)));
    }
    return piece;
}

/* The API request of a piece of any form at seed 1, with the members of the piece request replaced. */
static char *form_request(const char *fixture, const char *plan, const char *const edits[][2], size_t count, bool midi) {
    cJSON *request = api_request(form_piece(fixture, plan, edits, count), 1);
    assert(!midi || cJSON_AddBoolToObject(request, "midi", true));
    return print(request);
}

static cJSON *composed_document(const char *printed, ComposerResponse *response) {
    ComposerStatus status = composer_api_piece(printed, NULL, response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "any form: %s: %s\n", composer_status_name(status), response->error.message);
    }
    assert(status == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response->json);
    assert(document);
    return document;
}

/* The document holds the one version "piece", with the sections of ids and roles. */
static void expect_one_version(const cJSON *document, const char *const *ids, const char *const *roles, size_t count) {
    const cJSON *versions = cJSON_GetObjectItemCaseSensitive(document, "versions");
    const cJSON *plans = cJSON_GetObjectItemCaseSensitive(document, "plans");
    assert(cJSON_GetArraySize(versions) == 1 && cJSON_GetObjectItemCaseSensitive(versions, "piece"));
    assert(cJSON_GetArraySize(plans) == 1 && cJSON_GetObjectItemCaseSensitive(plans, "piece"));
    assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(document, "loop")));
    const cJSON *sections = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(versions, "piece"), "sections");
    assert(cJSON_GetArraySize(sections) == (int)count);
    for (size_t i = 0; i < count; i++) {
        const cJSON *row = cJSON_GetArrayItem(sections, (int)i);
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(row, "id")->valuestring, ids[i]) == 0);
        if (roles) {
            assert(strcmp(cJSON_GetObjectItemCaseSensitive(row, "role")->valuestring, roles[i]) == 0);
        }
        assert(cJSON_GetObjectItemCaseSensitive(row, "beats")->valuedouble > 0);
    }
}

static void test_composed_forms(char **rounded_plan) {
    static const char *const RB_IDS[] = {"A", "B", "A'"};
    static const char *const RB_ROLES[] = {"theme", "contrast", "return"};
    static const char *const VAR_IDS[] = {"theme", "variation 1", "variation 2", "variation 3"};
    ComposerResponse response;
    ComposerResponse again;
    /* Rounded binary: one version, its three sections, a MIDI file, and the same plan from the same request. */
    char *printed = form_request("rounded-binary.json", NULL, NULL, 0, true);
    cJSON *document = composed_document(printed, &response);
    expect_one_version(document, RB_IDS, RB_ROLES, 3);
    assert(response.midi && response.midi_size > 4 && memcmp(response.midi, "MThd", 4) == 0);
    cJSON_Delete(composed_document(printed, &again));
    assert(response.plan_json && again.plan_json && strcmp(response.plan_json, again.plan_json) == 0);
    write_pair("piece", "rounded-binary", printed, &response);
    *rounded_plan = strdup(response.plan_json);
    assert(*rounded_plan);
    cJSON_Delete(document);
    composer_response_destroy(&again);
    composer_response_destroy(&response);
    free(printed);
    /* Variations: the theme and its three variations. */
    printed = form_request("variations.json", NULL, NULL, 0, false);
    document = composed_document(printed, &response);
    expect_one_version(document, VAR_IDS, NULL, 4);
    write_pair("piece", "variations", printed, &response);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(printed);
    /* A modulating rounded binary of 24 bars in A minor, 3/4 (R-429: 10 of 10 seeds compose). */
    static const char *const EDITS[][2] = {{"key", "\"A\""}, {"mode", "\"minor\""}, {"meter", "\"3/4\""}, {"bars", "24"}, {"modulate", "true"}};
    printed = form_request("rounded-binary.json", NULL, EDITS, 5, false);
    document = composed_document(printed, &response);
    expect_one_version(document, RB_IDS, RB_ROLES, 3);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(printed);
}

/* Regenerating a section of a rounded binary keeps the others; the variations of a section hold the one version. */
static void test_edited_forms(const char *rounded_plan) {
    ComposerResponse response;
    cJSON *request = api_request(piece_fixture("rounded-binary.json", NULL), 1);
    add_edit(request, rounded_plan, "B", "melody");
    char *printed = print(request);
    cJSON *document = composed_document(printed, &response);
    const cJSON *sections =
        cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "versions"), "piece"),
                                         "sections");
    assert(cJSON_GetArraySize(sections) == 3);
    for (int i = 0; i < 3; i++) {
        const cJSON *row = cJSON_GetArrayItem(sections, i);
        const cJSON *in_scope = cJSON_GetObjectItemCaseSensitive(row, "in_scope");
        /* Only B was written again: the sections A and A' were kept. */
        assert(i == 1 ? in_scope == NULL : (in_scope && cJSON_IsFalse(in_scope)));
    }
    write_pair("piece", "rounded-binary-regenerate", printed, &response);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(printed);

    request = api_request(piece_fixture("rounded-binary.json", NULL), 1);
    cJSON *variations = cJSON_CreateObject();
    cJSON *lanes = cJSON_CreateArray();
    assert(variations && lanes && cJSON_AddItemToObject(request, "from", cJSON_Parse(rounded_plan)));
    assert(cJSON_AddItemToArray(lanes, cJSON_CreateString("melody")) && cJSON_AddItemToObject(variations, "lanes", lanes));
    assert(cJSON_AddStringToObject(variations, "section", "B") && cJSON_AddNumberToObject(variations, "count", 2));
    assert(cJSON_AddItemToObject(request, "variations", variations));
    printed = print(request);
    ComposerStatus status = composer_api_piece_variations(printed, NULL, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "variations of a rounded binary: %s: %s\n", composer_status_name(status), response.error.message);
    }
    assert(status == COMPOSER_OK && response.json);
    document = cJSON_Parse(response.json);
    assert(document);
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(document, "variations");
    assert(cJSON_GetArraySize(list) == 2);
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, list) {
        assert(cJSON_GetObjectItemCaseSensitive(entry, "piece") && !cJSON_GetObjectItemCaseSensitive(entry, "loop"));
        assert(!cJSON_GetObjectItemCaseSensitive(entry, "loop_markers"));
        assert(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(entry, "piece"), "plan")));
    }
    write_pair("piece_variations", "rounded-binary", printed, &response);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(printed);
}

/* The game loop as a version 2 request with "form": "game-loop" answers as the version 1 request does. */
static void test_game_loop_version_two(const char *plan) {
    ComposerResponse first;
    ComposerResponse second;
    cJSON *request = api_request(piece_fixture("loop-and-ending.json", plan), 1);
    assert(cJSON_AddBoolToObject(request, "midi", true) && cJSON_AddBoolToObject(request, "musicxml", true));
    char *printed = print(request);
    cJSON_Delete(composed_document(printed, &first));
    free(printed);
    static const char *const EDITS[][2] = {{"schema_version", "2"}, {"form", "\"game-loop\""}};
    request = api_request(form_piece("loop-and-ending.json", plan, EDITS, 2), 1);
    assert(cJSON_AddBoolToObject(request, "midi", true) && cJSON_AddBoolToObject(request, "musicxml", true));
    printed = print(request);
    cJSON_Delete(composed_document(printed, &second));
    free(printed);
    assert(strcmp(first.json, second.json) == 0);
    assert(strcmp(first.plan_json, second.plan_json) == 0);
    assert(first.midi_size == second.midi_size && memcmp(first.midi, second.midi, first.midi_size) == 0);
    assert(strcmp(first.musicxml, second.musicxml) == 0);
    composer_response_destroy(&first);
    composer_response_destroy(&second);
}

static void expect_form_refusal(const char *fixture, const char *plan, const char *const edits[][2], size_t count, const char *path,
                                const char *message) {
    char *text = form_request(fixture, plan, edits, count, false);
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, path, message);
    free(text);
}

/* What a piece request of any form refuses, with the paths; none of it composes anything. */
static void test_refused_forms(const char *plan) {
    static const char *const NO_FORM[][2] = {{"form", NULL}};
    static const char *const UNKNOWN[][2] = {{"form", "\"sonata\""}};
    static const char *const VERSION_ONE[][2] = {{"schema_version", "1"}};
    static const char *const SECTIONS[][2] = {{"sections", "[]"}};
    static const char *const VOICING[][2] = {{"voicing", "\"satb\""}};
    static const char *const BARS[][2] = {{"bars", "20"}};
    static const char *const COUNT[][2] = {{"count", "9"}};
    static const char *const GAME_LOOP_BARS[][2] = {{"schema_version", "2"}, {"form", "\"game-loop\""}, {"bars", "4"}};
    static const char *const GAME_LOOP_MODULATE[][2] = {{"schema_version", "2"}, {"form", "\"game-loop\""}, {"modulate", "true"}};
    expect_form_refusal("rounded-binary.json", NULL, NO_FORM, 1, "/request/form", "the request needs \"form\"");
    expect_form_refusal("rounded-binary.json", NULL, UNKNOWN, 1, "/request/form", "unknown form 'sonata'");
    expect_form_refusal("rounded-binary.json", NULL, VERSION_ONE, 1, "/request/form", "schema_version 1 is the game loop");
    expect_form_refusal("rounded-binary.json", NULL, SECTIONS, 1, "/request/sections", "takes its sections from its form");
    expect_form_refusal("rounded-binary.json", NULL, VOICING, 1, "/request/voicing", "no voicing");
    expect_form_refusal("rounded-binary.json", NULL, BARS, 1, "/request/bars", "has no size of 20 bars; it has 16, 24 bars");
    expect_form_refusal("rounded-binary.json", NULL, COUNT, 1, "/request/count", "has no size of count 9");
    expect_form_refusal("loop-and-ending.json", plan, GAME_LOOP_BARS, 3, "/request/bars", "takes its sections from the request");
    expect_form_refusal("loop-and-ending.json", plan, GAME_LOOP_MODULATE, 3, "/request/modulate", "takes its sections from the request");
    /* The ending version's MusicXML of a form that has none. */
    cJSON *request = api_request(piece_fixture("rounded-binary.json", NULL), 1);
    assert(cJSON_AddBoolToObject(request, "ending_musicxml", true));
    char *text = print(request);
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/ending_musicxml", "the rounded-binary form has no ending version");
    free(text);
}

/* The section of a regenerate must be one the plan has. */
static void test_refused_form_edit(const char *rounded_plan) {
    cJSON *request = api_request(piece_fixture("rounded-binary.json", NULL), 1);
    add_edit(request, rounded_plan, "C", "melody");
    char *text = print(request);
    expect_refusal(text, false, COMPOSER_ERR_REQUEST, "/regenerate/section", "the plan has no section 'C'");
    free(text);
}

/* Pieces of the other forms: composed, written again in a section, varied, and the refusals that need a plan. */
static void test_any_form(const char *plan) {
    char *rounded_plan = NULL;
    test_composed_forms(&rounded_plan);
    test_edited_forms(rounded_plan);
    test_game_loop_version_two(plan);
    test_refused_form_edit(rounded_plan);
    free(rounded_plan);
}

/*
 * With the argument "quick" only the cancel at the first step and the refused requests run, which stop before a piece is
 * heard: the valgrind run uses it, because hearing a piece leaks in the Analyzer's make_kernel_window (notes.md).
 */
int main(int argc, char **argv) {
    bool quick = argc > 1 && strcmp(argv[1], "quick") == 0;
    /* The quick run takes the plan of the theme snapshot: composing one hears it. */
    char *plan = quick ? test_read_file(SNAPSHOT("theme-one-motive.plan.json"), NULL) : theme_plan();
    if (quick) {
        /* The edits need a loop plan: the theme plan stands in, which the refused requests never read. */
        loop_plan_of_theme_fill = strdup(plan);
        test_cancel(plan, true);
        test_refused(plan);
        test_refused_forms(plan);
    } else {
        test_snapshots(plan);
        test_variations(plan);
        test_outputs(plan);
        test_kept_sections(plan);
        test_regenerate_theme_harmony();
        test_cancel(plan, false);
        test_cancel_edit(plan);
        test_refused(plan);
        test_refused_edits(plan);
        test_any_form(plan);
    }
    free(loop_plan_of_theme_fill);
    free(plan);
    printf("test_api_piece: ok\n");
    return 0;
}
