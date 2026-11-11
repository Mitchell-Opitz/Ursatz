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
    /* the rhythm settings of phase L2 (R-439) too */
    for (size_t i = 3; i < 8; i++) {
        explicit_defaults.durations[i] = 0;
    }
    explicit_defaults.compound_beats = false;
    /* and the endings and long non-chord tones of phase L2 (R-440, R-441) */
    explicit_defaults.endings[0] = 1;
    explicit_defaults.endings[1] = 0;
    explicit_defaults.endings[2] = 0;
    explicit_defaults.endings[3] = 0;
    explicit_defaults.long_nct_one_in = 0;
    for (size_t i = 0; i < 5; i++) {
        explicit_defaults.long_nct_types[i] = 1;
    }
    for (size_t i = 0; i < 4; i++) {
        explicit_defaults.long_nct_intervals[i] = 1;
    }
    for (size_t i = 0; i < 3; i++) {
        explicit_defaults.long_nct_beats[i] = 1;
        explicit_defaults.long_nct_lengths[i] = 1;
    }
    /* and the syncopation of phase L3 (R-454) */
    explicit_defaults.syncopation_one_in = 0;
    for (size_t i = 0; i < 4; i++) {
        explicit_defaults.syncopation_types[i] = 1;
    }
    for (size_t i = 0; i < 2; i++) {
        explicit_defaults.syncopation_levels[i] = 1;
    }
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

/* Each setting alone: how many of the seeds 1 to 16 still give a motive, in a meter of this many beats over this unit. */
static int motives_made_in(int beats, int unit, const MotiveRandomPreferences *preferences) {
    int made = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        MotiveRandomOptions options = options_of(beats, seed, preferences);
        options.meter.beat_unit = unit;
        Motive motive;
        size_t count = 0;
        made += motive_random(&options, &motive, &count) == MOTIVE_RANDOM_OK;
    }
    return made;
}

/* ------------------------------------------------- the rhythm settings (phase L2, R-439) */

#define GRID 48 /* ticks of 1/48 whole: a quarter beat of 4/4 is 3, a beat 12, a third of a beat 4 */

/* Note i as whole-note forty-eighths from the first bar line (the pickup, if any, comes before it): its start and its length. */
static long grid_start(const Motive *motive, size_t i) {
    Rational from_bar;
    Rational scaled;
    assert(rational_sub(motive->notes[i].onset, motive_pickup(motive), &from_bar) == RATIONAL_OK);
    assert(rational_mul_int(from_bar, GRID, &scaled) == RATIONAL_OK && scaled.den == 1);
    return (long)scaled.num;
}

static long grid_length(const Motive *motive, size_t i) {
    Rational scaled;
    assert(rational_mul_int(motive->notes[i].duration, GRID, &scaled) == RATIONAL_OK && scaled.den == 1);
    return (long)scaled.num;
}

/*
 * Counts the figures of a durations entry in the motives of seeds 1 to 16 in 4/4 (the defaults plus the entry at weight 4):
 * `first` then `rest` consecutive notes of the lengths `lengths` (in forty-eighths), the first starting at a multiple of
 * `start_unit`. Every such figure must start where requirement 3 of B4 says; the motives holding at least one are counted.
 */
static int motives_with_figure(size_t entry, const long *lengths, size_t notes, long start_unit) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.durations[entry] = 4;
    int holding = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, &preferences);
        bool found = false;
        for (size_t i = 0; i + notes <= motive.note_count; i++) {
            bool match = true;
            for (size_t k = 0; k < notes && match; k++) {
                match = grid_length(&motive, i + k) == lengths[k];
            }
            if (match) {
                assert(grid_start(&motive, i) % start_unit == 0);
                found = true;
                i += notes - 1; /* the next figure starts after this one */
            }
        }
        holding += found;
    }
    return holding;
}

