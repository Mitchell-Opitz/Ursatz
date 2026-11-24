#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <plan/plan_lanes.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_modal_theme.h"
#include "support/test_support.h"
#include "ursatz_test.h"
#include "util/util.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The mode's color note (R-732, R-733): in mixolydian themes (the draws of `listen --seed 500` onward) the characteristic seventh, Bb in
 * C mixolydian, sounds on a strong beat in most of them with the settings at their defaults, and in fewer with melody.color_note and
 * motif.color_note at 0; a C major theme is the same with the settings at 0 and at the most (a key with no characteristic step
 * draws nothing). A drawn motive in a mode has only chords on the tonic and the mode's primary degrees, and a given plan is kept.
 * The argument `smoke` runs the motive case, the major check and two seeds of the count, with no count asserted.
 */

#define THEMES 10
#define FIRST_SEED 500
#define SEVENTH_PC 10 /* Bb, the seventh of C mixolydian */

static const char *OFF = "{\"melody\": {\"color_note\": 0}, \"motif\": {\"color_note\": 0}}";

/* Whether a melody note on a strong beat of 4/4 (the first or third beat) is the pitch class. */
static bool strong_beat_has(const Plan *plan, int pitch_class) {
    LaneSpanId ids[256];
    size_t count = plan_lane_find(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, plan_root(plan)), ids, 256);
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        const PlanNotes notes = plan_lane_span_payload(plan, ids[i])->notes;
        for (size_t n = 0; n < notes.note_count; n++) {
            Rational onset;
            assert(rational_add(span.start, notes.notes[n].offset, &onset) == RATIONAL_OK);
            bool strong = (onset.num * 2) % (long)onset.den == 0;
            if (strong && util_pitch_class(notes.notes[n].midi) == pitch_class) {
                return true;
            }
        }
    }
    return false;
}

/* How many of the themes put the pitch class on a strong beat, and how many composed. */
static size_t count_strong(const char *word, const char *preferences, int pitch_class, size_t themes, size_t *composed) {
    size_t strong = 0;
    *composed = 0;
    for (unsigned seed = FIRST_SEED; seed < FIRST_SEED + themes; seed++) {
        Plan *plan = NULL;
        if (!compose_theme_with(word, seed, preferences, &plan)) {
            continue;
        }
        (*composed)++;
        if (getenv("TEST_MODAL_COLOR_SHOW")) {
            fprintf(stderr, "seed %u: %d\n", seed, strong_beat_has(plan, pitch_class) ? 1 : 0);
        }
        strong += strong_beat_has(plan, pitch_class) ? 1 : 0;
        plan_destroy(plan);
    }
    return strong;
}

static char *plan_text(const char *word, unsigned seed, const char *preferences) {
    Plan *plan = NULL;
    assert(compose_theme_with(word, seed, preferences, &plan));
    PlanJsonError error;
    char *text = plan_to_json(plan, test_plan_producer(), &error);
    assert(text);
    plan_destroy(plan);
    return text;
}

/* The harmony text of a random motive in C <word> (segments strategy), with the plan given or drawn. */
static void motive_harmony(const char *word, unsigned seed, const char *preferences, char *text, size_t size) {
    char request[512];
    ComposerResponse response;
    snprintf(request, sizeof(request), "{\"material\": \"%s\", \"meter\": \"4/4\", \"strategy\": \"segments\", \"count\": 1, \"seed\": \"%u\"%s%s}", word,
             seed, preferences ? ", \"preferences\": " : "", preferences ? preferences : "");
    cJSON *drawn = call_json(composer_api_random_motive, request, &response);
    assert(drawn);
    composer_response_destroy(&response);
    const cJSON *motive = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(drawn, "motives"), 0), "motive");
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(motive, "harmony");
    assert(cJSON_IsString(harmony));
    snprintf(text, size, "%s", harmony->valuestring);
    cJSON_Delete(drawn);
}

/* Every chord of the text (numerals before an @) is on the tonic or one of the mode's primary degrees. */
static void test_motive_chords(void) {
    static const struct {
        const char *word;
        const char *allowed;
    } MODES[] = {{"dorian", " I II IV "}, {"mixolydian", " I V VII "}, {"lydian", " I II VII "}, {"phrygian", " I II VII "}, {"locrian", " I II V "}};
    for (size_t m = 0; m < sizeof(MODES) / sizeof(MODES[0]); m++) {
        for (unsigned seed = FIRST_SEED; seed < FIRST_SEED + 4; seed++) {
            char text[128];
            motive_harmony(MODES[m].word, seed, NULL, text, sizeof(text));
            char *save = NULL;
            for (char *token = strtok_r(text, " ", &save); token; token = strtok_r(NULL, " ", &save)) {
                char numeral[16];
                snprintf(numeral, sizeof(numeral), " %.*s ", (int)strcspn(token, "@"), token);
                assert(strstr(MODES[m].allowed, numeral) != NULL);
            }
        }
    }
    /* A given plan keeps its degrees: V in C lydian is a chord the mode avoids, and the motive writes it as asked. */
    char text[128];
    motive_harmony("lydian", FIRST_SEED, "{\"motif\": {\"plan\": [\"I\", \"V\"]}}", text, sizeof(text));
    assert(strstr(text, "V") != NULL && strstr(text, "VII") == NULL);
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "smoke") == 0;
    test_motive_chords();
    size_t composed;
    size_t themes = smoke ? 2 : THEMES;
    size_t with_default = count_strong("mixolydian", getenv("TEST_MODAL_COLOR_PREFS"), SEVENTH_PC, themes, &composed);
    size_t default_composed = composed;
    size_t without = count_strong("mixolydian", OFF, SEVENTH_PC, themes, &composed);
    fprintf(stderr, "mixolydian: Bb on a strong beat in %zu of %zu themes by default, in %zu of %zu with the settings at 0\n", with_default,
            default_composed, without, composed);
    if (!smoke) {
        assert(default_composed >= 7 && composed >= 7);
        assert(with_default >= 7 && without < with_default);
    }

    /* A key with no characteristic step draws nothing at any value. */
    static const char *MAX = "{\"melody\": {\"color_note\": 20}, \"motif\": {\"color_note\": 8}}";
    for (unsigned seed = FIRST_SEED; seed < FIRST_SEED + 2; seed++) {
        char *a = plan_text("major", seed, OFF);
        char *b = plan_text("major", seed, MAX);
        assert(strcmp(a, b) == 0);
        free(a);
        free(b);
    }
    printf("test_modal_color passed\n");
    return 0;
}
