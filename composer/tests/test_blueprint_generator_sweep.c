#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <analysis/similarity/similarity_scoring.h>
#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>

#include "grow/grow_internal.h"
#include "motive/motive_internal.h"
#include "support/test_blueprint_check.h"
#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Gate 2 of P69c (R-757): the four motif fixtures (continuation/s77-chords, continuation/u51-chords, rates/two-bar-3-4 and motifs/motive-d-minor-arpeggio)
 * as the theme request `listen` sends for --motif (key C, the fixture's mode, its meter, the motif's degrees, pickup, harmony and features in the basic
 * slot), as a sentence and as a period (with first_cadence HC), seeds 1 to 10, no development, through composer_api_theme. Each case's composed
 * count (the themes the Composer wrote as a blueprint) is pinned below, at least 8, and may only rise. Of each theme that composes, from its plan's
 * melody and its echoed blueprint: every fixed item of the blueprint holds (test_blueprint_check_fixed); the melody spans at most the larger of 16 and the
 * climax's distance above the motif's lowest note; the climax, the theme's highest note, is in bar 5 or 6 of a sentence and bar 6 of a period; no half
 * note follows a half note after the second bar; a sentence's bar 4 has a rhythm of its own, not bar 0's; a period's bar 2, when the generator drew it
 * as a contrast, is at most 1/2 as similar to bar 0 as similarity_rhythm_score measures. With `fast`, seed 1 of the u51 and two-bar-3-4 cases (the smoke test);
 * with a case's name ("u51:period") that case alone; the environment variable SWEEP_SEED limits a run to one seed.
 */

#define SEEDS 10
#define FIXTURES 4

typedef struct {
    const char *name;
    const char *file;
    int composed[2]; /* of seeds 1 to 10 as a sentence and as a period, measured: the test fails below it */
} Fixture;

static const Fixture FIXTURE[FIXTURES] = {
    {"s77", "/continuation/s77-chords.json", {10, 10}},
    {"u51", "/continuation/u51-chords.json", {10, 10}},
    {"two-bar-3-4", "/rates/two-bar-3-4.json", {10, 10}},
    {"d-minor-arpeggio", "/motifs/motive-d-minor-arpeggio.json", {10, 10}},
};

static bool fast_run = false;
static Motive MOTIVE_OF[FIXTURES];
static char MODE_OF[FIXTURES][8];

/* The motif of a fixture in key C and its mode, as `listen --motif` reads it. */
static Motive load_motive(const char *path, char *mode, size_t mode_size) {
    char *text = test_must_read_file(path, NULL);
    Motive motive;
    Motive in_c;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(motive_from_json(text, test_motive_options(), &motive, &error) == MOTIVE_OK);
    free(text);
    SpelledKey source = motive_key(&motive);
    snprintf(mode, mode_size, "%s", pitch_material_third(source.material) == 3 ? "minor" : "major");
    SpelledKey key = test_key(0, mode);
    assert(motive_in_key(&motive, &key, &in_c));
    return in_c;
}

/* A rational of whole notes as "n" or "n/d" beats of the meter's beat unit (the pickup a request writes). */
static void beats_text(Rational whole, int beat_unit, char *out, size_t size) {
    Rational beats = whole;
    assert(rational_mul_int(whole, beat_unit, &beats) == RATIONAL_OK);
    if (beats.den == 1) {
        snprintf(out, size, "%lld", beats.num);
    } else {
        snprintf(out, size, "%lld/%lld", beats.num, beats.den);
    }
}

