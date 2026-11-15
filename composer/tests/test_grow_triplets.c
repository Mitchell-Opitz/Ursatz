#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_support.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Themes grown from motives with triplets, sixteenths and a compound figure (B3 of L2, R-435, R-442). Through
 * composer_api_theme, the degrees motive as the basic slot, for a sentence and a period over seeds 1 to 8 in C major:
 * every theme is OK, the realized basic idea keeps the motive's figure, and the cadential idea is not a copy of the
 * basic idea in most themes (a theme that scaled the rhythm of a motive with triplets, sixteenths or a compound figure
 * could not write the result before, and fell back to an interval change: at e14fa45 all 8 triplet themes had a
 * cadential idea with the basic idea's notes).
 */

#define SEEDS 8

/* "quick" (the smoke test run by CI): seed 1 of a sentence only. The full sweep takes about 90 s and is labeled slow. */
static unsigned seeds = SEEDS;
static size_t forms = 2;

typedef struct {
    long num;
    long den;
} Time;

typedef struct {
    Time start;
    Time end;
    size_t note_count;
    Time duration[32];
    int pitch[32];
} Idea;

static Time time_of(const cJSON *object) {
    Time time = {(long)cJSON_GetObjectItemCaseSensitive(object, "num")->valuedouble,
                 (long)cJSON_GetObjectItemCaseSensitive(object, "den")->valuedouble};
    return time;
}

static bool time_equal(Time a, Time b) {
    return a.num * b.den == b.num * a.den;
}

static bool time_less(Time a, Time b) {
    return a.num * b.den < b.num * a.den;
}

/* The request: tests/fixtures/theme/one-motive.json in C major, with the meter, the motive and the seed given. */
static char *request_of(const char *degrees, const char *meter, const char *into, unsigned seed) {
    char *text = test_read_file(COMPOSER_FIXTURE_DIR "/theme/one-motive.json", NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request);
    cJSON_ReplaceItemInObjectCaseSensitive(request, "key", cJSON_CreateString("C"));
    cJSON_ReplaceItemInObjectCaseSensitive(request, "mode", cJSON_CreateString("major"));
    cJSON_ReplaceItemInObjectCaseSensitive(request, "meter", cJSON_CreateString(meter));
    cJSON_ReplaceItemInObjectCaseSensitive(request, "into", cJSON_CreateString(into));
    cJSON *motive = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(request, "motives"), 0);
    cJSON_ReplaceItemInObjectCaseSensitive(motive, "degrees", cJSON_CreateString(degrees));
    assert(cJSON_AddNumberToObject(request, "seed", seed));
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    assert(printed);
    return printed;
}

static void read_idea(const cJSON *melody, Idea *idea) {
    const cJSON *span = cJSON_GetObjectItemCaseSensitive(melody, "span");
    idea->start = time_of(cJSON_GetObjectItemCaseSensitive(span, "start"));
    idea->end = time_of(cJSON_GetObjectItemCaseSensitive(span, "end"));
    const cJSON *notes = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(melody, "composer"), "payload"), "notes");
    idea->note_count = 0;
    const cJSON *note;
    cJSON_ArrayForEach(note, notes) {
        assert(idea->note_count < 32);
        idea->duration[idea->note_count] = time_of(cJSON_GetObjectItemCaseSensitive(note, "duration"));
        idea->pitch[idea->note_count] = cJSON_GetObjectItemCaseSensitive(note, "midi_pitch")->valueint;
        idea->note_count++;
    }
}

static bool same_notes(const Idea *a, const Idea *b) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (a->pitch[i] != b->pitch[i] || !time_equal(a->duration[i], b->duration[i])) {
            return false;
        }
    }
    return true;
}

