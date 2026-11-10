#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_develop.h>
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
    /* the nine listening settings (R-420 to R-422) set explicitly to their defaults change nothing either */
    MotiveRandomPreferences explicit_defaults = defaults;
    explicit_defaults.accented_one_in = 0;
    explicit_defaults.accented_types[0] = 1;
    explicit_defaults.accented_types[1] = 1;
    explicit_defaults.accented_types[2] = 1;
    explicit_defaults.avoid_repeats = false;
    explicit_defaults.leaps_per_motif = 11;
    explicit_defaults.leap_recovery = false;
    explicit_defaults.pickup_from_accent = false;
    explicit_defaults.contrast_fidelity_min.numerator = 0;
    explicit_defaults.contrast_fidelity_min.denominator = 1;
    explicit_defaults.long_on_weak = 0;
    explicit_defaults.final_long = false;
    for (int beats = 3; beats <= 4; beats++) {
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            Motive without = make(beats, seed, NULL);
            Motive with = make(beats, seed, &explicit_defaults);
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

/* ------------------------------------------------- the listening settings (phase L1) */

/* The start of note i in half-beats of 4/4, counted from the first bar line (the pickup, if any, comes before it). */
static long half_beats_from_bar(const Motive *motive, size_t i) {
    Rational from_bar;
    Rational half_beats;
    assert(rational_sub(motive->notes[i].onset, motive_pickup(motive), &from_bar) == RATIONAL_OK);
    assert(rational_mul_int(from_bar, 8, &half_beats) == RATIONAL_OK);
    long long whole;
    assert(rational_floor(half_beats, &whole) == RATIONAL_OK);
    return (long)whole;
}

static bool starts_on_beat(const Motive *motive, size_t i) {
    return half_beats_from_bar(motive, i) % 2 == 0;
}

static bool has_pickup(const Motive *motive) {
    return !rational_is_zero(motive_pickup(motive));
}

static int interval(const Motive *motive, size_t k) {
    return motive->notes[k + 1].pitch - motive->notes[k].pitch;
}

static void accented_tones(void) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.accented_one_in = 1;
    int with_accent = 0;
    int counts[2] = {0, 0}; /* [0] on-beat non-chord tones, [1] of them suspensions */
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, &preferences);
        MotiveAnalysis analysis;
        assert(motive_analyze(&motive, &analysis) && analysis.tones_known);
        bool found = false;
        for (size_t i = 0; i < motive.note_count; i++) {
            if (analysis.tones[i].role == MOTIVE_TONE_NON_CHORD && starts_on_beat(&motive, i)) {
                assert(analysis.tones[i].resolves_to == (long)i + 1);
                found = true;
                counts[0]++;
                counts[1] += analysis.tones[i].type == NON_CHORD_TONE_SUSPENSION;
            }
        }
        with_accent += found;
    }
    printf("accented_one_in 1: %d of %d motives have an on-beat non-chord tone (%d, %d suspensions)\n", with_accent, SEEDS, counts[0], counts[1]);
    assert(with_accent >= 1);

    preferences.accented_types[0] = 0;
    preferences.accented_types[1] = 0;
    preferences.accented_types[2] = 1;
    int suspensions = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, &preferences);
        MotiveAnalysis analysis;
        assert(motive_analyze(&motive, &analysis) && analysis.tones_known);
        for (size_t i = 0; i < motive.note_count; i++) {
            if (analysis.tones[i].role == MOTIVE_TONE_NON_CHORD && starts_on_beat(&motive, i)) {
                assert(analysis.tones[i].type == NON_CHORD_TONE_SUSPENSION);
                suspensions++;
            }
        }
    }
    printf("accented_types {0, 0, 1}: %d on-beat non-chord tones, all suspensions\n", suspensions);
}

static void no_repeated_notes(void) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.features[0] = 1;
    preferences.features[1] = 1;
    preferences.features[2] = 0;
    int without = 0;
    int with = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        preferences.avoid_repeats = false;
        Motive plain = make(4, seed, &preferences);
        for (size_t k = 0; k + 1 < plain.note_count; k++) {
            without += interval(&plain, k) == 0;
        }
        preferences.avoid_repeats = true;
        Motive motive = make(4, seed, &preferences);
        for (size_t k = 0; k + 1 < motive.note_count; k++) {
            with += interval(&motive, k) == 0;
        }
    }
    printf("avoid_repeats: repeated notes %d without, %d with\n", without, with);
    assert(with == 0);
}

