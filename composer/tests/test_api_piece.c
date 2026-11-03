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
    } else {
        test_snapshots(plan);
        test_variations(plan);
        test_outputs(plan);
        test_kept_sections(plan);
        test_cancel(plan, false);
        test_cancel_edit(plan);
        test_refused(plan);
        test_refused_edits(plan);
    }
    free(loop_plan_of_theme_fill);
    free(plan);
    printf("test_api_piece: ok\n");
    return 0;
}
