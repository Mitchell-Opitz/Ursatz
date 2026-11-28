#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "piece/request_chords.h"
#include "structure/request_parse.h"
#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

/*
 * The palette entry (R-765): composer_api_palette for C minor with dorian gives the shared chords (Cm, Eb, Gm, Bb) the texts I, III, v and
 * VII (each read back by request_read_chords as that chord), F major IV:dorian and the color carriers; C major with lydian gives D major
 * II:lydian; refusals name their paths; the request and response are written for the schema test.
 */

static cJSON *palette(const char *request, const char *name, UrsatzStatus expected, char *message, size_t size) {
    ComposerResponse response;
    UrsatzStatus status = composer_api_palette(test_context(), request, &response);
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
            snprintf(path, sizeof path, "api_palette_response_%s.json", name);
            FILE *file = fopen(path, "wb");
            assert(file != NULL && fputs(response.json, file) >= 0);
            fclose(file);
            snprintf(path, sizeof path, "api_palette_request_%s.json", name);
            file = fopen(path, "wb");
            assert(file != NULL && fputs(request, file) >= 0);
            fclose(file);
        }
    }
    composer_response_destroy(&response);
    return document;
}

static const cJSON *chord_named(const cJSON *document, size_t material, const char *numeral) {
    const cJSON *chords = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "materials"), (int)material), "chords");
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        if (strcmp(cJSON_GetObjectItemCaseSensitive(chord, "numeral")->valuestring, numeral) == 0) {
            return chord;
        }
    }
    return NULL;
}

static const char *text_of(const cJSON *chord) {
    const cJSON *text = cJSON_GetObjectItemCaseSensitive(chord, "text");
    return cJSON_IsString(text) ? text->valuestring : "";
}

static size_t count_of(const cJSON *array) {
    return (size_t)cJSON_GetArraySize(array);
}

static bool has_numeral(const cJSON *array, const char *numeral) {
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        if (strcmp(item->valuestring, numeral) == 0) {
            return true;
        }
    }
    return false;
}

static void test_minor_with_dorian(void) {
    cJSON *document = palette("{\"key\": \"C minor\", \"with\": \"dorian\"}", "minor_dorian", URSATZ_OK, NULL, 0);
    const cJSON *shared = cJSON_GetObjectItemCaseSensitive(document, "shared");
    assert(count_of(shared) == 4 && has_numeral(shared, "i") && has_numeral(shared, "bIII") && has_numeral(shared, "v") && has_numeral(shared, "bVII"));
    /* the key's own chords: the texts the Composer reads */
    assert(strcmp(text_of(chord_named(document, 0, "i")), "I") == 0);
    assert(strcmp(text_of(chord_named(document, 0, "bIII")), "III") == 0);
    assert(strcmp(text_of(chord_named(document, 0, "v")), "v") == 0);
    assert(strcmp(text_of(chord_named(document, 0, "bVII")), "VII") == 0);
    /* Dorian's color carriers: ii (Dm), IV (F) and vio (A diminished: no text, the Composer writes no diminished chord with a mode) */
    const cJSON *four = chord_named(document, 1, "IV");
    assert(four != NULL && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(four, "color")) && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(four, "shared")));
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(four, "name")->valuestring, "F") == 0 && strcmp(text_of(four), "IV:dorian") == 0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(chord_named(document, 1, "ii"), "name")->valuestring, "Dm") == 0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(chord_named(document, 1, "vio"), "name")->valuestring, "Adim") == 0);
    const cJSON *color = cJSON_GetObjectItemCaseSensitive(document, "color");
    assert(count_of(color) == 2);
    const cJSON *dorian = cJSON_GetArrayItem(color, 1);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(dorian, "material")->valuestring, "omt.material.dorian") == 0);
    assert(count_of(cJSON_GetObjectItemCaseSensitive(dorian, "numerals")) == 3);
    const cJSON *aeolian = cJSON_GetArrayItem(color, 0);
    const cJSON *numerals = cJSON_GetObjectItemCaseSensitive(aeolian, "numerals");
    assert(count_of(numerals) == 3 && has_numeral(numerals, "iio") && has_numeral(numerals, "iv") && has_numeral(numerals, "bVI"));
    cJSON_Delete(document);
}

