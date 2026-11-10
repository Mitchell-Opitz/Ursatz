#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_random.h>

#include "api/api_grow.h"
#include "grow/grow_internal.h"
#include "motive/motive_internal.h"
#include "support/test_support.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The motif and development preferences (B2 of P43, R-411). The segments generator with no preferences and with the
 * defaults gives the same motives for seeds 1 to 16 in 4/4 and 3/4; a bars setting of {0, 1} gives two-bar motives only; a
 * features setting of {1, 0, 0} gives a dotted figure in segment a of every motive; a pickup_one_in of 0 gives no pickup.
 * The fidelity table: the minimum a theme's Development list shows is the table's, 1/1 for the basic idea when the
 * presentation minimum is 1/1 (the themes made are counted in the comment of raised_minimum_is_shown).
 */

#define SEEDS 16
#define THEME_SEEDS 8

static MotiveRandomOptions options_of(int beats, unsigned long seed, const MotiveRandomPreferences *preferences) {
    MotiveRandomOptions options;
    memset(&options, 0, sizeof(options));
    options.mode = KEY_STRUCTURE_MODE_MAJOR;
    options.meter.beats_per_bar = beats;
    options.meter.beat_unit = 4;
    options.form = MOTIVE_SKETCH_FORM_FULL;
    options.strategy = MOTIVE_RANDOM_STRATEGY_SEGMENTS;
    options.count = 1;
    options.seed = seed;
    options.preferences = preferences;
    return options;
}

static Motive make(int beats, unsigned long seed, const MotiveRandomPreferences *preferences) {
    MotiveRandomOptions options = options_of(beats, seed, preferences);
    Motive motive;
    size_t count = 0;
    assert(motive_random(&options, &motive, &count) == MOTIVE_RANDOM_OK && count == 1);
    return motive;
}

static bool same_motive(const Motive *a, const Motive *b) {
    if (a->note_count != b->note_count || a->feature_count != b->feature_count || a->chord_count != b->chord_count ||
        rational_cmp(motive_pickup(a), motive_pickup(b)) != 0 || rational_cmp(a->length, b->length) != 0) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (a->notes[i].pitch != b->notes[i].pitch || rational_cmp(a->notes[i].onset, b->notes[i].onset) != 0 ||
            rational_cmp(a->notes[i].duration, b->notes[i].duration) != 0) {
            return false;
        }
    }
    for (size_t i = 0; i < a->feature_count; i++) {
        if (a->features[i].first != b->features[i].first || a->features[i].last != b->features[i].last ||
            strcmp(a->features[i].label, b->features[i].label) != 0) {
            return false;
        }
    }
    for (size_t i = 0; i < a->chord_count; i++) {
        if (a->chords[i].degree != b->chords[i].degree) {
            return false;
        }
    }
    return true;
}

static void defaults_change_nothing(void) {
    MotiveRandomPreferences defaults;
    motive_random_preferences_default(&defaults);
    for (int beats = 3; beats <= 4; beats++) {
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            Motive without = make(beats, seed, NULL);
            Motive with = make(beats, seed, &defaults);
            assert(same_motive(&without, &with));
        }
    }
}

/* Bars in whole notes: the motive's length less its pickup, over the bar. */
static bool two_bars(const Motive *motive, int beats) {
    Rational body;
    Rational bar;
    assert(rational_make(beats, 4, &bar) == RATIONAL_OK);
    assert(rational_sub(motive->length, motive_pickup(motive), &body) == RATIONAL_OK);
    Rational two;
    assert(rational_make(2 * beats, 4, &two) == RATIONAL_OK);
    return rational_cmp(body, two) == 0;
}

static void two_bars_only(void) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.bars[0] = 0;
    preferences.bars[1] = 1;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, &preferences);
        assert(two_bars(&motive, 4));
    }
}

/* A dotted figure in segment a: a dotted quarter (3/8) and the eighth that completes the beat (1/8). */
static bool dotted_in_a(const Motive *motive) {
    size_t first = motive->features[0].first;
    size_t last = motive->features[0].last;
    Rational dotted;
    Rational eighth;
    assert(rational_make(3, 8, &dotted) == RATIONAL_OK && rational_make(1, 8, &eighth) == RATIONAL_OK);
    for (size_t i = first; i < last; i++) {
        if (rational_cmp(motive->notes[i].duration, dotted) == 0 && rational_cmp(motive->notes[i + 1].duration, eighth) == 0) {
            return true;
        }
    }
    return false;
}