static void fewer_leaps(void) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.leaps_per_motif = 1;
    int leaps = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, &preferences);
        int count = 0;
        for (size_t k = 0; k + 1 < motive.note_count; k++) {
            count += abs(interval(&motive, k)) >= 5;
        }
        assert(count <= 1);
        leaps += count;
    }
    printf("leaps_per_motif 1: %d leaps in %d motives\n", leaps, SEEDS);

    preferences.leap_recovery = true;
    int recovered = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, &preferences);
        for (size_t k = 0; k + 2 < motive.note_count; k++) {
            int move = interval(&motive, k);
            if (abs(move) >= 5) {
                int back = interval(&motive, k + 1);
                assert(abs(back) >= 1 && abs(back) <= 2 && (back > 0) != (move > 0));
                recovered++;
            }
        }
    }
    printf("leap_recovery: %d leaps, each followed by a step back\n", recovered);
}

/* A motive that opens as one with a pickup does: a note of at most a beat, then a longer one on the root, higher. */
static bool opens_with_implied_upbeat(const Motive *motive) {
    Rational beat;
    assert(rational_make(1, 4, &beat) == RATIONAL_OK);
    return !has_pickup(motive) && rational_cmp(motive->notes[0].duration, beat) <= 0 &&
           rational_cmp(motive->notes[1].duration, motive->notes[0].duration) > 0 && motive->notes[1].pitch % 12 == 0 &&
           motive->notes[1].pitch > motive->notes[0].pitch;
}

static void pickups_from_the_accent(void) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.pickup_from_accent = true;
    preferences.pickup_one_in = 1;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, &preferences);
        MotiveAnalysis analysis;
        assert(has_pickup(&motive) && motive_analyze(&motive, &analysis) && analysis.tones_known);
        /* one chord tone below (a third or a fourth) and no longer than the downbeat note */
        int rise = interval(&motive, 0);
        assert(rise >= 3 && rise <= 5);
        assert(rational_cmp(motive.notes[0].duration, motive.notes[1].duration) <= 0);
        assert(analysis.tones[0].role == MOTIVE_TONE_CHORD);
    }
    printf("pickup_from_accent: %d pickups, each a chord tone below the downbeat note\n", SEEDS);

    preferences.pickup_one_in = 0;
    int implied_off = 0;
    int implied_on = 0;
    for (unsigned long seed = 1; seed <= 200; seed++) {
        preferences.pickup_from_accent = false;
        Motive plain = make(4, seed, &preferences);
        implied_off += opens_with_implied_upbeat(&plain);
        preferences.pickup_from_accent = true;
        Motive motive = make(4, seed, &preferences);
        implied_on += opens_with_implied_upbeat(&motive);
    }
    printf("pickup_from_accent, no pickup: %d motives of 200 open with an implied upbeat without it, %d with\n", implied_off, implied_on);
    assert(implied_on == 0);
}

/* The notes first to last of a motive as a motive of their own (no pickup, no chords or segments). */
static Motive notes_of(const Motive *motive, size_t first, size_t last) {
    Motive out = *motive;
    Rational base = motive->notes[first].onset;
    size_t count = 0;
    for (size_t i = first; i <= last; i++, count++) {
        out.notes[count] = motive->notes[i];
        assert(rational_sub(motive->notes[i].onset, base, &out.notes[count].onset) == RATIONAL_OK);
    }
    out.note_count = count;
    out.pickup = rational_ratio(0, 1);
    out.chord_count = 0;
    out.feature_count = 0;
    Rational end;
    assert(rational_add(out.notes[count - 1].onset, out.notes[count - 1].duration, &end) == RATIONAL_OK);
    out.length = end;
    return out;
}

