#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_random.h>

#include "motive/motive_internal.h"

/*
 * Random motives: in C major 4/4, D minor 4/4, A minor 3/4, D major 6/8 and G major 2/4, seeds 1 to 20 give motives of
 * 2 to 12 notes within a twelfth, one or two bars (and a pickup of at most a beat), that keep the hard melodic rules, in
 * the key asked for (C major or A minor without one). The same options and seed give the same motives and another seed
 * does not; a rhythm motive is all on degree 1 and a contour motive in quarters; a style is used and keeps the rules.
 */

typedef struct {
    const char *name;
    PlanKeyMode mode;
    int beats;
    int unit;
    bool has_key;
    int tonic;
} Scenario;

static const Scenario SCENARIOS[] = {
    {"C major 4/4", PLAN_MODE_MAJOR, 4, 4, false, 0},
    {"D minor 4/4", PLAN_MODE_MINOR, 4, 4, true, 2},
    {"A minor 3/4", PLAN_MODE_MINOR, 3, 4, false, 0},
    {"D major 6/8", PLAN_MODE_MAJOR, 6, 8, true, 2},
    {"G major 2/4", PLAN_MODE_MAJOR, 2, 4, true, 7},
};

#define SEEDS 20
#define COUNT 4

static MotiveRandomOptions options_of(const Scenario *scenario, unsigned long seed) {
    MotiveRandomOptions options;
    memset(&options, 0, sizeof(options));
    options.mode = scenario->mode;
    options.meter.beats_per_bar = scenario->beats;
    options.meter.beat_unit = scenario->unit;
    options.has_key = scenario->has_key;
    options.key.tonic_pitch_class = scenario->tonic;
    options.key.mode = scenario->mode;
    options.form = MOTIVE_FORM_FULL;
    options.count = COUNT;
    options.seed = seed;
    return options;
}

static Rational time_of(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

/* What every full motive must be, whatever the options. */
static void check_motive(const Scenario *scenario, const Motive *motive) {
    assert(motive->meter.beats_per_bar == scenario->beats && motive->meter.beat_unit == scenario->unit);
    assert(motive->key_source == MOTIVE_KEY_GIVEN);
    assert(motive->key.mode == scenario->mode);
    int tonic = scenario->has_key ? scenario->tonic : (scenario->mode == PLAN_MODE_MAJOR ? 0 : 9);
    assert(motive->key.tonic_pitch_class == tonic);
    assert(motive->form == MOTIVE_FORM_FULL);
    assert(motive->note_count >= 2 && motive->note_count <= 12);

    int low = 127;
    int high = 0;
    Rational onset = time_of(0, 1);
    for (size_t i = 0; i < motive->note_count; i++) {
        const PlanNote *note = &motive->notes[i];
        assert(rational_cmp(note->offset, onset) == 0);
        assert(rational_add(onset, note->duration, &onset) == RATIONAL_OK);
        low = note->midi_pitch < low ? note->midi_pitch : low;
        high = note->midi_pitch > high ? note->midi_pitch : high;
    }
    assert(rational_cmp(motive->length, onset) == 0);
    assert(high - low <= 19);

    /* One or two bars, after a pickup of a beat or less. */
    Rational bar = time_of(scenario->beats, (unsigned long)scenario->unit);
    Rational beat = time_of(1, (unsigned long)scenario->unit);
    Rational pickup = motive_pickup(motive);
    assert(rational_cmp(pickup, beat) <= 0);
    Rational bars = motive_time_minus(motive->length, pickup);
    Rational two;
    assert(rational_add(bar, bar, &two) == RATIONAL_OK);
    assert(rational_cmp(bars, bar) == 0 || rational_cmp(bars, two) == 0);

    const char *fault = "unchecked";
    assert(motive_melodic_fault(motive, &fault));
    if (fault) {
        fprintf(stderr, "a random motive breaks %s\n", fault);
    }
    assert(fault == NULL);
}

static void test_rules(void) {
    for (size_t s = 0; s < sizeof(SCENARIOS) / sizeof(SCENARIOS[0]); s++) {
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            MotiveRandomOptions options = options_of(&SCENARIOS[s], seed);
            Motive motives[COUNT];
            size_t made = 0;
            MotiveRandomResult result = motive_random(&options, motives, &made);
            if (result != MOTIVE_RANDOM_OK) {
                fprintf(stderr, "%s seed %lu: %s\n", SCENARIOS[s].name, seed, motive_random_result_name(result));
            }
            assert(result == MOTIVE_RANDOM_OK && made == COUNT);
            for (size_t i = 0; i < made; i++) {
                check_motive(&SCENARIOS[s], &motives[i]);
                for (size_t j = 0; j < i; j++) {
                    assert(!motive_equal(&motives[i], &motives[j]));
                }
            }
        }
    }
}

