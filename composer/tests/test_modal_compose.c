#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <plan/plan.h>
#include <plan/plan_json.h>
#include <plan/plan_lanes.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/piece_plan_keys.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_modal_theme.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Composing in the church modes (R-725): a theme in each mode composes with every chord of its harmony lane in the mode's
 * collection and written the mode's way, a random motive in a mode has the tonic its rotation names, and a game loop in A dorian
 * composes.
 */

#define MAX_CHORDS 64

typedef struct {
    const char *word;
    int tonic; /* the default tonic of a random motive: the major degree the mode rotates from */
    unsigned seed;
} Mode;

/* seed: the first of listen's draws (--seed 500 onward) that grows a theme in the mode */
static const Mode MODES[] = {{"dorian", 2, 500}, {"mixolydian", 7, 500}, {"lydian", 5, 500}, {"phrygian", 4, 506}, {"locrian", 11, 500}, {"minor", 9, 0}, {"major", 0, 0}};
#define MODE_COUNT (sizeof(MODES) / sizeof(MODES[0]))
#define CHURCH_MODES 5

static void test_random_motive_tonics(void) {
    for (size_t m = 0; m < MODE_COUNT; m++) {
        char request[256];
        snprintf(request, sizeof(request), "{\"material\": \"%s\", \"meter\": \"4/4\", \"count\": 1, \"seed\": \"500\"}", MODES[m].word);
        ComposerResponse response;
        memset(&response, 0, sizeof(response));
        assert(composer_api_random_motive(test_context(), request, NULL, &response) == URSATZ_OK && response.json != NULL);
        cJSON *document = cJSON_Parse(response.json);
        const cJSON *motive = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "motives"), 0), "motive");
        const cJSON *key = cJSON_GetObjectItemCaseSensitive(motive, "key");
        assert(cJSON_GetObjectItemCaseSensitive(key, "tonic_pitch_class")->valueint == MODES[m].tonic);
        cJSON_Delete(document);
        composer_response_destroy(&response);
    }
}

static void test_themes(void) {
    for (size_t m = 0; m < CHURCH_MODES; m++) {
        Plan *plan = NULL;
        assert(compose_theme(MODES[m].word, MODES[m].seed, &plan));
        SpelledKey key = test_key(0, MODES[m].word);
        HarmonyKind kind = harmony_kind(test_composer_chords(), &key);
        assert(kind.modal == test_material(MODES[m].word));
        LaneSpanId ids[MAX_CHORDS];
        size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, plan_root(plan)), ids, MAX_CHORDS);
        assert(count >= 4 && count <= MAX_CHORDS);
        for (size_t i = 0; i < count; i++) {
            HarmonyChord chord;
            int root_pc;
            int intervals[4];
            size_t interval_count;
            const char *roman = plan_lane_span_payload(plan, ids[i])->harmony.roman_numeral;
            assert(harmony_roman_parse_in(&kind, roman, &chord));
            assert(composer_chord_tones(test_composer_chords(), &key, &chord, &root_pc, intervals, &interval_count));
            for (size_t k = 0; k < interval_count; k++) {
                assert(pitch_material_contains(kind.modal, (root_pc + intervals[k]) % 12));
            }
        }
        plan_destroy(plan);
    }
}

static void test_game_loop(void) {
    cJSON *request = fixture_json("piece/loop-and-ending.json");
    cJSON_ReplaceItemInObject(request, "key", cJSON_CreateString("A"));
    cJSON_DeleteItemFromObject(request, "mode");
    cJSON_AddStringToObject(request, "material", "dorian");
    cJSON_ReplaceItemInObject(request, "style", cJSON_CreateNull());
    cJSON *wrapped = cJSON_CreateObject();
    assert(cJSON_AddItemToObject(wrapped, "request", request) && cJSON_AddStringToObject(wrapped, "seed", "1"));
    char *text = cJSON_PrintUnformatted(wrapped);
    cJSON_Delete(wrapped);
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    UrsatzStatus status = composer_api_piece(test_context(), text, NULL, &response);
    assert(status == URSATZ_OK && response.json != NULL);
    free(text);
    assert(strstr(response.json, "\"key\":\"A dorian\"") != NULL || strstr(response.json, "\"key\": \"A dorian\"") != NULL);
    composer_response_destroy(&response);
}

int main(void) {
    test_random_motive_tonics();
    test_themes();
    test_game_loop();
    printf("test_modal_compose passed\n");
    return 0;
}