static void close_contrast(void) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.shapes_one_bar[0] = 0;
    preferences.shapes_one_bar[1] = 1;
    preferences.shapes_two_bars[0] = 0;
    preferences.shapes_two_bars[1] = 1;
    preferences.shapes_two_bars[2] = 0;
    double lowest_off = 1.0;
    double lowest_on = 1.0;
    for (int on = 0; on < 2; on++) {
        preferences.contrast_fidelity_min.numerator = on ? 1 : 0;
        preferences.contrast_fidelity_min.denominator = on ? 2 : 1;
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            Motive motive = make(4, seed, &preferences);
            assert(motive.feature_count == 2);
            size_t a_first = motive.features[0].first + (has_pickup(&motive) ? 1 : 0); /* segment a's cell, not the pickup */
            Motive a = notes_of(&motive, a_first, motive.features[0].last);
            Motive b = notes_of(&motive, motive.features[1].first, motive.features[1].last);
            MotiveRecognizability score;
            assert(motive_recognizability(&a, &b, NULL, &score));
            double total = (double)score.total.numerator / (double)score.total.denominator;
            if (on) {
                assert(score.total.numerator * 2 >= score.total.denominator);
                lowest_on = total < lowest_on ? total : lowest_on;
            } else {
                lowest_off = total < lowest_off ? total : lowest_off;
            }
        }
    }
    printf("contrast_fidelity_min 1/2: lowest score of segment b against a %.3f (%.3f without)\n", lowest_on, lowest_off);
}

static int two_beat_notes_on_weak_beats(const MotiveRandomPreferences *preferences) {
    Rational two_beats;
    assert(rational_make(1, 2, &two_beats) == RATIONAL_OK);
    int count = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, preferences);
        for (size_t i = has_pickup(&motive) ? 1 : 0; i < motive.note_count; i++) {
            long half_beats = half_beats_from_bar(&motive, i);
            count += half_beats % 2 == 0 && half_beats % 4 != 0 && rational_cmp(motive.notes[i].duration, two_beats) == 0;
        }
    }
    return count;
}

static void long_notes(void) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    int plain = two_beat_notes_on_weak_beats(&preferences);
    preferences.long_on_weak = 20;
    int favored = two_beat_notes_on_weak_beats(&preferences);
    printf("long_on_weak 20: %d two-beat notes on weak beats (%d with the defaults)\n", favored, plain);
    assert(favored > plain);

    preferences.long_on_weak = 0;
    preferences.final_long = true;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, &preferences);
        size_t last = motive.note_count - 1;
        for (size_t i = motive.features[motive.feature_count - 1].first; i < last; i++) {
            assert(rational_cmp(motive.notes[i].duration, motive.notes[last].duration) < 0);
        }
    }
    printf("final_long: the last note is longer than every other note of the last segment in %d motives\n", SEEDS);
}

/* Each setting alone: how many of the seeds 1 to 16 still give a motive. */
static int motives_made(const MotiveRandomPreferences *preferences) {
    int made = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        MotiveRandomOptions options = options_of(4, seed, preferences);
        Motive motive;
        size_t count = 0;
        made += motive_random(&options, &motive, &count) == MOTIVE_RANDOM_OK;
    }
    return made;
}

static void each_setting_still_gives_motives(void) {
    MotiveRandomPreferences base;
    motive_random_preferences_default(&base);
    MotiveRandomPreferences accented = base;
    accented.accented_one_in = 1;
    MotiveRandomPreferences repeats = base;
    repeats.avoid_repeats = true;
    MotiveRandomPreferences leaps = base;
    leaps.leaps_per_motif = 1;
    MotiveRandomPreferences recovery = base;
    recovery.leap_recovery = true;
    MotiveRandomPreferences pickup = base;
    pickup.pickup_from_accent = true;
    MotiveRandomPreferences contrast = base;
    contrast.contrast_fidelity_min.numerator = 1;
    contrast.contrast_fidelity_min.denominator = 2;
    MotiveRandomPreferences weak = base;
    weak.long_on_weak = 20;
    MotiveRandomPreferences final_long = base;
    final_long.final_long = true;
    const struct {
        const char *name;
        const MotiveRandomPreferences *preferences;
    } settings[] = {{"accented_one_in 1", &accented}, {"avoid_repeats", &repeats},  {"leaps_per_motif 1", &leaps},
                    {"leap_recovery", &recovery},     {"pickup_from_accent", &pickup}, {"contrast_fidelity_min 1/2", &contrast},
                    {"long_on_weak 20", &weak},       {"final_long", &final_long}};
    for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
        int made = motives_made(settings[i].preferences);
        printf("%s alone: %d of %d seeds give a motive\n", settings[i].name, made, SEEDS);
        assert(made >= 12);
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    defaults_change_nothing();
    two_bars_only();
    dotted_features_only();
    no_pickup();
    raised_minimum_is_shown();
    accented_tones();
    no_repeated_notes();
    fewer_leaps();
    pickups_from_the_accent();
    close_contrast();
    long_notes();
    each_setting_still_gives_motives();
    printf("test_motif_preferences: ok\n");
    return 0;
}
