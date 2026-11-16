#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_support.h"
#include "support/test_composer.h"

/*
 * The rows of a small rondo (P52 B7, R-521): A1 names its embellish variation and the notes it added, A2 names the new setting
 * of its accompaniment, B1 is in the tonic major (C major in C major, A major in A minor) and R1 is home.
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

static const cJSON *row_named(const cJSON *document, const char *id) {
    const cJSON *rows = member(member(member(document, "versions"), "piece"), "sections");
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

static void check(const char *key, const char *mode, const char *b1_key, const char *r1_key) {
    char request[320];
    snprintf(request, sizeof request,
             "{\"request\": {\"schema\": \"ursatz-piece-request\", \"schema_version\": 2, \"form\": \"small-rondo-ababa\", "
             "\"key\": \"%s\", \"mode\": \"%s\", \"meter\": \"4/4\", \"bars\": 68, \"motive\": null}, \"seed\": 1}",
             key, mode);
    ComposerResponse response;
    ComposerStatus status = composer_api_piece(request, NULL, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "test_api_rondo: %s: %s\n", test_status_name(status), response.error.message);
    }
    assert(status == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response.json);
    assert(document);

    const cJSON *a1 = member(row_named(document, "A1"), "changes");
    assert(cJSON_IsObject(a1) && strcmp(text(a1, "variation"), "embellish") == 0 && cJSON_IsNull(member(a1, "setting")));
    assert(cJSON_IsNumber(member(a1, "melody_added")) && member(a1, "melody_added")->valueint >= 1);

    const cJSON *a2 = member(row_named(document, "A2"), "changes");
    assert(cJSON_IsObject(a2) && strcmp(text(a2, "variation"), "setting") == 0 && strcmp(text(a2, "setting"), "arpeggiated") == 0);
    if (!strstr(text(a2, "text"), "new setting")) {
        fprintf(stderr, "A2 text '%s'\n", text(a2, "text"));
    }
    assert(strstr(text(a2, "text"), "new setting") && strstr(text(a2, "text"), "accompaniment: arpeggiated (new setting)"));

    const cJSON *b1 = row_named(document, "B1");
    if (strcmp(text(b1, "key"), b1_key) != 0) {
        fprintf(stderr, "B1 key '%s', expected '%s'\n", text(b1, "key"), b1_key);
    }
    assert(strcmp(text(b1, "key"), b1_key) == 0);
    assert(cJSON_IsNull(member(member(b1, "changes"), "setting")));
    assert(strcmp(text(row_named(document, "R1"), "key"), r1_key) == 0);
    cJSON_Delete(document);
    composer_response_destroy(&response);
}

int main(void) {
    check("C", "major", "C major", "C major");
    check("A", "minor", "A major", "A minor");
    printf("test_api_rondo: ok\n");
    return 0;
}