/* In 3/4 with a pickup the bars are 3 beats; some motive in the seeds has one and some does not. */
static void test_minor_three_four_lengths(void) {
    size_t with_pickup = 0;
    size_t one_bar = 0;
    size_t two_bars = 0;
    const Scenario *scenario = &SCENARIOS[2];
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        MotiveRandomOptions options = options_of(scenario, seed);
        Motive motives[COUNT];
        size_t made = 0;
        assert(motive_random(&options, motives, &made) == MOTIVE_RANDOM_OK);
        for (size_t i = 0; i < made; i++) {
            Rational pickup = motive_pickup(&motives[i]);
            Rational bars = motive_time_minus(motives[i].length, pickup);
            with_pickup += pickup.num != 0;
            one_bar += rational_cmp(bars, time_of(3, 4)) == 0;
            two_bars += rational_cmp(bars, time_of(3, 2)) == 0;
        }
    }
    assert(with_pickup > 0 && with_pickup < SEEDS * COUNT);
    assert(one_bar > 0 && two_bars > 0 && one_bar + two_bars == SEEDS * COUNT);
}

/*
 * The frame: the base harmony is I and proposed, the first and last notes and every note on a beat are chord tones, every
 * other note is a chord tone or a non-chord tone with a type that resolves to a later chord tone, and the same seed gives
 * the same tones. Over the seeds, all four types that can form are heard.
 */
static void test_frames(void) {
    bool heard[NON_CHORD_TONE_COUNT] = {false};
    size_t non_chord = 0;
    for (size_t s = 0; s < sizeof(SCENARIOS) / sizeof(SCENARIOS[0]); s++) {
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            MotiveRandomOptions options = options_of(&SCENARIOS[s], seed);
            Motive motives[COUNT];
            size_t made = 0;
            assert(motive_random(&options, motives, &made) == MOTIVE_RANDOM_OK && made == COUNT);
            for (size_t i = 0; i < made; i++) {
                MotiveAnalysis analysis;
                assert(motive_analyze(&motives[i], &analysis));
                assert(analysis.base_harmony_known && !analysis.base_harmony.given);
                assert(analysis.base_harmony.count == 1 && analysis.base_harmony.chords[0].degree == 1);
                assert(analysis.tones_known);
                size_t last = motives[i].note_count - 1;
                for (size_t n = 0; n <= last; n++) {
                    const MotiveToneLabel *label = &analysis.tones[n];
                    if (analysis.rhythm[n].on_beat || n == 0 || n == last) {
                        assert(label->role == MOTIVE_TONE_CHORD);
                    } else if (label->role == MOTIVE_TONE_NON_CHORD) {
                        assert(!label->unresolved && label->resolves_to > (long)n);
                        assert(analysis.tones[label->resolves_to].role == MOTIVE_TONE_CHORD);
                        heard[label->type] = true;
                        non_chord++;
                    } else {
                        assert(label->role == MOTIVE_TONE_CHORD);
                    }
                }
            }
        }
    }
    assert(non_chord > 0);
    assert(heard[NON_CHORD_TONE_PASSING] && heard[NON_CHORD_TONE_NEIGHBOR] && heard[NON_CHORD_TONE_APPOGGIATURA] &&
           heard[NON_CHORD_TONE_ESCAPE_TONE]);

    MotiveRandomOptions options = options_of(&SCENARIOS[1], 7);
    Motive first[COUNT];
    Motive again[COUNT];
    size_t made = 0;
    assert(motive_random(&options, first, &made) == MOTIVE_RANDOM_OK);
    assert(motive_random(&options, again, &made) == MOTIVE_RANDOM_OK);
    for (size_t i = 0; i < COUNT; i++) {
        MotiveAnalysis a;
        MotiveAnalysis b;
        assert(motive_analyze(&first[i], &a) && motive_analyze(&again[i], &b));
        assert(memcmp(a.tones, b.tones, first[i].note_count * sizeof(a.tones[0])) == 0);
    }
}

