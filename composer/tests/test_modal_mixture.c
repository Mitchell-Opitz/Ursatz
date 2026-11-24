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
 * Modal mixture (R-729): a numeral the key's own chords do not read is a borrowed chord (iv and bVI in C major, IV in C minor), it
 * sounds the triad of the mode it is borrowed from, a theme with harmony.mixture.share 1 borrows its predominant chords and its plan
 * keeps the material, and share 0 changes nothing.
 */

#define MAX_CHORDS 64

static void expect_tones(const SpelledKey *key, const HarmonyChord *chord, int root_pc, int third, int fifth) {
    int root;
    int intervals[4];
    size_t count;
    assert(composer_chord_tones(test_composer_chords(), key, chord, &root, intervals, &count));
    assert(count == 3 && root == root_pc && intervals[0] == 0 && intervals[1] == third && intervals[2] == fifth);
}

static void test_numerals(void) {
    char roman[PLAN_ROMAN_NUMERAL_MAX];
    HarmonyChord chord;
    SpelledKey major = test_key(0, "major");
    HarmonyKind kind = harmony_kind(test_composer_chords(), &major);
    /* iv: F Ab C from the natural minor; bVI: Ab C Eb; bVII: Bb D F. */
    assert(harmony_roman_parse_in(&kind, "iv", &chord) && chord.degree == 4 && chord.borrowed_from == test_material("minor"));
    expect_tones(&major, &chord, 5, 3, 7);
    assert(harmony_roman_format_in(&kind, &chord, roman) && strcmp(roman, "iv") == 0);
    assert(harmony_roman_parse_in(&kind, "bVI", &chord) && chord.degree == 6 && chord.borrowed_from == test_material("minor"));
    expect_tones(&major, &chord, 8, 4, 7);
    assert(harmony_roman_format_in(&kind, &chord, roman) && strcmp(roman, "bVI") == 0);
    assert(harmony_roman_parse_in(&kind, "bVII", &chord) && chord.degree == 7 && chord.borrowed_from != NULL);
    expect_tones(&major, &chord, 10, 4, 7);
    /* The key's own numerals keep their meaning, and what no mode on the tonic holds is refused. */
    assert(harmony_roman_parse_in(&kind, "IV", &chord) && chord.borrowed_from == NULL);
    assert(harmony_roman_parse_in(&kind, "vi", &chord) && chord.borrowed_from == NULL);
    assert(!harmony_roman_parse_in(&kind, "VI", &chord) && !harmony_roman_parse_in(&kind, "bV7", &chord) && !harmony_roman_parse_in(&kind, "V/V", &chord));
    assert(!harmony_roman_parse_in(&kind, "i", &chord)); /* the tonic is never borrowed */
    SpelledKey minor = test_key(0, "minor");
    kind = harmony_kind(test_composer_chords(), &minor);
    assert(harmony_roman_parse_in(&kind, "IV", &chord) && chord.borrowed_from == test_material("dorian"));
    expect_tones(&minor, &chord, 5, 4, 7);
    assert(harmony_roman_format_in(&kind, &chord, roman) && strcmp(roman, "IV") == 0);
    assert(harmony_roman_parse_in(&kind, "iv", &chord) && chord.borrowed_from == NULL);
    /* In a modal key too: C dorian borrows its V from the major and its iv from the natural minor. */
    SpelledKey dorian = test_key(0, "dorian");
    kind = harmony_kind(test_composer_chords(), &dorian);
    assert(harmony_roman_parse_in(&kind, "bVI", &chord) && chord.borrowed_from != NULL);
    /* Without the run's chords nothing is borrowed. */
    HarmonyKind plain = harmony_kind(NULL, &major);
    assert(!harmony_roman_parse_in(&plain, "iv", &chord));
}