static void rhythm_figures(void) {
    const long sixteenths[] = {3, 3};
    const long long_short[] = {9, 3};
    const long short_long[] = {3, 9};
    const long triplet[] = {4, 4, 4};
    const long triplet_two[] = {8, 8, 8};
    const struct {
        const char *name;
        size_t entry;
        const long *lengths;
        size_t notes;
        long start_unit;
    } figures[] = {{"quarter_beats", 3, sixteenths, 2, 6}, {"long_short", 4, long_short, 2, 12},
                   {"short_long", 5, short_long, 2, 12},   {"triplet", 6, triplet, 3, 12},
                   {"triplet_two_beats", 7, triplet_two, 3, 12}};
    for (size_t f = 0; f < sizeof(figures) / sizeof(figures[0]); f++) {
        int holding = motives_with_figure(figures[f].entry, figures[f].lengths, figures[f].notes, figures[f].start_unit);
        printf("%s 4: %d of %d motives hold the figure\n", figures[f].name, holding, SEEDS);
        assert(holding >= 1);
    }
}

/* The notes of a motive in a compound meter that cross a dotted-quarter beat (3/8 of a whole, 36 ninety-sixths). */
static int notes_crossing_beats(const Motive *motive) {
    int crossing = 0;
    for (size_t i = 0; i < motive->note_count; i++) {
        Rational from_bar;
        Rational start;
        Rational length;
        assert(rational_sub(motive->notes[i].onset, motive_pickup(motive), &from_bar) == RATIONAL_OK);
        assert(rational_mul_int(from_bar, 96, &start) == RATIONAL_OK && start.den == 1);
        assert(rational_mul_int(motive->notes[i].duration, 96, &length) == RATIONAL_OK && length.den == 1);
        long offset = (((long)start.num % 36) + 36) % 36; /* a beat is 3/8 = 36 ninety-sixths */
        crossing += offset != 0 && offset + (long)length.num > 36;
    }
    return crossing;
}

static void compound_beats(void) {
    MotiveRandomPreferences off;
    motive_random_preferences_default(&off);
    MotiveRandomPreferences on = off;
    on.compound_beats = true;
    int crossing_off = 0;
    const int meters[] = {6, 9, 12};
    for (size_t m = 0; m < 3; m++) {
        int crossing_on = 0;
        int made = 0;
        for (unsigned long seed = 1; seed <= 40; seed++) {
            MotiveRandomOptions options = options_of(meters[m], seed, &on);
            options.meter.beat_unit = 8;
            Motive motive;
            size_t count = 0;
            assert(motive_random(&options, &motive, &count) == MOTIVE_RANDOM_OK);
            crossing_on += notes_crossing_beats(&motive);
            options.preferences = &off;
            /* a seed that gives no motive without the setting (9/8, seed 36, as at e14fa45) is not counted */
            if (motive_random(&options, &motive, &count) == MOTIVE_RANDOM_OK) {
                crossing_off += notes_crossing_beats(&motive);
            }
            made++;
        }
        printf("compound_beats in %d/8: %d notes cross a beat (%d of %d motives made)\n", meters[m], crossing_on, made, 40);
        assert(crossing_on == 0);
    }
    /* the dotted feature of a compound beat: a dotted eighth, a sixteenth and an eighth, from a beat */
    MotiveRandomPreferences dotted = on;
    dotted.features[1] = 0;
    dotted.features[2] = 0;
    int dotted_motives = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        MotiveRandomOptions options = options_of(6, seed, &dotted);
        options.meter.beat_unit = 8;
        Motive motive;
        size_t count = 0;
        assert(motive_random(&options, &motive, &count) == MOTIVE_RANDOM_OK);
        bool found = false;
        for (size_t i = motive.features[0].first; i + 2 <= motive.features[0].last; i++) {
            Rational start;
            Rational a;
            Rational b;
            Rational c;
            assert(rational_sub(motive.notes[i].onset, motive_pickup(&motive), &start) == RATIONAL_OK);
            assert(rational_mul_int(start, 96, &a) == RATIONAL_OK && a.den == 1);
            if ((long)a.num % 36 == 0 && rational_mul_int(motive.notes[i].duration, 96, &a) == RATIONAL_OK && a.num == 18 &&
                rational_mul_int(motive.notes[i + 1].duration, 96, &b) == RATIONAL_OK && b.num == 6 &&
                rational_mul_int(motive.notes[i + 2].duration, 96, &c) == RATIONAL_OK && c.num == 12) {
                found = true;
            }
        }
        dotted_motives += found;
    }
    printf("compound_beats in 6/8: %d of %d motives hold the dotted eighth, sixteenth, eighth\n", dotted_motives, SEEDS);
    assert(dotted_motives >= 1);
    printf("compound_beats off: %d notes cross a beat in 6/8, 9/8 and 12/8\n", crossing_off);
    assert(crossing_off > 0);
    /* in 4/4 it changes nothing */
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive without = make(4, seed, &off);
        Motive with = make(4, seed, &on);
        assert(same_motive(&without, &with));
    }
}