/* The theme request `listen` sends for the motif: key C, the mode, the meter, the motif as M1 in the basic slot; no development. */
static cJSON *request_of(size_t which, bool period, unsigned seed) {
    const Motive *motive = &MOTIVE_OF[which];
    PlanMeter meter = motive_meter(motive);
    char meter_text[16];
    char pickup[32];
    char seed_text[16];
    snprintf(meter_text, sizeof meter_text, "%d/%d", meter.beats_per_bar, meter.beat_unit);
    snprintf(seed_text, sizeof seed_text, "%u", seed);
    beats_text(motive_pickup(motive), meter.beat_unit, pickup, sizeof pickup);
    char *degrees = motive_to_degrees_text(motive);
    char *harmony = motive_harmony_to_text(motive);
    assert(degrees);
    cJSON *request = cJSON_CreateObject();
    cJSON *list = cJSON_AddArrayToObject(request, "motives");
    cJSON *entry = cJSON_CreateObject();
    cJSON *features = cJSON_CreateArray();
    cJSON *slots = cJSON_CreateObject();
    assert(request && list && entry && features && slots);
    for (size_t f = 0; f < motive->feature_count; f++) {
        cJSON *feature = cJSON_CreateObject();
        assert(feature && cJSON_AddStringToObject(feature, "label", motive->features[f].label) &&
               cJSON_AddNumberToObject(feature, "first", (double)motive->features[f].first) &&
               cJSON_AddNumberToObject(feature, "last", (double)motive->features[f].last) && cJSON_AddItemToArray(features, feature));
    }
    assert(cJSON_AddItemToArray(list, entry) && cJSON_AddItemToObject(request, "slots", slots));
    assert(cJSON_AddStringToObject(request, "schema", "ursatz-theme-request") && cJSON_AddNumberToObject(request, "schema_version", 2) &&
           cJSON_AddStringToObject(request, "into", period ? "period" : "sentence") && cJSON_AddStringToObject(request, "key", "C") &&
           cJSON_AddStringToObject(request, "material", MODE_OF[which]) && cJSON_AddStringToObject(request, "meter", meter_text) &&
           cJSON_AddNumberToObject(request, "bars", 8) && cJSON_AddStringToObject(request, "seed", seed_text) &&
           cJSON_AddStringToObject(entry, "id", "M1") && cJSON_AddStringToObject(entry, "form", "full") &&
           cJSON_AddStringToObject(entry, "degrees", degrees) && cJSON_AddStringToObject(entry, "pickup", pickup) &&
           (harmony && harmony[0] ? cJSON_AddStringToObject(entry, "harmony", harmony) != NULL : cJSON_AddNullToObject(entry, "harmony") != NULL) &&
           cJSON_AddItemToObject(entry, "features", features) && cJSON_AddStringToObject(entry, "role", "melodic") &&
           cJSON_AddStringToObject(slots, "basic", "M1") && (!period || cJSON_AddStringToObject(request, "first_cadence", "HC")) &&
           cJSON_AddBoolToObject(request, "plans", false));
    /* One candidate for each theme: ranking three (the default) makes a request three themes long, and the sweep reads what the generator composes. */
    cJSON *preferences = cJSON_AddObjectToObject(request, "preferences");
    cJSON *blueprint_group = preferences ? cJSON_AddObjectToObject(preferences, "blueprint") : NULL;
    assert(blueprint_group && cJSON_AddNumberToObject(blueprint_group, "candidates", 1));
    free(degrees);
    free(harmony);
    return request;
}

/* The rhythm of the notes that start in a bar of the theme: their offsets and durations in ticks, as an array of pairs. */
static size_t bar_rhythm(const Theme *theme, long bar, long out[][2], size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < theme->note_count && count < capacity; i++) {
        if (theme->notes[i].onset >= bar * theme->bar && theme->notes[i].onset < (bar + 1) * theme->bar) {
            out[count][0] = theme->notes[i].onset - bar * theme->bar;
            out[count++][1] = theme->notes[i].duration;
        }
    }
    return count;
}

static bool same_rhythm(const Theme *theme, long a, long b) {
    long left[64][2];
    long right[64][2];
    size_t n = bar_rhythm(theme, a, left, 64);
    size_t m = bar_rhythm(theme, b, right, 64);
    return n == m && memcmp(left, right, n * sizeof left[0]) == 0;
}