/* A C major sentence from a motive with no harmony of its own, so the phrase model deals its chords. */
static bool compose_free_theme(unsigned seed, const char *preferences, Plan **plan, char **plan_json) {
    cJSON *request = fixture_json("theme/one-motive.json");
    cJSON_ReplaceItemInObject(request, "key", cJSON_CreateString("C"));
    cJSON_DeleteItemFromObject(request, "mode");
    cJSON_AddStringToObject(request, "material", "major");
    cJSON_AddStringToObject(request, "development", "balanced");
    char number[16];
    snprintf(number, sizeof(number), "%u", seed);
    cJSON_AddStringToObject(request, "seed", number);
    cJSON_AddStringToObject(request, "suggest", "1");
    if (preferences) {
        cJSON_AddItemToObject(request, "preferences", cJSON_Parse(preferences));
    }
    char *text = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    UrsatzStatus status = composer_api_theme(test_context(), text, NULL, &response);
    free(text);
    bool composed = status == URSATZ_OK && response.plan_json != NULL;
    if (composed) {
        PlanJsonError error;
        assert(plan_from_json(test_knowledge(), response.plan_json, composer_plan_read_hooks(), plan, &error) == PLAN_OK);
        *plan_json = strdup(response.plan_json);
    }
    composer_response_destroy(&response);
    return composed;
}

static size_t count_borrowed(const Plan *plan) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, plan_root(plan)), ids, MAX_CHORDS);
    SpelledKey key = test_key(0, "major");
    HarmonyKind kind = harmony_kind(test_composer_chords(), &key);
    size_t borrowed = 0;
    for (size_t i = 0; i < count; i++) {
        const PlanHarmony *harmony = &plan_lane_span_payload(plan, ids[i])->harmony;
        HarmonyChord chord;
        assert(harmony_roman_parse_in(&kind, harmony->roman_numeral, &chord));
        if (harmony->borrowed_from != NULL) {
            borrowed++;
            /* The plan's material is the one the numeral names, a predominant chord, and every tone of it is in that mode. */
            assert(chord.borrowed_from == harmony->borrowed_from && harmony->has_function && harmony->function == HARMONIC_FUNCTION_PREDOMINANT);
            int root;
            int intervals[4];
            size_t tones;
            assert(composer_chord_tones(test_composer_chords(), &key, &chord, &root, intervals, &tones));
            for (size_t k = 0; k < tones; k++) {
                assert(pitch_material_contains(harmony->borrowed_from, (root + intervals[k]) % 12));
            }
        } else {
            assert(chord.borrowed_from == NULL);
        }
    }
    return borrowed;
}

static void test_theme_share(void) {
    size_t composed = 0;
    for (unsigned seed = 1; seed <= 6; seed++) {
        Plan *plan = NULL;
        char *text = NULL;
        if (!compose_free_theme(seed, "{\"harmony\": {\"mixture\": {\"share\": \"1/1\"}}}", &plan, &text)) {
            continue;
        }
        composed++;
        assert(count_borrowed(plan) >= 1);
        /* The plan's file keeps the material of every borrowed chord. */
        Plan *again = NULL;
        PlanJsonError error;
        assert(plan_from_json(test_knowledge(), text, composer_plan_read_hooks(), &again, &error) == PLAN_OK);
        assert(count_borrowed(again) == count_borrowed(plan));
        char *written = plan_to_json(again, test_plan_producer(), &error);
        assert(written != NULL && strstr(written, "\"borrowed_from\"") != NULL);
        free(written);
        plan_destroy(again);
        plan_destroy(plan);
        free(text);
    }
    assert(composed >= 4);
}

static void test_share_zero_changes_nothing(void) {
    for (unsigned seed = 1; seed <= 3; seed++) {
        Plan *with_zero = NULL;
        Plan *plain = NULL;
        char *zero_text = NULL;
        char *plain_text = NULL;
        bool first = compose_free_theme(seed, "{\"harmony\": {\"mixture\": {\"share\": \"0/1\"}}}", &with_zero, &zero_text);
        bool second = compose_free_theme(seed, NULL, &plain, &plain_text);
        assert(first == second);
        if (first) {
            assert(strcmp(zero_text, plain_text) == 0 && count_borrowed(plain) == 0);
            plan_destroy(with_zero);
            plan_destroy(plain);
            free(zero_text);
            free(plain_text);
        }
    }
}

int main(void) {
    test_numerals();
    test_theme_share();
    test_share_zero_changes_nothing();
    printf("test_modal_mixture: ok\n");
    return 0;
}