/* ------------------------------------------- endings and long non-chord tones (phase L2, R-440, R-441) */

static const int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};

/* The pitch class of the root of the chord of degree d in C major. */
static int root_pc(int degree) {
    return MAJOR_SCALE[(degree - 1) % 7];
}

static int pitch_class(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

static MotiveRandomPreferences only_ending(size_t ending) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.endings[0] = 0;
    preferences.endings[ending] = 1;
    return preferences;
}

/* The metric level of a position in forty-eighths of a whole note from the bar line of 4/4: 3 the downbeat, 2 beat three, 1 other beats. */
static int level_of(long position) {
    return position % 48 == 0 ? 3 : position % 24 == 0 ? 2 : 1;
}

/* Closed: the last note is the root or third of the last chord, 1 or 2 semitones from the note before, longer than every other
   note of the last segment, and starts on a beat of the highest level the segment offers. */
static void closed_endings(void) {
    MotiveRandomPreferences preferences = only_ending(1);
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, &preferences);
        size_t last = motive.note_count - 1;
        const MotiveSketchFeature *segment = &motive.features[motive.feature_count - 1];
        int degree = motive.chords[motive.chord_count - 1].degree;
        int pc = pitch_class(motive.notes[last].pitch);
        assert(pc == root_pc(degree) || pc == MAJOR_SCALE[(degree + 1) % 7]);
        int move = abs(motive.notes[last].pitch - motive.notes[last - 1].pitch);
        assert(move == 1 || move == 2);
        for (size_t i = segment->first; i < last; i++) {
            assert(rational_cmp(motive.notes[i].duration, motive.notes[last].duration) < 0);
        }
        long first = grid_start(&motive, segment->first);
        long start = grid_start(&motive, last);
        Rational body;
        Rational scaled;
        assert(rational_sub(motive.length, motive_pickup(&motive), &body) == RATIONAL_OK);
        assert(rational_mul_int(body, GRID, &scaled) == RATIONAL_OK && scaled.den == 1);
        long length = (long)scaled.num - first;
        int best = 0;
        for (long p = 12; p <= length - 12; p += 12) {
            best = level_of(first + p) > best ? level_of(first + p) : best;
        }
        assert(start - first >= 12 && start - first <= length - 12 && level_of(start) == best);
    }
    printf("endings closed only: %d motives end closed\n", SEEDS);
}

/* Open: the last note is degree 2, 5 or 7 of the key. */
static void open_endings(void) {
    MotiveRandomPreferences preferences = only_ending(2);
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, &preferences);
        int pc = pitch_class(motive.notes[motive.note_count - 1].pitch);
        assert(pc == MAJOR_SCALE[1] || pc == MAJOR_SCALE[4] || pc == MAJOR_SCALE[6]);
    }
    printf("endings open only: %d motives end on degree 2, 5 or 7\n", SEEDS);
}

/* Lead-in: the last two notes are each at most a half beat and the last starts off the beat. */
static void lead_in_endings(void) {
    MotiveRandomPreferences preferences = only_ending(3);
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, &preferences);
        size_t last = motive.note_count - 1;
        assert(grid_length(&motive, last) <= 6 && grid_length(&motive, last - 1) <= 6);
        assert(grid_start(&motive, last) % 12 != 0);
    }
    printf("endings lead_in only: %d motives end in a lead-in\n", SEEDS);
}

/* The notes of a motive that are on-beat non-chord tones a beat long or longer resolving to the next note: how many, and each
   one is passed to `check` (NULL for none) with its label and its motive. */
typedef void (*LongToneCheck)(const Motive *motive, size_t i, const MotiveToneLabel *label);