/* Each text of the palette, read back by request_read_chords in the key, is the chord it names (the numeral it gives is the one of the plan). */
static void test_texts_read_back(void) {
    cJSON *document = palette("{\"key\": \"C minor\", \"with\": \"dorian\"}", NULL, URSATZ_OK, NULL, 0);
    const cJSON *materials = cJSON_GetObjectItemCaseSensitive(document, "materials");
    const cJSON *material;
    size_t read = 0;
    cJSON_ArrayForEach(material, materials) {
        const cJSON *chord;
        cJSON_ArrayForEach(chord, cJSON_GetObjectItemCaseSensitive(material, "chords")) {
            const char *text = text_of(chord);
            if (text[0] == '\0') {
                continue;
            }
            cJSON *list = cJSON_Parse("[{\"bar\": 1, \"text\": \"\"}]");
            assert(list != NULL);
            cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(list, 0), "text"), text);
            PlanMeter meter = {4, 4, 0};
            SpelledKey key;
            assert(request_parse_key("C minor", test_knowledge(), &key));
            ComposerChords chords;
            assert(composer_chords_init(&chords, test_knowledge(), NULL, 0));
            GivenChord given[4];
            char numerals[4][PLAN_ROMAN_NUMERAL_MAX];
            size_t count = 0;
            UrsatzError error;
            assert(request_read_chords(&chords, list, "/chords", "the section", 1, &meter, &key, 1, given, numerals, 4, &count, &error) == URSATZ_OK);
            assert(count == 1 && numerals[0][0] != '\0');
            cJSON_Delete(list);
            read++;
        }
    }
    assert(read >= 12);
    cJSON_Delete(document);
}

static void test_major_with_lydian(void) {
    cJSON *document = palette("{\"key\": \"C major\", \"with\": \"lydian\"}", "major_lydian", URSATZ_OK, NULL, 0);
    const cJSON *two = chord_named(document, 1, "II");
    assert(two != NULL && strcmp(cJSON_GetObjectItemCaseSensitive(two, "name")->valuestring, "D") == 0 && strcmp(text_of(two), "II:lydian") == 0);
    cJSON_Delete(document);
    /* one material alone: nothing shared, nothing color */
    document = palette("{\"key\": \"D dorian\"}", "alone", URSATZ_OK, NULL, 0);
    assert(count_of(cJSON_GetObjectItemCaseSensitive(document, "materials")) == 1 && count_of(cJSON_GetObjectItemCaseSensitive(document, "shared")) == 0 &&
           count_of(cJSON_GetObjectItemCaseSensitive(document, "color")) == 0);
    cJSON_Delete(document);
}

static void test_refusals(void) {
    char message[512];
    palette("{\"with\": \"dorian\"}", NULL, URSATZ_ERR_REQUEST, message, sizeof message);
    assert(strncmp(message, "/key|", 5) == 0);
    palette("{\"key\": \"C minor\", \"with\": \"minor\"}", NULL, URSATZ_ERR_REQUEST, message, sizeof message);
    assert(strncmp(message, "/with|", 6) == 0 && strstr(message, "key's own mode") != NULL);
    palette("{\"key\": \"C minor\", \"with\": \"blues\"}", NULL, URSATZ_ERR_REQUEST, message, sizeof message);
    assert(strncmp(message, "/with|", 6) == 0 && strstr(message, "not a mode the Composer writes") != NULL);
    palette("{\"key\": \"H minor\"}", NULL, URSATZ_ERR_REQUEST, message, sizeof message);
    assert(strncmp(message, "/key|", 5) == 0);
    palette("{\"key\": \"C minor\", \"seed\": 3}", NULL, URSATZ_ERR_REQUEST, message, sizeof message);
    assert(strstr(message, "seed") != NULL);
    palette("[]", NULL, URSATZ_ERR_REQUEST, message, sizeof message);
}

int main(void) {
    test_minor_with_dorian();
    test_texts_read_back();
    test_major_with_lydian();
    test_refusals();
    printf("test_palette passed\n");
    return 0;
}
