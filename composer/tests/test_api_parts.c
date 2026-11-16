#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_support.h"
#include "support/test_composer.h"

/*
 * The section functions in the piece entry (P51 B8, R-510): the rows of a main-and-subordinate piece name each section's
 * function, the rows of a game loop with a transition and a coda do, and a request function that is neither is refused at its
 * path. The requests and responses are written beside the test, for the schema test.
 */

static const cJSON *member(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    assert(item);
    return item;
}

static const char *text(const cJSON *object, const char *name) {
    const cJSON *item = member(object, name);
    assert(cJSON_IsString(item));
    return item->valuestring;
}

static void write_pair(const char *tag, const char *request, const ComposerResponse *response) {
    char path[160];
    snprintf(path, sizeof path, "api_piece_request_%s.json", tag);
    test_write_file(path, request);
    snprintf(path, sizeof path, "api_piece_response_%s.json", tag);
    test_write_file(path, response->json);
}

static char *print(cJSON *request) {
    char *printed = cJSON_PrintUnformatted(request);
    assert(printed);
    cJSON_Delete(request);
    return printed;
}

static cJSON *composed(const char *printed, ComposerResponse *response) {
    ComposerStatus status = composer_api_piece(printed, NULL, response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "test_api_parts: %s: %s\n", test_status_name(status), response->error.message);
    }
    assert(status == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response->json);
    assert(document);
    return document;
}

/* The rows of the version of that name. */
static const cJSON *rows_of(const cJSON *document, const char *version) {
    const cJSON *rows = member(member(member(document, "versions"), version), "sections");
    assert(cJSON_IsArray(rows));
    return rows;
}

static const cJSON *row_named(const cJSON *rows, const char *id) {
    const cJSON *row = NULL;
    cJSON_ArrayForEach(row, rows) {
        if (strcmp(text(row, "id"), id) == 0) {
            return row;
        }
    }
    fprintf(stderr, "no row %s\n", id);
    assert(0);
    return NULL;
}

/* The row has no function (null) or the function of that kind, with the flags and the text. */
static void expect_function(const cJSON *row, const char *kind, bool independent, bool lyric, const char *words) {
    const cJSON *function = member(row, "function");
    if (!kind) {
        assert(cJSON_IsNull(function));
        return;
    }
    assert(cJSON_IsObject(function) && strcmp(text(function, "kind"), kind) == 0);
    assert(cJSON_IsBool(member(function, "independent")) && cJSON_IsTrue(member(function, "independent")) == independent);
    assert(cJSON_IsBool(member(function, "lyric")) && cJSON_IsTrue(member(function, "lyric")) == lyric);
    if (!strstr(text(function, "text"), words)) {
        fprintf(stderr, "row %s: function text '%s' lacks '%s'\n", text(row, "id"), text(function, "text"), words);
    }
    assert(strstr(text(function, "text"), words));
}

/* A main-and-subordinate piece (C major 4/4, 57 bars, seed 1): T, S1, S2, R and K carry their function, A, C and A return none. */
static void test_form_rows(void) {
    static const char REQUEST[] =
        "{\"request\": {\"schema\": \"ursatz-piece-request\", \"schema_version\": 2, \"form\": \"main-and-subordinate\", "
        "\"key\": \"C\", \"mode\": \"major\", \"meter\": \"4/4\", \"bars\": 57, \"motive\": null}, \"seed\": 1}";
    ComposerResponse response;
    cJSON *document = composed(REQUEST, &response);
    const cJSON *rows = rows_of(document, "piece");
    assert(cJSON_GetArraySize(rows) == 8);
    expect_function(row_named(rows, "A"), NULL, false, false, "");
    expect_function(row_named(rows, "T"), "transition", false, false, "a transition grown from the main theme, modulating to G major, liquidating");
    expect_function(row_named(rows, "S1"), "subordinate", false, false, "a subordinate theme");
    expect_function(row_named(rows, "S2"), "subordinate", false, true, "a subordinate theme, lyric");
    expect_function(row_named(rows, "C"), NULL, false, false, "");
    expect_function(row_named(rows, "R"), "retransition", false, false, "a retransition over the dominant pedal");
    expect_function(row_named(rows, "A return"), NULL, false, false, "");
    expect_function(row_named(rows, "K"), "coda", false, false, "a coda of 4, 2 and 2 bars");
    write_pair("main-and-subordinate", REQUEST, &response);
    cJSON_Delete(document);
    composer_response_destroy(&response);
}

static cJSON *motif_fill(void) {
    cJSON *fill = cJSON_Parse("{\"kind\": \"motif\", \"motive\": {\"degrees\": \"1:q 3:e 2:e 5:h\", \"pickup\": \"0\", \"harmony\": null, "
                              "\"features\": [], \"role\": \"melodic\"}, \"forms\": [], \"into\": null, \"hybrid\": null}");
    assert(fill);
    return fill;
}