static int long_tones(const MotiveRandomPreferences *preferences, LongToneCheck check) {
    int total = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motive = make(4, seed, preferences);
        MotiveAnalysis analysis;
        assert(motive_analyze(&motive, &analysis) && analysis.tones_known);
        for (size_t i = 0; i + 1 < motive.note_count; i++) {
            const MotiveToneLabel *label = &analysis.tones[i];
            if (label->role == MOTIVE_TONE_NON_CHORD && !label->unresolved && label->resolves_to == (long)i + 1 && starts_on_beat(&motive, i) &&
                grid_length(&motive, i) >= 12) {
                total++;
                if (check) {
                    check(&motive, i, label);
                }
            }
        }
    }
    return total;
}

static MotiveRandomPreferences long_nct_base(void) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.long_nct_one_in = 1;
    preferences.durations[2] = 4;
    return preferences;
}

static void only_neighbors(const Motive *motive, size_t i, const MotiveToneLabel *label) {
    (void)motive;
    (void)i;
    assert(label->type == NON_CHORD_TONE_NEIGHBOR);
}

static void only_seconds(const Motive *motive, size_t i, const MotiveToneLabel *label) {
    (void)label;
    size_t segment = 0;
    while (segment + 1 < motive->feature_count && motive->features[segment].last < i) {
        segment++;
    }
    int above = pitch_class(motive->notes[i].pitch - root_pc(motive->chords[segment].degree));
    assert(above == 1 || above == 2);
}

static void only_downbeats(const Motive *motive, size_t i, const MotiveToneLabel *label) {
    (void)label;
    assert(grid_start(motive, i) % 48 == 0);
}

static void only_two_beats(const Motive *motive, size_t i, const MotiveToneLabel *label) {
    (void)label;
    assert(grid_length(motive, i) >= 24);
}

static void long_non_chord_tones(void) {
    MotiveRandomPreferences preferences = long_nct_base();
    int any = long_tones(&preferences, NULL);
    printf("long_nct_one_in 1: %d long non-chord tones in %d motives\n", any, SEEDS);
    assert(any >= 1);

    MotiveRandomPreferences neighbors = long_nct_base();
    for (size_t i = 0; i < 5; i++) {
        neighbors.long_nct_types[i] = i == 4;
    }
    printf("long_nct_types neighbor only: %d tones, all neighbors\n", long_tones(&neighbors, only_neighbors));

    MotiveRandomPreferences seconds = long_nct_base();
    for (size_t i = 0; i < 4; i++) {
        seconds.long_nct_intervals[i] = i == 0;
    }
    printf("long_nct_intervals second only: %d tones, all a second above the chord root\n", long_tones(&seconds, only_seconds));

    MotiveRandomPreferences downbeats = long_nct_base();
    downbeats.long_nct_beats[1] = 0;
    downbeats.long_nct_beats[2] = 0;
    printf("long_nct_beats downbeat only: %d tones, all on a downbeat\n", long_tones(&downbeats, only_downbeats));

    MotiveRandomPreferences two = long_nct_base();
    two.long_nct_lengths[0] = 0;
    two.long_nct_lengths[1] = 0;
    printf("long_nct_lengths two_beats only: %d tones, all two beats or longer\n", long_tones(&two, only_two_beats));
}

/* Rhythm variety (R-443): with the taste profile "none" settings and the endings drawn, the most common duration sequence is under
   18 of 60 motives, in each of three meters and two modes. */
static bool same_durations(const Motive *a, const Motive *b) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (rational_cmp(a->notes[i].duration, b->notes[i].duration) != 0) {
            return false;
        }
    }
    return true;
}