/* Grows one theme; the realized melody that starts the theme is its basic idea, the one that ends it its cadential idea. */
static void grow(const char *degrees, const char *meter, const char *into, unsigned seed, Idea *basic, Idea *cadential) {
    char *request = request_of(degrees, meter, into, seed);
    ComposerResponse response;
    ComposerStatus status = composer_api_theme(request, NULL, &response);
    if (status != COMPOSER_OK) {
        fprintf(stderr, "%s %s seed %u: %s: %s\n", into, meter, seed, composer_status_name(status), response.error.message);
    }
    assert(status == COMPOSER_OK);
    free(request);
    assert(response.plan_json);
    cJSON *plan = cJSON_Parse(response.plan_json);
    assert(plan);
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(plan, "form");
    const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(form, "children"), 0);
    Time theme_end = time_of(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(theme, "span"), "end"));
    bool found_basic = false;
    bool found_cadential = false;
    const cJSON *melody;
    cJSON_ArrayForEach(melody, cJSON_GetObjectItemCaseSensitive(plan, "melody")) {
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(melody, "composer");
        const cJSON *state = cJSON_GetObjectItemCaseSensitive(composer, "state");
        if (!cJSON_IsString(state) || strcmp(state->valuestring, "realized") != 0) {
            continue;
        }
        Idea idea;
        read_idea(melody, &idea);
        Time zero = {0, 1};
        if (time_equal(idea.start, zero) && !found_basic) {
            *basic = idea;
            found_basic = true;
        }
        if (time_equal(idea.end, theme_end) && time_less(zero, idea.start) &&
            (!found_cadential || time_less(cadential->start, idea.start))) {
            *cadential = idea;
            found_cadential = true;
        }
    }
    assert(found_basic && found_cadential);
    cJSON_Delete(plan);
    composer_response_destroy(&response);
}

static size_t count_of(const Idea *idea, long num, long den) {
    size_t count = 0;
    Time value = {num, den};
    for (size_t i = 0; i < idea->note_count; i++) {
        count += time_equal(idea->duration[i], value) ? 1 : 0;
    }
    return count;
}

/* The themes of a motive: OK for every seed and form, returns how many have a cadential idea that is not the basic idea's. */
static size_t grow_all(const char *degrees, const char *meter, size_t *kept_minimum_out, long num, long den, size_t kept) {
    size_t differ = 0;
    size_t kept_minimum = (size_t)-1;
    static const char *const INTO[] = {"sentence", "period"};
    for (size_t f = 0; f < forms; f++) {
        for (unsigned seed = 1; seed <= seeds; seed++) {
            Idea basic;
            Idea cadential;
            grow(degrees, meter, INTO[f], seed, &basic, &cadential);
            size_t have = count_of(&basic, num, den);
            if (have < kept_minimum) {
                kept_minimum = have;
            }
            if (have < kept) {
                fprintf(stderr, "%s %s seed %u: the basic idea has %zu notes of %ld/%ld, the motive has %zu\n", INTO[f], meter, seed,
                        have, num, den, kept);
            }
            assert(have >= kept);
            differ += same_notes(&basic, &cadential) ? 0 : 1;
        }
    }
    *kept_minimum_out = kept_minimum;
    return differ;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "quick") == 0) {
        seeds = 1;
        forms = 1;
    }
    size_t themes = forms * seeds;
    size_t kept;
    /* Eighth triplets: the motive has six 1/12 notes; the basic idea keeps at least three of them. */
    size_t differ = grow_all("1:e3 2:e3 3:e3 5:q 3:e3 2:e3 1:e3 3:q 5:h 1':h", "4/4", &kept, 1, 12, 3);
    fprintf(stderr, "triplets: %zu of %zu themes have a cadential idea unlike the basic idea\n", differ, themes);
    assert(differ >= (themes + 1) / 2);

    /* Sixteenths. */
    differ = grow_all("1:s 2:s 3:s 4:s 5:q 3:q 1:h", "4/4", &kept, 1, 16, 4);
    fprintf(stderr, "sixteenths: %zu of %zu themes have a cadential idea unlike the basic idea\n", differ, themes);
    assert(differ >= (themes + 1) / 2);

    /* A 6/8 figure of a dotted eighth, a sixteenth and an eighth: the presentation keeps it. */
    for (size_t f = 0; f < forms; f++) {
        static const char *const INTO[] = {"sentence", "period"};
        for (unsigned seed = 1; seed <= seeds; seed++) {
            Idea basic;
            Idea cadential;
            grow("5:e. 6:s 5:e 1':q. 7:e. 1':s 2':e 1':q.", "6/8", INTO[f], seed, &basic, &cadential);
            Time dotted = {3, 16};
            Time sixteenth = {1, 16};
            Time eighth = {1, 8};
            assert(basic.note_count >= 3);
            assert(time_equal(basic.duration[0], dotted) && time_equal(basic.duration[1], sixteenth) &&
                   time_equal(basic.duration[2], eighth));
        }
    }
    printf("test_grow_triplets: ok\n");
    return 0;
}
