#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <analysis/motive/motive_sketch.h>
#include <cJSON.h>
#include <plan/plan.h>
#include <plan/plan_json.h>
#include <plan/plan_lanes.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_random.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_modal_theme.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Color chords (R-727): the numerals with a color suffix parse to the snapshot's types and format back, a colored chord sounds its type's
 * members while every check reads the basic chord, a theme with harmony.color.share 1 colors chords with members in the key and keeps its
 * cadence, a motif plan takes a color suffix and its added tone is a chord tone, and the defaults draw what they drew.
 */

#define MAX_CHORDS 64

static const SpelledKey *c_major(void) {
    static SpelledKey key;
    key = test_key(0, "major");
    return &key;
}

static void expect_round_trip(const char *text, const char *type_id, size_t count, const int *intervals) {
    char roman[PLAN_ROMAN_NUMERAL_MAX];
    HarmonyChord chord;
    HarmonyKind kind = harmony_kind(test_composer_chords(), c_major());
    assert(harmony_roman_parse_in(&kind, text, &chord) && chord.color != NULL && strcmp(chord.color->id, type_id) == 0);
    int root;
    int tones[COMPOSER_CHORD_TONES_MAX];
    size_t tone_count;
    assert(composer_chord_tones(test_composer_chords(), c_major(), &chord, &root, tones, &tone_count) && tone_count == count);
    for (size_t i = 0; i < count; i++) {
        assert(tones[i] == intervals[i]);
    }
    assert(harmony_roman_format_in(&kind, &chord, roman) && strcmp(roman, text) == 0);
}

static void test_numerals(void) {
    static const int ADD9[] = {0, 4, 7, 14};
    static const int SIX_NINE[] = {0, 4, 7, 9, 14};
    expect_round_trip("Iadd9", "persichetti.chord-type.add9", 4, ADD9);
    expect_round_trip("IV69", "persichetti.chord-type.six-nine", 5, SIX_NINE);
    expect_round_trip("Imaj9", "persichetti.chord-type.major-ninth", 5, (const int[]){0, 4, 7, 11, 14});
    expect_round_trip("V9", "persichetti.chord-type.dominant-ninth", 5, (const int[]){0, 4, 7, 10, 14});
    expect_round_trip("ii9", "persichetti.chord-type.minor-ninth", 5, (const int[]){0, 3, 7, 10, 14});
    HarmonyChord chord;
    HarmonyKind kind = harmony_kind(test_composer_chords(), c_major());
    /* Iadd9 is C E G D, and every check reads it as I: the chord's basic triad is C E G. */
    assert(harmony_roman_parse_in(&kind, "Iadd9", &chord) && chord.degree == 1 && !chord.seventh);
    int root;
    int basic[4];
    size_t basic_count;
    assert(composer_chord_intervals(test_composer_chords(), HARMONY_MODE_MAJOR, NULL, &chord, &root, basic, &basic_count) && basic_count == 3 &&
           basic[1] == 4 && basic[2] == 7);
    /* A suffix with no type for the chord's quality is refused. */
    assert(!harmony_roman_parse_in(&kind, "Vmaj", &chord) && !harmony_roman_parse_in(&kind, "iadd6x", &chord));
    /* Without the run's chords a suffix is refused. */
    HarmonyKind plain = harmony_kind(NULL, c_major());
    assert(!harmony_roman_parse_in(&plain, "Iadd9", &chord));
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

/* The colored chords of a theme's plan: every member in C major, and the last two chords are V then I to every check. */
static size_t check_colored(const Plan *plan) {
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, plan_root(plan)), ids, MAX_CHORDS);
    HarmonyKind kind = harmony_kind(test_composer_chords(), c_major());
    size_t colored = 0;
    int degrees[MAX_CHORDS];
    for (size_t i = 0; i < count; i++) {
        const PlanHarmony *harmony = &plan_lane_span_payload(plan, ids[i])->harmony;
        HarmonyChord chord;
        assert(harmony_roman_parse_in(&kind, harmony->roman_numeral, &chord));
        degrees[i] = chord.degree;
        if (chord.color == NULL) {
            continue;
        }
        colored++;
        int root;
        int tones[COMPOSER_CHORD_TONES_MAX];
        size_t tone_count;
        assert(composer_chord_tones(test_composer_chords(), c_major(), &chord, &root, tones, &tone_count) && tone_count >= 3);
        for (size_t k = 0; k < tone_count; k++) {
            assert(pitch_material_contains(test_material("major"), (root + tones[k]) % 12));
        }
    }
    assert(count >= 2 && degrees[count - 2] == 5 && degrees[count - 1] == 1);
    return colored;
}

