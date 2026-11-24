#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The R-611 piece (the Persona 5 move, gate 2 of P69; `r611.sh` is the same request): a ternary in C locrian, A given a locrian period
 * and B, two steps brighter than A, a minor sentence, both themes drawn at seed 1 from a motive with no harmony of its own (a theme from
 * `listen` has chords of its own, which a piece fill does not take) and the piece at seed 1. A and the A return are in C locrian, B in
 * C natural minor, and B's measured brightness (R-730, R-735) is at least 3/2 above A's.
 */

static char *printed(const cJSON *json) {
    char *text = cJSON_PrintUnformatted(json);
    assert(text);
    return text;
}

/* The plan of the one-motive theme of the given kind and material in C, seed 1, as a JSON object. */
static cJSON *theme_plan(const char *into, const char *material) {
    char *text = test_must_read_file(COMPOSER_FIXTURE_DIR "/theme/one-motive.json", NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request);
    cJSON_ReplaceItemInObjectCaseSensitive(request, "into", cJSON_CreateString(into));
    cJSON_ReplaceItemInObjectCaseSensitive(request, "key", cJSON_CreateString("C"));
    cJSON_DeleteItemFromObjectCaseSensitive(request, "mode");
    assert(cJSON_AddStringToObject(request, "material", material) && cJSON_AddNumberToObject(request, "seed", 1));
    char *body = printed(request);
    cJSON_Delete(request);
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    assert(composer_api_theme(test_context(), body, NULL, &response) == URSATZ_OK);
    free(body);
    cJSON *plan = cJSON_Parse(response.plan_json);
    composer_response_destroy(&response);
    assert(plan);
    return plan;
}

static cJSON *section(const char *name, cJSON *region, cJSON *plan) {
    cJSON *item = cJSON_CreateObject();
    cJSON *fill = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(item, "name", name) && cJSON_AddItemToObject(item, "region", region));
    assert(cJSON_AddStringToObject(fill, "kind", "theme") && cJSON_AddItemToObject(fill, "plan", plan) && cJSON_AddItemToObject(item, "fill", fill));
    return item;
}

/* The row of a section of the response: its key text and brightness as a ratio. */
static const cJSON *row_of(const cJSON *response, size_t index) {
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(response, "versions"), "piece"), "sections");
    assert(cJSON_GetArraySize(rows) == 3);
    return cJSON_GetArrayItem(rows, (int)index);
}

static void brightness(const cJSON *row, long *num, long *den) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(row, "brightness");
    assert(cJSON_IsObject(value));
    *num = (long)cJSON_GetObjectItemCaseSensitive(value, "numerator")->valuedouble;
    *den = (long)cJSON_GetObjectItemCaseSensitive(value, "denominator")->valuedouble;
}

int main(void) {
    cJSON *piece = cJSON_CreateObject();
    cJSON *sections = cJSON_CreateArray();
    cJSON *region_a = cJSON_CreateObject();
    cJSON *region_b = cJSON_CreateObject();
    assert(cJSON_AddStringToObject(region_a, "tonic", "C") && cJSON_AddStringToObject(region_a, "material", "locrian"));
    assert(cJSON_AddStringToObject(region_b, "relation", "two steps brighter") && cJSON_AddStringToObject(region_b, "anchor", "A"));
    assert(cJSON_AddItemToArray(sections, section("A", region_a, theme_plan("period", "locrian"))));
    assert(cJSON_AddItemToArray(sections, section("B", region_b, theme_plan("sentence", "minor"))));
    assert(cJSON_AddStringToObject(piece, "schema", "ursatz-piece-request") && cJSON_AddNumberToObject(piece, "schema_version", 2) &&
           cJSON_AddStringToObject(piece, "form", "ternary") && cJSON_AddStringToObject(piece, "key", "C") &&
           cJSON_AddStringToObject(piece, "material", "locrian") && cJSON_AddStringToObject(piece, "meter", "4/4") &&
           cJSON_AddItemToObject(piece, "sections", sections));
    cJSON *request = cJSON_CreateObject();
    assert(cJSON_AddItemToObject(request, "request", piece) && cJSON_AddNumberToObject(request, "seed", 1));
    char *text = printed(request);
    cJSON_Delete(request);
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    UrsatzStatus status = composer_api_piece(test_context(), text, NULL, &response);
    free(text);
    if (status != URSATZ_OK) {
        fprintf(stderr, "the R-611 piece: %s\n", response.error.message);
    }
    assert(status == URSATZ_OK && response.json != NULL);
    cJSON *document = cJSON_Parse(response.json);
    composer_response_destroy(&response);
    assert(document);

    /* The regions: A and the A return in C locrian, B in C natural minor, two steps brighter than A. */
    const cJSON *a = row_of(document, 0);
    const cJSON *b = row_of(document, 1);
    const cJSON *again = row_of(document, 2);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(a, "id")->valuestring, "A") == 0 && strcmp(cJSON_GetObjectItemCaseSensitive(b, "id")->valuestring, "B") == 0 &&
           strcmp(cJSON_GetObjectItemCaseSensitive(again, "id")->valuestring, "A return") == 0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(a, "key")->valuestring, "C locrian") == 0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(b, "key")->valuestring, "C minor") == 0 &&
           strcmp(cJSON_GetObjectItemCaseSensitive(b, "region")->valuestring, "two steps brighter of A") == 0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(again, "key")->valuestring, "C locrian") == 0);

    /* B measures at least 3/2 brighter than A (R-730): the values this seed gives are 1 and 4. */
    long a_num;
    long a_den;
    long b_num;
    long b_den;
    brightness(a, &a_num, &a_den);
    brightness(b, &b_num, &b_den);
    assert(a_num == 1 && a_den == 1 && b_num == 4 && b_den == 1);
    assert(2 * (b_num * a_den - a_num * b_den) >= 3 * a_den * b_den);
    cJSON_Delete(document);
    printf("test_piece_r611 passed\n");
    return 0;
}