/* The rhythm similarity of two bars of the theme, similarity_rhythm_score on their durations (the score as 1 when they cannot be compared). */
static Rational similarity(const Theme *theme, long a, long b) {
    long left[64][2];
    long right[64][2];
    SimilarityDuration x[64];
    SimilarityDuration y[64];
    SimilarityScore score = {1, 1};
    size_t n = bar_rhythm(theme, a, left, 64);
    size_t m = bar_rhythm(theme, b, right, 64);
    for (size_t i = 0; i < n; i++) {
        x[i] = (Rational){left[i][1], 96};
    }
    for (size_t i = 0; i < m; i++) {
        y[i] = (Rational){right[i][1], 96};
    }
    if (n == 0 || m == 0 || similarity_rhythm_score(x, n, y, m, &score) != SIMILARITY_SCORE_OK) {
        return (Rational){1, 1};
    }
    return score;
}

/* Whether the generator drew bar 2 of the period as a contrast to bar 0, regenerated from the theme's seed as `grow_one` does. */
static bool period_bar_is_contrast(size_t which, unsigned long seed) {
    GrowRequest request;
    memset(&request, 0, sizeof request);
    request.motive = &MOTIVE_OF[which];
    request.key = motive_key(&MOTIVE_OF[which]);
    request.meter = motive_meter(&MOTIVE_OF[which]);
    request.bars = 8;
    request.pairing.beginning = SECTION_BEGINNING_ANTECEDENT;
    request.pairing.ending = SECTION_ENDING_CONSEQUENT;
    request.knowledge = test_knowledge();
    request.seed = seed;
    GrowBlueprint *blueprint = calloc(1, sizeof(*blueprint));
    assert(blueprint);
    FillRng rng;
    GrowRefusal refusal;
    memset(&refusal, 0, sizeof refusal);
    fill_rng_seed(&rng, seed + 11);
    bool contrast = grow_blueprint_generate(&request, NULL, &rng, blueprint, &refusal) == GROW_OK && blueprint->bars[2].job == GROW_JOB_CONTRAST;
    free(blueprint);
    return contrast;
}

/* Composes one case on one seed; true when it composed as a blueprint (and every check held). */
static bool compose(size_t which, bool period, unsigned seed) {
    cJSON *request = request_of(which, period, seed);
    char *printed = cJSON_PrintUnformatted(request);
    ComposerResponse response;
    UrsatzStatus status = composer_api_theme(test_context(), printed, NULL, &response);
    free(printed);
    char name[64];
    snprintf(name, sizeof name, "%s %s", FIXTURE[which].name, period ? "period" : "sentence");
    if (status != URSATZ_OK) {
        printf("%s seed %u: %s: %s\n", name, seed, test_status_name(status), response.error.message);
        assert(status == URSATZ_ERR_UNSATISFIABLE);
        composer_response_destroy(&response);
        cJSON_Delete(request);
        return false;
    }
    cJSON *document = cJSON_Parse(response.json);
    assert(document && response.plan_json);
    const cJSON *theme_json = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
    const cJSON *blueprint = cJSON_GetObjectItemCaseSensitive(theme_json, "blueprint");
    bool composed = cJSON_IsObject(blueprint);
    if (composed) {
        Where where = {name, seed};
        static Theme theme;
        assert(cJSON_AddItemToObject(request, "blueprint", cJSON_Duplicate(blueprint, true)));
        test_blueprint_check_fixed(&where, response.plan_json, request); /* (a) */
        test_blueprint_read_theme(response.plan_json, request, &theme);
        int low = 127;
        int high = 0;
        for (size_t i = 0; i < theme.note_count; i++) {
            low = theme.notes[i].pitch < low ? theme.notes[i].pitch : low;
            high = theme.notes[i].pitch > high ? theme.notes[i].pitch : high;
        }
        int motif_low = 127;
        for (size_t i = 0; i < MOTIVE_OF[which].note_count; i++) {
            int pitch = motive_pitch(&MOTIVE_OF[which], i);
            motif_low = pitch < motif_low ? pitch : motif_low;
        }
        const cJSON *climax = cJSON_GetObjectItemCaseSensitive(blueprint, "climax");
        long climax_bar = (long)cJSON_GetObjectItemCaseSensitive(climax, "bar")->valuedouble;
        int allowed = 16 > high - motif_low ? 16 : high - motif_low;
        if (high - low > allowed) { /* (b): the climax is the highest note */
            fprintf(stderr, "%s seed %u: the melody spans %d, over %d\n", name, seed, high - low, allowed);
            assert(false);
        }
        if (period ? climax_bar != 6 : (climax_bar != 5 && climax_bar != 6)) { /* (c) */
            fprintf(stderr, "%s seed %u: the climax is in bar %ld\n", name, seed, climax_bar);
            assert(false);
        }
        for (size_t i = 1; i < theme.note_count; i++) { /* (d) */
            bool halves = theme.notes[i].duration == 48 && theme.notes[i - 1].duration == 48;
            if (halves && theme.notes[i].onset >= 2 * theme.bar) {
                fprintf(stderr, "%s seed %u: a half note follows a half note at tick %ld\n", name, seed, theme.notes[i].onset);
                assert(false);
            }
        }
        if (!period && same_rhythm(&theme, 4, 0)) { /* (e) */
            fprintf(stderr, "%s seed %u: bar 4 has the rhythm of bar 0\n", name, seed);
            assert(false);
        }
        unsigned long theme_seed = (unsigned long)cJSON_GetObjectItemCaseSensitive(theme_json, "seed")->valuedouble;
        if (period && period_bar_is_contrast(which, theme_seed)) {
            Rational score = similarity(&theme, 2, 0);
            if (rational_cmp(score, (Rational){1, 2}) > 0) {
                fprintf(stderr, "%s seed %u: bar 2 is a contrast, %lld/%lld similar to bar 0\n", name, seed, score.num, score.den);
                assert(false);
            }
        }
    }
    cJSON_Delete(document);
    composer_response_destroy(&response);
    cJSON_Delete(request);
    return composed;
}