static void rhythm_variety(void) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.durations[0] = 4;
    preferences.durations[1] = 2;
    preferences.durations[2] = 2;
    preferences.features[0] = 3;
    preferences.features[1] = 1;
    preferences.features[2] = 0;
    preferences.rung_moves[0] = 6;
    preferences.rung_moves[1] = 1;
    preferences.decorations[0] = 0;
    preferences.decorations[1] = 2;
    preferences.decorations[2] = 2;
    preferences.decorations[3] = 1;
    preferences.decorations[4] = 1;
    preferences.endings[0] = 0;
    preferences.endings[1] = 2;
    preferences.endings[2] = 1;
    preferences.endings[3] = 1;
    const int beats[3] = {4, 3, 6};
    const int units[3] = {4, 4, 8};
    for (int minor = 0; minor < 2; minor++) {
        for (int m = 0; m < 3; m++) {
            static Motive motives[60];
            for (unsigned long seed = 500; seed < 560; seed++) {
                MotiveRandomOptions options = options_of(beats[m], seed, &preferences);
                options.meter.beat_unit = units[m];
                options.mode = minor ? KEY_STRUCTURE_MODE_MINOR : KEY_STRUCTURE_MODE_MAJOR;
                size_t count = 0;
                assert(motive_random(&options, &motives[seed - 500], &count) == MOTIVE_RANDOM_OK && count == 1);
            }
            int most = 0;
            for (size_t a = 0; a < 60; a++) {
                int same = 0;
                for (size_t b = 0; b < 60; b++) {
                    same += same_durations(&motives[a], &motives[b]);
                }
                most = same > most ? same : most;
            }
            printf("rhythm variety, %s %d/%d: the most common duration sequence is %d of 60 motives\n", minor ? "A minor" : "C major", beats[m],
                   units[m] == 4 ? 4 : 8, most);
            assert(most < 18);
        }
    }
}

/* ------------------------------------------------------ syncopation (phase L3, R-454) */

/* One syncopation type at the given levels (bit 0 eighth, bit 1 sixteenth), one in one. */
static MotiveRandomPreferences only_syncopation(size_t type, unsigned levels) {
    MotiveRandomPreferences preferences;
    motive_random_preferences_default(&preferences);
    preferences.syncopation_one_in = 1;
    for (size_t i = 0; i < 4; i++) {
        preferences.syncopation_types[i] = i == type;
    }
    for (size_t i = 0; i < 2; i++) {
        preferences.syncopation_levels[i] = (levels >> i) & 1u;
    }
    return preferences;
}

/* The last note starts on a beat or is not held across one: it is never syncopated. */
static void last_note_plain(const Motive *motive) {
    size_t last = motive->note_count - 1;
    long offset = ((grid_start(motive, last) % 12) + 12) % 12;
    assert(offset == 0 || offset + grid_length(motive, last) <= 12);
}

/* True when the motive holds notes of the lengths `lengths` (forty-eighths) in a row, the first starting on a beat. */
static bool holds_figure_on_beat(const Motive *motive, const long *lengths, size_t notes) {
    for (size_t i = 0; i + notes <= motive->note_count; i++) {
        bool match = grid_start(motive, i) % 12 == 0;
        for (size_t k = 0; k < notes && match; k++) {
            match = grid_length(motive, i + k) == lengths[k];
        }
        if (match) {
            return true;
        }
    }
    return false;
}

/* Each in-cell figure at each level alone: the motives of seeds 1 to 16 that hold it, in 4/4 and 3/4. */
static void syncopation_figures(void) {
    const long off_beat_eighth_a[] = {6, 12, 6};
    const long off_beat_eighth_b[] = {6, 18};
    const long off_beat_sixteenth_a[] = {3, 6, 3};
    const long off_beat_sixteenth_b[] = {3, 9};
    const long tie_beat_eighth[] = {18, 18, 12};
    const long tie_beat_sixteenth[] = {9, 9, 6};
    const struct {
        const char *name;
        size_t type;
        unsigned level;
        const long *first;
        size_t first_notes;
        const long *second;
        size_t second_notes;
    } figures[] = {{"off_beat eighth", 0, 1, off_beat_eighth_a, 3, off_beat_eighth_b, 2},
                   {"off_beat sixteenth", 0, 2, off_beat_sixteenth_a, 3, off_beat_sixteenth_b, 2},
                   {"tie_beat eighth", 1, 1, tie_beat_eighth, 3, NULL, 0},
                   {"tie_beat sixteenth", 1, 2, tie_beat_sixteenth, 3, NULL, 0}};
    for (size_t f = 0; f < sizeof(figures) / sizeof(figures[0]); f++) {
        MotiveRandomPreferences preferences = only_syncopation(figures[f].type, figures[f].level);
        for (int beats = 4; beats >= 3; beats--) {
            int holding = 0;
            for (unsigned long seed = 1; seed <= SEEDS; seed++) {
                Motive motive = make(beats, seed, &preferences);
                last_note_plain(&motive);
                holding += holds_figure_on_beat(&motive, figures[f].first, figures[f].first_notes) ||
                           (figures[f].second != NULL && holds_figure_on_beat(&motive, figures[f].second, figures[f].second_notes));
            }
            printf("syncopation %s in %d/4: %d of %d motives hold the figure\n", figures[f].name, beats, holding, SEEDS);
            if (figures[f].type == 1 && figures[f].level == 1 && beats == 3) {
                assert(holding == 0); /* four beats never fit a 3/4 cell */
            } else {
                assert(holding >= 1);
            }
        }
    }
}

