#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_support.h"
#include "support/test_composer.h"

/*
 * The rows of a sonatina (P53 B3, R-525): B1 returns B at home, so in A minor its row names the copy across modes (`changes.mode`
 * "minor", the text `mode: mapped into minor`) and in C major it names none; E is in the elaboration key, F major in both.
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

static void check(const char *key, const char *mode, const char *b1_key, const char *b1_mode) {
    char request[320];
    snprintf(request, sizeof request,
             "{\"request\": {\"schema\": \"ursatz-piece-request\", \"schema_version\": 2, \"form\": \"sonatina\", "
             "\"key\": \"%s\", \"mode\": \"%s\", \"meter\": \"4/4\", \"bars\": 92, \"motive\": null}, \"seed\": 1}",
             key, mode);
    ComposerResponse response;
    ComposerStatus status = composer_api_piece(request, NULL, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "test_api_sonata: %s: %s\n", test_status_name(status), response.error.message);
    }
    assert(status == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response.json);
    assert(document);

    const cJSON *e = row_named(document, "E");
    if (strcmp(text(e, "key"), "F major") != 0) {
        fprintf(stderr, "E key '%s'\n", text(e, "key"));
    }
    assert(strcmp(text(e, "key"), "F major") == 0 && strcmp(text(e, "role"), "elaboration") == 0);

    const cJSON *b1 = row_named(document, "B1");
    if (strcmp(text(b1, "key"), b1_key) != 0) {
        fprintf(stderr, "B1 key '%s', expected '%s'\n", text(b1, "key"), b1_key);
    }
    assert(strcmp(text(b1, "key"), b1_key) == 0);
    const cJSON *changes = member(b1, "changes");
    assert(cJSON_IsObject(changes) && strcmp(text(changes, "variation"), "reharmonize") == 0);
    const cJSON *changed_mode = member(changes, "mode");
    if (b1_mode) {
        assert(cJSON_IsString(changed_mode) && strcmp(changed_mode->valuestring, b1_mode) == 0);
        if (!strstr(text(changes, "text"), "mode: mapped into minor")) {
            fprintf(stderr, "B1 text '%s'\n", text(changes, "text"));
        }
        assert(strstr(text(changes, "text"), "mode: mapped into minor"));
    } else {
        assert(cJSON_IsNull(changed_mode) && !strstr(text(changes, "text"), "mode:"));
    }
    cJSON_Delete(document);
    composer_response_destroy(&response);
}

int main(void) {
    check("A", "minor", "A minor", "minor");
    check("C", "major", "C major", NULL);
    printf("test_api_sonata: ok\n");
    return 0;
}
