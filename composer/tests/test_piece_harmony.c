#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/harmony_schema.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * A section's harmony in a piece request (R-610): a progression, a galant schema or chords at bars, given before composing. The old
 * progression fill is the same request with an empty fill; a schema or chords of a ternary's B are Given from where they start; each
 * refusal is at the section's `harmony`. The seeds are the first of 1 to 16 that compose each request, recorded.
 */

#define SEED 1

static char *printed(cJSON *json) {
    char *text = cJSON_PrintUnformatted(json);
    assert(text);
    return text;
}

static ComposerStatus run(cJSON *request, ComposerResponse *response) {
    char *text = printed(request);
    ComposerStatus status = composer_api_piece(test_context(), text, NULL, response);
    free(text);
    cJSON_Delete(request);
    return status;
}

static cJSON *wrap(cJSON *piece) {
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "request", piece) && cJSON_AddNumberToObject(request, "seed", SEED));
    return request;
}

/* A ternary of 24 bars in C with the one overlay. */
static cJSON *ternary(const char *material, cJSON *overlay) {
    cJSON *piece = cJSON_CreateObject();
    cJSON *sections = cJSON_CreateArray();
    assert(cJSON_AddStringToObject(piece, "schema", "ursatz-piece-request") && cJSON_AddNumberToObject(piece, "schema_version", 2) &&
           cJSON_AddStringToObject(piece, "form", "ternary") && cJSON_AddStringToObject(piece, "key", "C") &&
           cJSON_AddStringToObject(piece, "material", material) && cJSON_AddStringToObject(piece, "meter", "4/4") &&
           cJSON_AddItemToObject(piece, "sections", sections) && cJSON_AddItemToArray(sections, overlay));
    return piece;
}

static cJSON *overlay(const char *name, const char *member, cJSON *value) {
    cJSON *item = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(item, "name", name) && cJSON_AddItemToObject(item, member, value));
    return item;
}

static cJSON *schema_harmony(const char *name) {
    cJSON *harmony = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(harmony, "schema", name));
    return harmony;
}

static cJSON *chords_harmony(int bar, const char *text) {
    cJSON *harmony = cJSON_CreateObject();
    cJSON *list = cJSON_AddArrayToObject(harmony, "chords");
    cJSON *chord = cJSON_CreateObject();
    assert(list && cJSON_AddNumberToObject(chord, "bar", bar) && cJSON_AddStringToObject(chord, "text", text) &&
           cJSON_AddItemToArray(list, chord));
    return harmony;
}

static void expect_refusal(cJSON *request, const char *path, const char *message) {
    ComposerResponse response;
    ComposerStatus status = run(request, &response);
    if (status != COMPOSER_ERR_REQUEST || strcmp(response.error.path, path) != 0 || !strstr(response.error.message, message)) {
        fprintf(stderr, "'%s' at '%s' (status %d), expected '%s' at '%s'\n", response.error.message, response.error.path, (int)status,
                message, path);
    }
    assert(status == COMPOSER_ERR_REQUEST && strcmp(response.error.path, path) == 0 && strstr(response.error.message, message));
    composer_response_destroy(&response);
}

static double time_of(const cJSON *rational) {
    return cJSON_GetObjectItemCaseSensitive(rational, "num")->valuedouble / cJSON_GetObjectItemCaseSensitive(rational, "den")->valuedouble;
}

static const cJSON *section_named(const cJSON *plan, const char *name) {
    const cJSON *node;
    cJSON_ArrayForEach(node, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(plan, "form"), "children")) {
        const cJSON *label = cJSON_GetObjectItemCaseSensitive(node, "label");
        if (cJSON_IsString(label) && strcmp(label->valuestring, name) == 0) {
            return node;
        }
    }
    return NULL;
}

/* The Given chords of the plan starting inside [from, to), in time order: their numerals and starts. */
static size_t given_chords(const cJSON *plan, double from, double to, char numerals[][16], double *starts, size_t capacity) {
    size_t count = 0;
    const cJSON *span;
    cJSON_ArrayForEach(span, cJSON_GetObjectItemCaseSensitive(plan, "chords")) {
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(span, "composer");
        double start = time_of(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(span, "span"), "start"));
        if (strcmp(cJSON_GetObjectItemCaseSensitive(composer, "state")->valuestring, "given") != 0 || start < from - 1e-9 || start >= to - 1e-9 ||
            count >= capacity) {
            continue;
        }
        size_t place = count++;
        while (place > 0 && starts[place - 1] > start) {
            starts[place] = starts[place - 1];
            memcpy(numerals[place], numerals[place - 1], 16);
            place--;
        }
        starts[place] = start;
        snprintf(numerals[place], 16, "%s",
                 cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(composer, "payload"), "roman_numeral")->valuestring);
    }
    return count;
}

/* --- the old progression fill ------------------------------------------------------------------------------------- */

static char *loop_plan(cJSON *piece) {
    ComposerResponse response;
    ComposerStatus status = run(wrap(piece), &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "%s\n", response.error.message);
    }
    assert(status == COMPOSER_OK);
    char *plan = strdup(response.plan_json);
    composer_response_destroy(&response);
    return plan;
}

/* A progression fill and the same progression as harmony on an empty fill compose the same plan. */
static void test_progression_is_the_old_fill(void) {
    char *text = test_must_read_file(COMPOSER_FIXTURE_DIR "/piece/progression-fill.json", NULL);
    cJSON *old_form = cJSON_Parse(text);
    cJSON *new_form = cJSON_Parse(text);
    free(text);
    assert(old_form && new_form);
    cJSON *section = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(new_form, "sections"), 1);
    cJSON *harmony = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(harmony, "progression", "i iv V i") && cJSON_AddNumberToObject(harmony, "cells", 2));
    cJSON *empty = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(empty, "kind", "empty"));
    cJSON_ReplaceItemInObjectCaseSensitive(section, "fill", empty);
    assert(cJSON_AddItemToObject(section, "harmony", harmony));
    char *before = loop_plan(old_form);
    char *after = loop_plan(new_form);
    assert(strcmp(before, after) == 0);
    free(before);
    free(after);
}