static void test_theme_share(void) {
    size_t composed = 0;
    for (unsigned seed = 1; seed <= 6; seed++) {
        Plan *plan = NULL;
        char *text = NULL;
        if (!compose_free_theme(seed, "{\"harmony\": {\"color\": {\"share\": \"1/1\"}}}", &plan, &text)) {
            continue;
        }
        composed++;
        assert(check_colored(plan) >= 1);
        /* The plan's file keeps the colored numerals. */
        Plan *again = NULL;
        PlanJsonError error;
        assert(plan_from_json(test_knowledge(), text, composer_plan_read_hooks(), &again, &error) == PLAN_OK);
        assert(check_colored(again) == check_colored(plan));
        plan_destroy(again);
        plan_destroy(plan);
        free(text);
    }
    assert(composed >= 4);
}

static void test_defaults_unchanged(void) {
    for (unsigned seed = 1; seed <= 3; seed++) {
        Plan *with_zero = NULL;
        Plan *plain = NULL;
        char *zero_text = NULL;
        char *plain_text = NULL;
        bool first = compose_free_theme(seed, "{\"harmony\": {\"color\": {\"share\": \"0/1\"}}, \"motif\": {\"color_chords\": \"0/1\"}}", &with_zero, &zero_text);
        bool second = compose_free_theme(seed, NULL, &plain, &plain_text);
        assert(first == second);
        if (first) {
            assert(strcmp(zero_text, plain_text) == 0 && check_colored(plain) == 0);
            plan_destroy(with_zero);
            plan_destroy(plain);
            free(zero_text);
            free(plain_text);
        }
    }
}

static MotiveRandomOptions options_for(unsigned long seed, const MotiveRandomPreferences *preferences, const ComposerColor *color) {
    MotiveRandomOptions options;
    memset(&options, 0, sizeof(options));
    options.material = test_material("major");
    options.knowledge = test_knowledge();
    options.meter.beats_per_bar = 4;
    options.meter.beat_unit = 4;
    options.form = MOTIVE_SKETCH_FORM_FULL;
    options.strategy = MOTIVE_RANDOM_STRATEGY_SEGMENTS;
    options.count = 1;
    options.seed = seed;
    options.preferences = preferences;
    options.color = color;
    return options;
}

static bool pitch_class_in(const int *pcs, size_t count, int pc) {
    for (size_t i = 0; i < count; i++) {
        if (pcs[i] == pc) {
            return true;
        }
    }
    return false;
}

/* A motif with plan I Vadd9: the second chord holds A beside G B D, and a note sounding A inside it reads as a chord tone. */
static void test_motif_plan(void) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.plan_count = 2;
    preferences.plan[0] = (MotiveRandomPlanChord){1, 0, 0, ""};
    preferences.plan[1] = (MotiveRandomPlanChord){5, 0, 0, "add9"};
    size_t made = 0;
    size_t heard = 0;
    for (unsigned long seed = 1; seed <= 12; seed++) {
        MotiveRandomOptions options = options_for(seed, &preferences, NULL);
        Motive motive;
        size_t count = 0;
        if (motive_random(&options, &motive, &count) != MOTIVE_RANDOM_OK || count != 1) {
            continue;
        }
        made++;
        assert(motive.chord_count == 2 && motive.chords[0].color == NULL && motive.chords[1].degree == 5 && motive.chords[1].color != NULL &&
               strcmp(motive.chords[1].color->id, "persichetti.chord-type.add9") == 0);
        int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
        size_t tones = motive_sketch_chord_tones(&motive, &motive.chords[1], pcs);
        assert(tones == 4 && pitch_class_in(pcs, tones, 9) && pitch_class_in(pcs, tones, 7) && pitch_class_in(pcs, tones, 11) &&
               pitch_class_in(pcs, tones, 2));
        MotiveAnalysis analysis;
        assert(motive_analyze(&motive, &analysis) && analysis.tones_known);
        for (size_t i = 0; i < motive.note_count; i++) {
            const MotiveSketchChord *chord = &motive.chords[1];
            if (rational_cmp(motive.notes[i].onset, chord->onset) >= 0 && motive_pitch(&motive, i) % 12 == 9 &&
                analysis.tones[i].role == MOTIVE_TONE_CHORD) {
                heard++;
            }
        }
        /* The motive's harmony text keeps the suffix and reads back to the same chords. */
        char *text = motive_harmony_to_text(&motive);
        assert(text != NULL && strstr(text, "Vadd9") != NULL);
        MotiveSketchChord back[MOTIVE_MAX_CHORDS];
        size_t back_count = 0;
        MotiveError error;
        assert(motive_harmony_parse_in(test_knowledge(), text, &(PlanMeter){4, 4, 0}, back, &back_count, &error) == MOTIVE_OK && back_count == 2 &&
               back[1].color == motive.chords[1].color);
        free(text);
    }
    assert(made >= 6 && heard >= 1);
    /* A chord that is not a major triad of the key, and a suffix no motif takes, are refused. */
    MotiveRandomPlanChord minor_chord[] = {{1, 0, 0, ""}, {2, 0, 0, "add9"}};
    MotiveRandomPreferences refused = preferences;
    memcpy(refused.plan, minor_chord, sizeof(minor_chord));
    MotiveRandomOptions options = options_for(1, &refused, NULL);
    Motive motive;
    size_t count = 0;
    assert(motive_random(&options, &motive, &count) == MOTIVE_RANDOM_ERR_PLAN);
    MotiveSketchChord parsed[MOTIVE_MAX_CHORDS];
    size_t parsed_count = 0;
    MotiveError error;
    assert(motive_harmony_parse("I V@2", &(PlanMeter){4, 4, 0}, parsed, &parsed_count, &error) == MOTIVE_OK);
    assert(motive_harmony_parse("I Vadd9@2", &(PlanMeter){4, 4, 0}, parsed, &parsed_count, &error) != MOTIVE_OK); /* no snapshot, no suffix */
}