static void add_section(cJSON *sections, const char *id, const char *part, const char *letter, int beats, const char *function, bool motif) {
    cJSON *section = cJSON_CreateObject();
    assert(section && cJSON_AddStringToObject(section, "id", id) && cJSON_AddStringToObject(section, "part", part) &&
           cJSON_AddStringToObject(section, "letter", letter) && cJSON_AddNumberToObject(section, "beats", beats) &&
           cJSON_AddStringToObject(section, "pickup", "0") && cJSON_AddNullToObject(section, "cadence"));
    assert(!function || cJSON_AddStringToObject(section, "function", function));
    cJSON *fill = motif ? motif_fill() : cJSON_Parse("{\"kind\": \"empty\"}");
    assert(fill && cJSON_AddItemToObject(section, "fill", fill) && cJSON_AddItemToArray(sections, section));
}

/* The game loop request: intro 4 bars, A 8, T 8 (a transition), B 8, turnaround 4, ending K 8 (a coda), C major 4/4. */
static cJSON *loop_request(const char *transition_function) {
    cJSON *piece = cJSON_Parse("{\"schema\": \"ursatz-piece-request\", \"schema_version\": 1, \"key\": \"C\", \"mode\": \"major\", "
                               "\"meter\": \"4/4\", \"motive\": null}");
    cJSON *sections = cJSON_CreateArray();
    assert(piece && sections && cJSON_AddItemToObject(piece, "sections", sections));
    add_section(sections, "intro", "intro", "I", 16, NULL, false);
    add_section(sections, "a", "body", "A", 32, NULL, true);
    add_section(sections, "t", "body", "T", 32, transition_function, true);
    add_section(sections, "b", "body", "B", 32, NULL, true);
    add_section(sections, "turn", "turnaround", "R", 16, NULL, false);
    add_section(sections, "k", "ending", "K", 32, transition_function ? "coda" : NULL, true);
    cJSON *request = cJSON_CreateObject();
    assert(request && cJSON_AddItemToObject(request, "request", piece) && cJSON_AddNumberToObject(request, "seed", 1));
    return request;
}

/* A game loop with a transition body section and a coda ending: the loop's rows and the ending's name those functions. */
static void test_loop_rows(void) {
    char *printed = print(loop_request("transition"));
    ComposerResponse response;
    cJSON *document = composed(printed, &response);
    const cJSON *loop = rows_of(document, "loop");
    const cJSON *ending = rows_of(document, "ending");
    assert(cJSON_GetArraySize(loop) == 5 && cJSON_GetArraySize(ending) == 5);
    const char *const IDS[] = {"intro", "a", "t", "b", "turn"};
    for (int i = 0; i < 5; i++) {
        const cJSON *row = row_named(loop, IDS[i]);
        assert(cJSON_IsString(member(row, "part")));
        if (strcmp(IDS[i], "t") == 0) {
            expect_function(row, "transition", false, false, "a transition grown from the main theme");
        } else {
            expect_function(row, NULL, false, false, "");
        }
    }
    expect_function(row_named(ending, "k"), "coda", false, false, "a coda of 4, 2 and 2 bars, each closing on the tonic");
    expect_function(row_named(ending, "t"), "transition", false, false, "a transition");
    expect_function(row_named(ending, "intro"), NULL, false, false, "");
    write_pair("loop-functions", printed, &response);
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(printed);
}

/* A function that is neither "transition" nor "coda" is refused at its path, and "none" is not a function a request gives. */
static void test_refusals(void) {
    static const char *const VALUES[] = {"bridge", "none", "retransition"};
    for (size_t v = 0; v < sizeof VALUES / sizeof VALUES[0]; v++) {
        cJSON *request = loop_request(NULL);
        cJSON *section = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(request, "request"), "sections"), 2);
        assert(cJSON_AddStringToObject(section, "function", VALUES[v]));
        char *printed = print(request);
        ComposerResponse response;
        ComposerStatus status = composer_api_piece(printed, NULL, &response);
        char expected[96];
        snprintf(expected, sizeof expected, "the function must be transition or coda, not '%s'", VALUES[v]);
        assert(status == COMPOSER_ERR_REQUEST);
        if (strcmp(response.error.path, "/request/sections/2/function") != 0 || !strstr(response.error.message, expected)) {
            fprintf(stderr, "'%s' at '%s'\n", response.error.message, response.error.path);
        }
        assert(strcmp(response.error.path, "/request/sections/2/function") == 0 && strstr(response.error.message, expected));
        composer_response_destroy(&response);
        free(printed);
    }
    /* A null function is none, as no member is. */
    cJSON *request = loop_request(NULL);
    cJSON *section = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(request, "request"), "sections"), 2);
    assert(cJSON_AddNullToObject(section, "function"));
    char *printed = print(request);
    ComposerResponse response;
    cJSON *document = composed(printed, &response);
    expect_function(row_named(rows_of(document, "loop"), "t"), NULL, false, false, "");
    cJSON_Delete(document);
    composer_response_destroy(&response);
    free(printed);
    /* A function the structure refuses (a coda on the loop body) surfaces as the bad-request failure. */
    request = loop_request(NULL);
    section = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(request, "request"), "sections"), 3);
    assert(cJSON_AddStringToObject(section, "function", "coda"));
    printed = print(request);
    assert(composer_api_piece(printed, NULL, &response) == COMPOSER_ERR_REQUEST);
    composer_response_destroy(&response);
    free(printed);
}

int main(void) {
    test_form_rows();
    test_loop_rows();
    test_refusals();
    printf("test_api_parts: ok\n");
    return 0;
}