int main(int argc, char **argv) {
    fast_run = argc > 1 && strcmp(argv[1], "fast") == 0;
    /* A case's name as the argument ("u51:period") runs that one alone, to look at one while working on it. */
    const char *only = argc > 1 && !fast_run ? argv[1] : NULL;
    setvbuf(stdout, NULL, _IOLBF, 0);
    for (size_t which = 0; which < FIXTURES; which++) {
        char path[512];
        snprintf(path, sizeof path, "%s%s", COMPOSER_FIXTURE_DIR, FIXTURE[which].file);
        MOTIVE_OF[which] = load_motive(path, MODE_OF[which], sizeof MODE_OF[which]);
    }
    unsigned last = fast_run ? 1 : SEEDS;
    const char *seed_only = getenv("SWEEP_SEED");
    unsigned first = seed_only ? (unsigned)strtoul(seed_only, NULL, 10) : 1;
    last = seed_only ? first : last;
    for (size_t which = 0; which < FIXTURES; which++) {
        for (int period = 0; period < 2; period++) {
            char case_name[64];
            snprintf(case_name, sizeof case_name, "%s:%s", FIXTURE[which].name, period ? "period" : "sentence");
            if (only && strcmp(only, case_name) != 0) {
                continue;
            }
            if (fast_run && which != 1 && which != 2) {
                continue; /* the smoke test: seed 1 of u51 and two-bar-3-4 (a seed that does not compose costs its melody stage: s77 and the arpeggio take half a minute) */
            }
            unsigned composed = 0;
            for (unsigned seed = first; seed <= last; seed++) {
                composed += compose(which, period != 0, seed);
            }
            printf("%s %s: %u of %u composed (pinned %d)\n", FIXTURE[which].name, period ? "period" : "sentence", composed, last,
                   FIXTURE[which].composed[period]);
            if (!fast_run && !only) {
                assert(FIXTURE[which].composed[period] >= 8);
                assert((int)composed >= FIXTURE[which].composed[period]);
            } else if (fast_run) {
                assert(composed == 1);
            }
        }
    }
    printf("test_blueprint_generator_sweep%s: ok\n", fast_run ? " fast" : "");
    return 0;
}
