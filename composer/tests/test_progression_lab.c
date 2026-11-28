#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <theory/knowledge/knowledge_snapshot.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The progression request with a palette (R-766): her sad-then-Dorian-lift plan at seed 1 (chords of bars 1 to 12 among i, III, v and VII, a chord
 * holding A natural in bar 13); each active progression schema's example is what the writer gives for its stages in that key (a rotation of it); a PAC
 * ends on the tonic after V; given chords are kept; the refusals name their paths; the accompaniment sounds every chord with no melody lane; `measures`
 * has a row for each bar; the request and the response are written for the schema test.
 */

static cJSON *lab(const char *request, const char *name, UrsatzStatus expected, char *message, size_t size) {
    ComposerResponse response;
    UrsatzStatus status = composer_api_progression(test_context(), request, NULL, &response);
    if (status != expected) {
        fprintf(stderr, "%s: %s: %s\n", request, test_status_name(status), response.error.message);
    }
    assert(status == expected);
    if (message != NULL) {
        snprintf(message, size, "%s|%s", response.error.path, response.error.message);
    }
    cJSON *document = NULL;
    if (expected == URSATZ_OK) {
        assert(response.json != NULL);
        document = cJSON_Parse(response.json);
        assert(document != NULL);
        if (name != NULL) {
            char path[96];
            snprintf(path, sizeof path, "api_progression_response_%s.json", name);
            FILE *file = fopen(path, "wb");
            assert(file != NULL && fputs(response.json, file) >= 0);
            fclose(file);
            snprintf(path, sizeof path, "api_progression_request_%s.json", name);
            file = fopen(path, "wb");
            assert(file != NULL && fputs(request, file) >= 0);
            fclose(file);
        }
    }
    composer_response_destroy(&response);
    return document;
}

static const char *member_text(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

/* The chord numeral of the row of a bar (the first chord of the bar). */
static const char *chord_of_bar(const cJSON *document, int bar) {
    const cJSON *row;
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(document, "chords")) {
        if ((int)cJSON_GetObjectItemCaseSensitive(row, "bar")->valuedouble == bar) {
            return member_text(row, "chord");
        }
    }
    return "";
}

static char *read_fixture(void) {
    return test_must_read_file(COMPOSER_FIXTURE_DIR "/harmony/sad-dorian-lift.json", NULL);
}

static void test_sad_dorian_lift(void) {
    char *request = read_fixture();
    cJSON *document = lab(request, "lab", URSATZ_OK, NULL, 0);
    free(request);
    assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(document, "heard")));
    for (int bar = 1; bar <= 12; bar++) {
        const char *chord = chord_of_bar(document, bar);
        assert(strcmp(chord, "i") == 0 || strcmp(chord, "III") == 0 || strcmp(chord, "v") == 0 || strcmp(chord, "VII") == 0);
    }
    /* the lift: Dm or F major, both with A natural */
    const char *lift = chord_of_bar(document, 13);
    assert(strcmp(lift, "ii") == 0 || strcmp(lift, "IV") == 0);
    const cJSON *measures = cJSON_GetObjectItemCaseSensitive(document, "measures");
    assert(cJSON_GetArraySize(measures) == 16 && (int)cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(measures, 15), "bar")->valuedouble == 16);
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(document, "harmony");
    assert(strcmp(member_text(harmony, "material"), "omt.material.natural-minor") == 0 && strcmp(member_text(harmony, "with"), "omt.material.dorian") == 0);
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(harmony, "chords")) == 16 && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(harmony, "stages")) >= 2);
    cJSON_Delete(document);
}

/* Whether `chords` (the writer's texts) is a rotation of `example`. */
static bool rotation_of(const char *const *chords, const cJSON *example, int count) {
    for (int shift = 0; shift < count; shift++) {
        bool equal = true;
        for (int i = 0; equal && i < count; i++) {
            equal = strcmp(chords[i], cJSON_GetArrayItem(example, (i + shift) % count)->valuestring) == 0;
        }
        if (equal) {
            return true;
        }
    }
    return false;
}