static void dotted_features_only(void) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.features[0] = 1;
    preferences.features[1] = 0;
    preferences.features[2] = 0;
    for (int beats = 3; beats <= 4; beats++) {
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            Motive motive = make(beats, seed, &preferences);
            assert(dotted_in_a(&motive));
        }
    }
}

static void no_pickup(void) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.pickup_one_in = 0;
    size_t pickups_by_default = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, &preferences);
        assert(motive_pickup(&motive).num == 0);
        Motive drawn = make(4, seed, NULL);
        pickups_by_default += motive_pickup(&drawn).num != 0;
    }
    /* The default does draw pickups in these seeds, so the setting is what removed them. */
    assert(pickups_by_default > 0);
}

/* The brief of the Test Motif sentence for a seed, under a fidelity table (NULL for the default). */
static void sentence_setup(GrowSetup *setup, GrowBriefText *text, const Motive *motive, const GrowFidelityTable *table,
                           unsigned long seed) {
    memset(text, 0, sizeof(*text));
    text->key = "C major";
    text->meter = "6/8";
    text->cadence = "PAC";
    text->into = "hybrid";
    memset(setup, 0, sizeof(*setup));
    setup->text = text;
    setup->into = GROW_INTO_HYBRID;
    setup->pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
    setup->pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    setup->key = motive_key(motive);
    setup->meter = motive_meter(motive);
    setup->bars = 8;
    setup->cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    setup->first_cadence = CADENCE_TYPE_HALF;
    setup->motive = motive;
    setup->suggest = 1;
    setup->seed = seed;
    setup->fidelity = table;
    setup->tempo_from = "none";
}

/* Whether the theme was made; with shown not NULL, the largest minimum its Development list shows. */
static bool theme_made(const Motive *motive, const GrowFidelityTable *table, unsigned long seed, double *shown) {
    GrowBriefText text;
    GrowSetup setup;
    sentence_setup(&setup, &text, motive, table, seed);
    GrowRun run;
    ComposerError error;
    memset(&error, 0, sizeof(error));
    bool made = api_grow_run(&setup, &run, &error) == COMPOSER_OK && run.made == 1;
    if (made && shown) {
        cJSON *document = api_grow_result_json(&setup, &run, NULL);
        assert(document);
        const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
        const cJSON *entry;
        *shown = 0;
        cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(theme, "developments")) {
            const cJSON *minimum = cJSON_GetObjectItemCaseSensitive(entry, "minimum");
            if (cJSON_IsNumber(minimum) && minimum->valuedouble > *shown) {
                *shown = minimum->valuedouble;
            }
        }
        cJSON_Delete(document);
    }
    if (made) {
        api_grow_run_destroy(&run);
    }
    return made;
}

/*
 * A table whose presentation minimum is 1/1 for every development setting. Measured at the commit of this test, for the
 * Test Motif sentence and seeds 1 to 8: 8 of 8 seeds make a theme with the default table and 8 of 8 with this one (the
 * melody stage still finds a candidate for each, so the table does not refuse more of them). The counts are equal, so the
 * test asserts what the table changes instead: the minimum the theme's Development list shows is 1/1 for the basic idea
 * with this table and never 1/1 with the default table, whose largest minimum is 11/20.
 */
static void raised_minimum_is_shown(void) {
    char *text = test_read_file(COMPOSER_FIXTURE_DIR "/motifs/test-motif-6-8.json", NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(text && motive_from_json(text, NULL, &motive, &error) == MOTIVE_OK);
    free(text);
    GrowFidelityTable raised;
    grow_fidelity_table_default(&raised);
    for (size_t d = 0; d < GROW_DEVELOP_COUNT; d++) {
        raised.minimum[d][GROW_ROLE_PRESENTATION].numerator = 1;
        raised.minimum[d][GROW_ROLE_PRESENTATION].denominator = 1;
    }
    size_t made_default = 0;
    size_t made_raised = 0;
    for (unsigned long seed = 1; seed <= THEME_SEEDS; seed++) {
        double shown_default = 0;
        double shown_raised = 0;
        bool a = theme_made(&motive, NULL, seed, &shown_default);
        bool b = theme_made(&motive, &raised, seed, &shown_raised);
        made_default += a;
        made_raised += b;
        assert(!a || (shown_default > 0.5 && shown_default < 0.56));
        assert(!b || shown_raised == 1.0);
    }
    printf("default table: %zu of %d seeds made a theme; presentation 1/1: %zu\n", made_default, THEME_SEEDS, made_raised);
    assert(made_default > 0 && made_raised <= made_default);
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    defaults_change_nothing();
    two_bars_only();
    dotted_features_only();
    no_pickup();
    raised_minimum_is_shown();
    printf("test_motif_preferences: ok\n");
    return 0;
}