/* motif.color_chords 1: every drawn plan chord that a type fits takes one, its members all in C major. */
static void test_motif_share(void) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.color_chords.num = 1;
    preferences.color_chords.den = 1;
    ComposerColor color;
    composer_color_default(&color);
    size_t colored = 0;
    for (unsigned long seed = 1; seed <= 10; seed++) {
        MotiveRandomOptions options = options_for(seed, &preferences, &color);
        Motive motive;
        size_t count = 0;
        if (motive_random(&options, &motive, &count) != MOTIVE_RANDOM_OK || count != 1) {
            continue;
        }
        for (size_t c = 0; c < motive.chord_count; c++) {
            if (motive.chords[c].color == NULL) {
                continue;
            }
            colored++;
            int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
            size_t tones = motive_sketch_chord_tones(&motive, &motive.chords[c], pcs);
            assert(tones >= 4);
            for (size_t k = 0; k < tones; k++) {
                assert(pitch_material_contains(test_material("major"), pcs[k]));
            }
        }
    }
    assert(colored >= 5);
    /* With no share, the same seeds draw the motives of the default preferences. */
    for (unsigned long seed = 1; seed <= 4; seed++) {
        MotiveRandomPreferences none;
        motive_random_preferences_default(&none);
        MotiveRandomOptions with_color = options_for(seed, &none, &color);
        MotiveRandomOptions without = options_for(seed, NULL, NULL);
        Motive a;
        Motive b;
        size_t count_a = 0;
        size_t count_b = 0;
        MotiveRandomResult first = motive_random(&with_color, &a, &count_a);
        MotiveRandomResult second = motive_random(&without, &b, &count_b);
        assert(first == second);
        if (first == MOTIVE_RANDOM_OK) {
            assert(a.note_count == b.note_count && a.chord_count == b.chord_count);
            for (size_t i = 0; i < a.note_count; i++) {
                assert(a.notes[i].pitch == b.notes[i].pitch);
            }
        }
    }
}

/* A given chord of a theme request takes a suffix when its type fits the key's chord there: Iadd9 does, IIadd9 (a minor triad) does not. */
static void test_given_chords(void) {
    const char *texts[2] = {"Iadd9", "IIadd9"};
    for (int k = 0; k < 2; k++) {
        cJSON *request = fixture_json("theme/one-motive.json");
        cJSON_ReplaceItemInObject(request, "key", cJSON_CreateString("C"));
        cJSON_DeleteItemFromObject(request, "mode");
        cJSON_AddStringToObject(request, "material", "major");
        cJSON *chords = cJSON_AddArrayToObject(request, "chords");
        cJSON *chord = cJSON_CreateObject();
        cJSON_AddNumberToObject(chord, "bar", 0);
        cJSON_AddStringToObject(chord, "text", texts[k]);
        cJSON_AddItemToArray(chords, chord);
        cJSON_AddNumberToObject(request, "seed", 1);
        char *text = cJSON_PrintUnformatted(request);
        cJSON_Delete(request);
        ComposerResponse response;
        memset(&response, 0, sizeof(response));
        UrsatzStatus status = composer_api_theme(test_context(), text, NULL, &response);
        free(text);
        if (k == 0) {
            assert(status == URSATZ_OK);
        } else {
            assert(status == URSATZ_ERR_REQUEST && strcmp(response.error.path, "/chords/0") == 0 && strstr(response.error.message, "does not fit the key"));
        }
        composer_response_destroy(&response);
    }
}

int main(void) {
    test_numerals();
    test_theme_share();
    test_defaults_unchanged();
    test_motif_plan();
    test_motif_share();
    test_given_chords();
    printf("test_color_chords: ok\n");
    return 0;
}