static void test_schema_examples(void) {
    const KnowledgeSnapshot *knowledge = test_knowledge();
    size_t checked = 0;
    for (size_t i = 0; i < knowledge_snapshot_schema_count(knowledge); i++) {
        const SchemaEntry *schema = knowledge_snapshot_schema_at(knowledge, i);
        if (schema->subtype != SCHEMA_PROGRESSION) {
            continue;
        }
        const cJSON *entry = knowledge_snapshot_entry_json(knowledge, schema->id);
        const cJSON *example = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(entry, "examples"), 0);
        const cJSON *chords = cJSON_GetObjectItemCaseSensitive(example, "chords");
        int count = cJSON_GetArraySize(chords);
        assert(example != NULL && count > 0);
        char request[512];
        snprintf(request, sizeof request,
                 "{\"key\": \"%s\", \"meter\": \"4/4\", \"bars\": %d, \"cadence\": \"none\", \"seed\": 1, \"palette\": {\"spans\": [{\"bars\": [1, %d], \"chords\": \"any\", "
                 "\"schema\": \"%s\", \"loop\": false}]}}",
                 member_text(example, "key"), count, count, schema->id);
        cJSON *document = lab(request, NULL, URSATZ_OK, NULL, 0);
        const char *written[64];
        const cJSON *rows = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "harmony"), "chords");
        assert(cJSON_GetArraySize(rows) == count);
        for (int bar = 0; bar < count; bar++) {
            written[bar] = member_text(cJSON_GetArrayItem(rows, bar), "text");
        }
        if (!rotation_of(written, chords, count)) {
            fprintf(stderr, "%s: the writer gives a different progression than its example\n", schema->id);
        }
        assert(rotation_of(written, chords, count));
        cJSON_Delete(document);
        checked++;
    }
    assert(checked == 15);
}

static void test_cadence_and_given(void) {
    cJSON *document = lab("{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 4, \"cadence\": \"PAC\", \"seed\": 3, \"palette\": {}}", NULL, URSATZ_OK, NULL, 0);
    assert(strcmp(chord_of_bar(document, 4), "I") == 0 && strcmp(chord_of_bar(document, 3), "V") == 0);
    cJSON_Delete(document);
    document = lab("{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 4, \"cadence\": \"none\", \"seed\": 3, \"palette\": {\"chords\": [{\"bar\": 2, \"text\": \"IV\"}, {\"bar\": 3, \"text\": \"ii\"}]}}",
                   NULL, URSATZ_OK, NULL, 0);
    assert(strcmp(chord_of_bar(document, 2), "IV") == 0 && strcmp(chord_of_bar(document, 3), "ii") == 0);
    cJSON_Delete(document);
    /* two chords a bar: the unit is a half bar */
    document = lab("{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 2, \"cadence\": \"none\", \"seed\": 2, \"palette\": {\"chords_per_bar\": 2, \"chords\": [{\"bar\": 1, \"text\": \"I V@2\"}]}}",
                   NULL, URSATZ_OK, NULL, 0);
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(document, "harmony");
    assert(strncmp(member_text(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(harmony, "chords"), 0), "text"), "I V@2", 5) == 0);
    cJSON_Delete(document);
}