static bool triad_tone(int pitch, int degree) {
    int pc = pitch_class(pitch);
    return pc == root_pc(degree) || pc == MAJOR_SCALE[(degree + 1) % 7] || pc == MAJOR_SCALE[(degree + 3) % 7];
}

/* Chord c's onset as whole-note forty-eighths from the first bar line. */
static long chord_grid_start(const Motive *motive, size_t c) {
    Rational from_bar;
    Rational scaled;
    assert(rational_sub(motive->chords[c].onset, motive_pickup(motive), &from_bar) == RATIONAL_OK);
    assert(rational_mul_int(from_bar, GRID, &scaled) == RATIONAL_OK && scaled.den == 1);
    return (long)scaled.num;
}

/* Tie across the barline: a note held over a chord onset at a barline ends its feature and is a triad tone of both chords.
   Anticipation: a feature's first note starting before its chord's onset is a triad tone of the new chord, and the chord changed. */
static void syncopation_boundaries(void) {
    MotiveRandomPreferences tie_bar = only_syncopation(2, 3);
    MotiveRandomPreferences anticipation = only_syncopation(3, 3);
    for (int beats = 3; beats <= 4; beats++) {
        long bar = 12 * beats;
        int tied = 0;
        int early = 0;
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            Motive motive = make(beats, seed, &tie_bar);
            last_note_plain(&motive);
            bool held = false;
            for (size_t c = 1; c < motive.chord_count; c++) {
                long onset = chord_grid_start(&motive, c);
                if (onset % bar != 0) {
                    continue;
                }
                for (size_t i = 0; i < motive.note_count; i++) {
                    long start = grid_start(&motive, i);
                    if (start < onset && start + grid_length(&motive, i) > onset) {
                        held = true;
                        assert(i == motive.features[c - 1].last);
                        assert(triad_tone(motive.notes[i].pitch, motive.chords[c - 1].degree));
                        assert(triad_tone(motive.notes[i].pitch, motive.chords[c].degree));
                    }
                }
            }
            tied += held;
            Motive anticipated = make(beats, seed, &anticipation);
            last_note_plain(&anticipated);
            bool arrived = false;
            for (size_t c = 1; c < anticipated.chord_count; c++) {
                size_t first = anticipated.features[c].first;
                if (grid_start(&anticipated, first) < chord_grid_start(&anticipated, c)) {
                    arrived = true;
                    assert(anticipated.chords[c].degree != anticipated.chords[c - 1].degree);
                    assert(triad_tone(anticipated.notes[first].pitch, anticipated.chords[c].degree));
                }
            }
            early += arrived;
        }
        printf("syncopation tie_bar in %d/4: %d of %d motives tie across a barline; anticipation: %d of %d\n", beats, tied, SEEDS, early, SEEDS);
        assert(tied >= 1 && early >= 1);
    }
}

/* A motive of any meter and beat unit; false when none is made. */
static bool make_in_unit(int beats, int unit, unsigned long seed, const MotiveRandomPreferences *preferences, Motive *out) {
    MotiveRandomOptions options = options_of(beats, seed, preferences);
    options.meter.beat_unit = unit;
    size_t count = 0;
    return motive_random(&options, out, &count) == MOTIVE_RANDOM_OK && count == 1;
}