static void test_seeds(void) {
    MotiveRandomOptions options = options_of(&SCENARIOS[1], 11);
    Motive first[COUNT];
    Motive again[COUNT];
    Motive other[COUNT];
    size_t made = 0;
    assert(motive_random(&options, first, &made) == MOTIVE_RANDOM_OK && made == COUNT);
    assert(motive_random(&options, again, &made) == MOTIVE_RANDOM_OK && made == COUNT);
    for (size_t i = 0; i < COUNT; i++) {
        assert(motive_equal(&first[i], &again[i]));
    }
    options.seed = 12;
    assert(motive_random(&options, other, &made) == MOTIVE_RANDOM_OK);
    bool differs = false;
    for (size_t i = 0; i < COUNT; i++) {
        differs = differs || !motive_equal(&first[i], &other[i]);
    }
    assert(differs);
}

static void test_forms(void) {
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Motive motives[COUNT];
        size_t made = 0;
        MotiveRandomOptions options = options_of(&SCENARIOS[1], seed);
        options.form = MOTIVE_FORM_RHYTHM;
        assert(motive_random(&options, motives, &made) == MOTIVE_RANDOM_OK && made == COUNT);
        for (size_t i = 0; i < made; i++) {
            assert(motives[i].form == MOTIVE_FORM_RHYTHM && motives[i].key_source == MOTIVE_KEY_GIVEN);
            for (size_t n = 1; n < motives[i].note_count; n++) {
                assert(motives[i].notes[n].midi_pitch == motives[i].notes[0].midi_pitch);
            }
        }
        options.form = MOTIVE_FORM_CONTOUR;
        assert(motive_random(&options, motives, &made) == MOTIVE_RANDOM_OK && made == COUNT);
        for (size_t i = 0; i < made; i++) {
            assert(motives[i].form == MOTIVE_FORM_CONTOUR && motive_pickup(&motives[i]).num == 0);
            for (size_t n = 0; n < motives[i].note_count; n++) {
                assert(rational_cmp(motives[i].notes[n].duration, time_of(1, 4)) == 0);
            }
        }
    }
}

static void test_style(void) {
    ComposerStyle *style = NULL;
    assert(composer_style_load(COMPOSER_FIXTURE_DIR "/styles/chorales.style.json", &style) == COMPOSER_STYLE_OK);
    MotiveRandomOptions options = options_of(&SCENARIOS[0], 5);
    options.style = style;
    Motive styled[COUNT];
    Motive again[COUNT];
    Motive plain[COUNT];
    size_t made = 0;
    assert(motive_random(&options, styled, &made) == MOTIVE_RANDOM_OK && made == COUNT);
    for (size_t i = 0; i < made; i++) {
        check_motive(&SCENARIOS[0], &styled[i]);
    }
    assert(motive_random(&options, again, &made) == MOTIVE_RANDOM_OK);
    for (size_t i = 0; i < COUNT; i++) {
        assert(motive_equal(&styled[i], &again[i]));
    }
    options.style = NULL;
    assert(motive_random(&options, plain, &made) == MOTIVE_RANDOM_OK);
    bool differs = false;
    for (size_t i = 0; i < COUNT; i++) {
        differs = differs || !motive_equal(&styled[i], &plain[i]);
    }
    assert(differs);
    composer_style_destroy(style);
}

static void test_refusals(void) {
    Motive motives[COUNT];
    size_t made = 99;
    MotiveRandomOptions options = options_of(&SCENARIOS[0], 1);
    options.key.mode = PLAN_MODE_MINOR;
    options.has_key = true;
    assert(motive_random(&options, motives, &made) == MOTIVE_RANDOM_ERR_KEY && made == 0);
    options = options_of(&SCENARIOS[0], 1);
    options.meter.beat_unit = 3;
    assert(motive_random(&options, motives, &made) == MOTIVE_RANDOM_ERR_METER);
    options = options_of(&SCENARIOS[0], 1);
    options.count = 0;
    assert(motive_random(&options, motives, &made) == MOTIVE_RANDOM_ERR_ARGUMENT);
    options.count = MOTIVE_RANDOM_MAX_COUNT + 1;
    assert(motive_random(&options, motives, &made) == MOTIVE_RANDOM_ERR_ARGUMENT);
    assert(motive_random(NULL, motives, &made) == MOTIVE_RANDOM_ERR_NULL);
}

int main(void) {
    test_rules();
    test_minor_three_four_lengths();
    test_frames();
    test_seeds();
    test_forms();
    test_style();
    test_refusals();
    puts("ok");
    return 0;
}
