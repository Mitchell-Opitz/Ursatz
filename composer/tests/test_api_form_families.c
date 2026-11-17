#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_support.h"
#include "support/test_composer.h"
#include "ursatz_test.h"

/*
 * The rows of the rondo and sonata forms in the piece document, one table and one test per family:
 *
 *   test_api_form_families rondo | sonata
 *
 * rondo, the rows of a small rondo (P52 B7, R-521): A1 names its embellish variation and the notes it added, A2 names the new
 * setting of its accompaniment, B1 is in the tonic major (C major in C major, A major in A minor) and R1 is home.
 *
 * sonata, the rows of a sonatina (P53 B3, R-525): B1 returns B at home, so in A minor its row names the copy across modes
 * (`changes.mode` "minor", the text `mode: mapped into minor`) and in C major it names none; E is in the elaboration key, F
 * major in both.
 */

/* One piece requested and its rows checked: the key and mode, the key of B1 and `other`, the key of R1 (rondo) or the mode of
   B1's copy (sonata; NULL for none). */
typedef struct {
    const char *key;
    const char *mode;
    const char *b1_key;
    const char *other;
} ApiCase;

typedef struct ApiFamily ApiFamily;

struct ApiFamily {
    const char *name;
    const char *form;
    int bars;
    void (*check)(const cJSON *document, const ApiCase *row);
    ApiCase cases[2];
};

static const cJSON *member(const cJSON *object, const char *name) {
    const cJSON *item = test_json_member(object, name);
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

/* A small rondo: A1 and A2 name their changes, B1 is in the tonic major with no setting and R1 is home. */
static void check_rondo_rows(const cJSON *document, const ApiCase *row) {
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
    if (strcmp(text(b1, "key"), row->b1_key) != 0) {
        fprintf(stderr, "B1 key '%s', expected '%s'\n", text(b1, "key"), row->b1_key);
    }
    assert(strcmp(text(b1, "key"), row->b1_key) == 0);
    assert(cJSON_IsNull(member(member(b1, "changes"), "setting")));
    assert(strcmp(text(row_named(document, "R1"), "key"), row->other) == 0);
}

/* A sonatina: E is in the elaboration key, B1 returns B at home, with the copy across modes named in a minor piece. */
static void check_sonata_rows(const cJSON *document, const ApiCase *row) {
    const cJSON *e = row_named(document, "E");
    if (strcmp(text(e, "key"), "F major") != 0) {
        fprintf(stderr, "E key '%s'\n", text(e, "key"));
    }
    assert(strcmp(text(e, "key"), "F major") == 0 && strcmp(text(e, "role"), "elaboration") == 0);

    const cJSON *b1 = row_named(document, "B1");
    if (strcmp(text(b1, "key"), row->b1_key) != 0) {
        fprintf(stderr, "B1 key '%s', expected '%s'\n", text(b1, "key"), row->b1_key);
    }
    assert(strcmp(text(b1, "key"), row->b1_key) == 0);
    const cJSON *changes = member(b1, "changes");
    assert(cJSON_IsObject(changes) && strcmp(text(changes, "variation"), "reharmonize") == 0);
    const cJSON *changed_mode = member(changes, "mode");
    if (row->other) {
        assert(cJSON_IsString(changed_mode) && strcmp(changed_mode->valuestring, row->other) == 0);
        if (!strstr(text(changes, "text"), "mode: mapped into minor")) {
            fprintf(stderr, "B1 text '%s'\n", text(changes, "text"));
        }
        assert(strstr(text(changes, "text"), "mode: mapped into minor"));
    } else {
        assert(cJSON_IsNull(changed_mode) && !strstr(text(changes, "text"), "mode:"));
    }
}

static const ApiFamily FAMILIES[] = {
    {"rondo", "small-rondo-ababa", 68, check_rondo_rows, {{"C", "major", "C major", "C major"}, {"A", "minor", "A major", "A minor"}}},
    {"sonata", "sonatina", 92, check_sonata_rows, {{"A", "minor", "A minor", "minor"}, {"C", "major", "C major", NULL}}},
};
#define FAMILY_COUNT (sizeof(FAMILIES) / sizeof(FAMILIES[0]))

static void check(const ApiFamily *family, const ApiCase *row) {
    char request[320];
    snprintf(request, sizeof request,
             "{\"request\": {\"schema\": \"ursatz-piece-request\", \"schema_version\": 2, \"form\": \"%s\", "
             "\"key\": \"%s\", \"mode\": \"%s\", \"meter\": \"4/4\", \"bars\": %d, \"motive\": null}, \"seed\": 1}",
             family->form, row->key, row->mode, family->bars);
    ComposerResponse response;
    ComposerStatus status = composer_api_piece(request, NULL, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "test_api_%s: %s: %s\n", family->name, test_status_name(status), response.error.message);
    }
    assert(status == COMPOSER_OK);
    cJSON *document = cJSON_Parse(response.json);
    assert(document);
    family->check(document, row);
    cJSON_Delete(document);
    composer_response_destroy(&response);
}

int main(int argc, char **argv) {
    for (size_t i = 0; argc == 2 && i < FAMILY_COUNT; i++) {
        if (strcmp(argv[1], FAMILIES[i].name) == 0) {
            for (size_t c = 0; c < sizeof(FAMILIES[i].cases) / sizeof(FAMILIES[i].cases[0]); c++) {
                check(&FAMILIES[i], &FAMILIES[i].cases[c]);
            }
            printf("test_api_%s: ok\n", FAMILIES[i].name);
            return 0;
        }
    }
    fprintf(stderr, "usage: test_api_form_families rondo|sonata\n");
    return 2;
}