static void test_refusals(void) {
    char message[512];
    lab("{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 4, \"cadence\": \"none\", \"palette\": {\"spans\": [{\"bars\": [1, 4], \"chords\": \"color\"}]}}", NULL,
        URSATZ_ERR_REQUEST, message, sizeof message);
    assert(strncmp(message, "/palette/spans/0/chords|", 24) == 0);
    lab("{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 4, \"cadence\": \"none\", \"embellish\": \"low\", \"palette\": {}}", NULL, URSATZ_ERR_REQUEST, message, sizeof message);
    assert(strncmp(message, "/embellish|", 11) == 0 && strstr(message, "leave out schema, sequence and embellish") != NULL);
    lab("{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 4, \"cadence\": \"DC\", \"palette\": {}}", NULL, URSATZ_ERR_REQUEST, message, sizeof message);
    assert(strncmp(message, "/cadence|", 9) == 0);
    lab("{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 4, \"cadence\": \"none\", \"palette\": {\"spans\": [{\"bars\": [1, 4], \"schema\": \"omt.schema.twelve-bar-blues\", \"loop\": false}]}}",
        NULL, URSATZ_ERR_REQUEST, message, sizeof message);
    assert(strncmp(message, "/palette/spans/0/schema|", 24) == 0);
    lab("{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 4, \"cadence\": \"none\", \"palette\": {\"spans\": [{\"bars\": [1, 2]}, {\"bars\": [2, 3]}]}}", NULL, URSATZ_ERR_REQUEST,
        message, sizeof message);
    assert(strncmp(message, "/palette/spans/1|", 17) == 0);
    lab("{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 4, \"cadence\": \"none\", \"palette\": {\"chords_per_bar\": 3}}", NULL, URSATZ_ERR_REQUEST, message, sizeof message);
    assert(strncmp(message, "/palette/chords_per_bar|", 24) == 0);
    lab("{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 4, \"cadence\": \"none\", \"palette\": {\"brightness\": {\"shape\": \"peak\"}}}", NULL, URSATZ_ERR_REQUEST, message, sizeof message);
    assert(strncmp(message, "/palette/brightness/at|", 23) == 0);
    lab("{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 4, \"cadence\": \"none\", \"accompaniment\": {\"pattern\": \"block\"}, \"embellish\": \"none\", \"palette\": {}}", NULL,
        URSATZ_ERR_REQUEST, message, sizeof message);
    lab("{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 4, \"cadence\": \"none\", \"palette\": {\"nope\": 1}}", NULL, URSATZ_ERR_REQUEST, message, sizeof message);
    assert(strncmp(message, "/palette/nope|", 14) == 0);
}

/* Every bar has an inner-voice note (the accompaniment sounds every chord) and the plan has no melody lane. */
static void test_accompaniment(void) {
    cJSON *document = lab("{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 4, \"cadence\": \"PAC\", \"seed\": 1, \"plans\": true, \"accompaniment\": {\"pattern\": \"block\"}, \"palette\": {}}",
                          NULL, URSATZ_OK, NULL, 0);
    const cJSON *plan = cJSON_GetObjectItemCaseSensitive(document, "plan");
    assert(plan != NULL && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(plan, "melody")) == 0);
    bool sounded[4] = {false, false, false, false};
    const cJSON *item;
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(plan, "texture")) {
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(item, "composer");
        if (strcmp(member_text(composer, "lane"), "inner_voice") != 0) {
            continue;
        }
        const cJSON *start = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(item, "span"), "start");
        double base = cJSON_GetObjectItemCaseSensitive(start, "num")->valuedouble / cJSON_GetObjectItemCaseSensitive(start, "den")->valuedouble;
        const cJSON *note;
        cJSON_ArrayForEach(note, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(composer, "payload"), "notes")) {
            const cJSON *offset = cJSON_GetObjectItemCaseSensitive(note, "offset");
            double at = base + cJSON_GetObjectItemCaseSensitive(offset, "num")->valuedouble / cJSON_GetObjectItemCaseSensitive(offset, "den")->valuedouble;
            if (at >= 0 && at < 4) {
                sounded[(int)at] = true;
            }
        }
    }
    for (int bar = 0; bar < 4; bar++) {
        assert(sounded[bar]);
    }
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(document, "measures")) == 4);
    cJSON_Delete(document);
}

static void test_suggest(void) {
    cJSON *document = lab("{\"key\": \"A minor\", \"meter\": \"3/4\", \"bars\": 4, \"cadence\": \"HC\", \"seed\": 4, \"suggest\": 3, \"palette\": {\"with\": \"dorian\"}}", NULL, URSATZ_OK, NULL, 0);
    const cJSON *alternatives = cJSON_GetObjectItemCaseSensitive(document, "alternatives");
    assert(cJSON_GetArraySize(alternatives) >= 1);
    const cJSON *first = cJSON_GetArrayItem(alternatives, 0);
    assert(cJSON_GetObjectItemCaseSensitive(first, "measures") != NULL && cJSON_GetObjectItemCaseSensitive(first, "harmony") != NULL);
    cJSON_Delete(document);
}

int main(void) {
    test_sad_dorian_lift();
    test_schema_examples();
    test_cadence_and_given();
    test_refusals();
    test_accompaniment();
    test_suggest();
    printf("test_progression_lab passed\n");
    return 0;
}