/* Compound and six, nine and twelve beat meters are never syncopated, and a setting with nothing to draw draws nothing. */
static void syncopation_changes_nothing_else(void) {
    MotiveRandomPreferences off;
    motive_random_preferences_default(&off);
    MotiveRandomPreferences on = off;
    on.syncopation_one_in = 1;
    MotiveRandomPreferences compound_off = off;
    MotiveRandomPreferences compound_on = on;
    compound_on.compound_beats = true;
    const int meters[] = {6, 12};
    for (size_t m = 0; m < 2; m++) {
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            Motive without;
            Motive with;
            bool made = make_in_unit(meters[m], 8, seed, &compound_off, &without);
            assert(make_in_unit(meters[m], 8, seed, &on, &with) == made);
            assert(!made || same_motive(&without, &with));
            Motive grouped_without;
            Motive grouped_with;
            MotiveRandomPreferences grouped_off = off;
            grouped_off.compound_beats = true;
            made = make_in_unit(meters[m], 8, seed, &grouped_off, &grouped_without);
            assert(make_in_unit(meters[m], 8, seed, &compound_on, &grouped_with) == made);
            assert(!made || same_motive(&grouped_without, &grouped_with));
        }
    }
    MotiveRandomPreferences no_types = on;
    MotiveRandomPreferences no_levels = on;
    for (size_t i = 0; i < 4; i++) {
        no_types.syncopation_types[i] = 0;
    }
    for (size_t i = 0; i < 2; i++) {
        no_levels.syncopation_levels[i] = 0;
    }
    for (int beats = 3; beats <= 4; beats++) {
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            Motive without = make(beats, seed, &off);
            Motive types = make(beats, seed, &no_types);
            Motive levels = make(beats, seed, &no_levels);
            assert(same_motive(&without, &types));
            assert(same_motive(&without, &levels));
        }
    }
    printf("syncopation: compound and six, nine, twelve beat meters and empty settings change nothing\n");
}

static void each_setting_still_gives_motives(void) {
    MotiveRandomPreferences base;
    motive_random_preferences_default(&base);
    MotiveRandomPreferences syncopation = base;
    syncopation.syncopation_one_in = 1;
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
    MotiveRandomPreferences figures[5];
    for (size_t i = 0; i < 5; i++) {
        figures[i] = base;
        figures[i].durations[3 + i] = 4;
    }
    MotiveRandomPreferences compound = base;
    compound.compound_beats = true;
    MotiveRandomPreferences endings[3];
    for (size_t i = 0; i < 3; i++) {
        endings[i] = only_ending(1 + i);
    }
    MotiveRandomPreferences long_nct = base;
    long_nct.long_nct_one_in = 1;
    const struct {
        const char *name;
        const MotiveRandomPreferences *preferences;
        int beats;
        int unit;
    } settings[] = {{"accented_one_in 1", &accented, 4, 4},   {"avoid_repeats", &repeats, 4, 4},
                    {"leaps_per_motif 1", &leaps, 4, 4},      {"leap_recovery", &recovery, 4, 4},
                    {"pickup_from_accent", &pickup, 4, 4},    {"contrast_fidelity_min 1/2", &contrast, 4, 4},
                    {"long_on_weak 20", &weak, 4, 4},         {"final_long", &final_long, 4, 4},
                    {"quarter_beats 4", &figures[0], 4, 4},   {"long_short 4", &figures[1], 4, 4},
                    {"short_long 4", &figures[2], 4, 4},      {"triplet 4", &figures[3], 4, 4},
                    {"triplet_two_beats 4", &figures[4], 4, 4}, {"compound_beats in 6/8", &compound, 6, 8},
                    {"compound_beats in 12/8", &compound, 12, 8}, {"endings closed", &endings[0], 4, 4},
                    {"endings open", &endings[1], 4, 4},      {"endings lead_in", &endings[2], 4, 4},
                    {"long_nct_one_in 1", &long_nct, 4, 4},   {"syncopation_one_in 1 in 4/4", &syncopation, 4, 4},
                    {"syncopation_one_in 1 in 3/4", &syncopation, 3, 4}};
    for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
        int made = motives_made_in(settings[i].beats, settings[i].unit, settings[i].preferences);
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
    rhythm_figures();
    compound_beats();
    closed_endings();
    open_endings();
    lead_in_endings();
    long_non_chord_tones();
    rhythm_variety();
    syncopation_figures();
    syncopation_boundaries();
    syncopation_changes_nothing_else();
    each_setting_still_gives_motives();
    printf("test_motif_preferences: ok\n");
    return 0;
}