/* --- a schema and chords -------------------------------------------------------------------------------------------- */

static void test_schema(void) {
    ComposerResponse response;
    ComposerStatus status = run(wrap(ternary("major", overlay("B", "harmony", schema_harmony("prinner")))), &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "ternary with a schema: %s\n", response.error.message);
    }
    assert(status == COMPOSER_OK);
    cJSON *plan = cJSON_Parse(response.plan_json);
    composer_response_destroy(&response);
    assert(plan);
    /* The schema's chords in C major, one stage a cell from B's first bar, Given. */
    SpelledKey key = test_key(0, "major");
    char expected[GALANT_MAX_STAGES][PLAN_ROMAN_NUMERAL_MAX];
    size_t stages = 0;
    GalantSchemaId schema;
    assert(harmony_schema_find("prinner", &schema) && harmony_schema_chords(&key, schema, expected, &stages) && stages >= 2);
    const cJSON *b = section_named(plan, "B");
    double start = time_of(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(b, "span"), "start"));
    char numerals[16][16];
    double starts[16];
    size_t found = given_chords(plan, start, start + 8, numerals, starts, 16);
    assert(found == stages);
    for (size_t i = 0; i < stages; i++) {
        assert(strcmp(numerals[i], expected[i]) == 0);
        assert(i == 0 ? starts[0] == start : starts[i] > starts[i - 1]);
    }
    cJSON_Delete(plan);
}

static void test_chords(void) {
    ComposerResponse response;
    ComposerStatus status = run(wrap(ternary("minor", overlay("B", "harmony", chords_harmony(2, "V")))), &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "ternary with chords: %s\n", response.error.message);
    }
    assert(status == COMPOSER_OK);
    cJSON *plan = cJSON_Parse(response.plan_json);
    composer_response_destroy(&response);
    assert(plan);
    const cJSON *b = section_named(plan, "B");
    double start = time_of(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(b, "span"), "start"));
    char numerals[16][16];
    double starts[16];
    size_t found = given_chords(plan, start, start + 8, numerals, starts, 16);
    /* bar 2 is a bar after B's start: a whole note in 4/4 */
    assert(found == 1 && strcmp(numerals[0], "V") == 0 && starts[0] == start + 1.0);
    cJSON_Delete(plan);
}

/* --- refusals ------------------------------------------------------------------------------------------------------- */

static void test_refusals(void) {
    /* a theme brings its harmony: the minor theme's fill with harmony on the same section */
    char *text = test_must_read_file(COMPOSER_FIXTURE_DIR "/theme/one-motive.json", NULL);
    cJSON *theme_request = cJSON_Parse(text);
    free(text);
    assert(theme_request && cJSON_AddNumberToObject(theme_request, "seed", 1));
    char *body = printed(theme_request);
    cJSON_Delete(theme_request);
    ComposerResponse response;
    assert(composer_api_theme(test_context(), body, NULL, &response) == COMPOSER_OK);
    free(body);
    cJSON *plan = cJSON_Parse(response.plan_json);
    composer_response_destroy(&response);
    cJSON *item = overlay("B", "harmony", schema_harmony("prinner"));
    cJSON *fill = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(fill, "kind", "theme") && cJSON_AddItemToObject(fill, "plan", plan) && cJSON_AddItemToObject(item, "fill", fill));
    expect_refusal(wrap(ternary("minor", item)), "/request/sections/0/harmony", "a theme brings its harmony");
    /* a section that returns another is given nothing of its own */
    expect_refusal(wrap(ternary("minor", overlay("A return", "harmony", schema_harmony("prinner")))), "/request/sections/0/fill",
                   "returns 'A': give it to 'A'");
    /* a schema the key cannot hold, a name that is none, a chord that cannot be read, a bar outside the section */
    expect_refusal(wrap(ternary("minor", overlay("B", "harmony", schema_harmony("no-such-schema")))), "/request/sections/0/harmony",
                   "not one that makes a chord of each stage");
    expect_refusal(wrap(ternary("minor", overlay("B", "harmony", chords_harmony(2, "xyz")))), "/request/sections/0/harmony", "chords bar 2");
    expect_refusal(wrap(ternary("minor", overlay("B", "harmony", chords_harmony(9, "V")))), "/request/sections/0/harmony", "has bars 1 to 8");
    /* a progression longer than the section: 8 bars of B hold 16 cells */
    cJSON *progression = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(progression, "progression", "I V I V I V I V I") && cJSON_AddNumberToObject(progression, "cells", 2));
    expect_refusal(wrap(ternary("minor", overlay("B", "harmony", progression))), "/request/sections/0/harmony", "past the section's end");
    /* the old fill and harmony together */
    cJSON *both = overlay("B", "harmony", schema_harmony("prinner"));
    cJSON *legacy = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(legacy, "kind", "progression") && cJSON_AddStringToObject(legacy, "numerals", "I V") &&
           cJSON_AddItemToObject(both, "fill", legacy));
    expect_refusal(wrap(ternary("minor", both)), "/request/sections/0/fill/harmony", "not both");
}

int main(void) {
    test_progression_is_the_old_fill();
    test_schema();
    test_chords();
    test_refusals();
    return 0;
}
