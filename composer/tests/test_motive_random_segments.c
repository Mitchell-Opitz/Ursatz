#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_random.h>

#include "motive/motive_internal.h"

/*
 * Segment-based random motives: 200 seeds over 2/4, 3/4, 4/4 and 6/8 in major and minor give motives with no melodic fault,
 * whose segments cover every note, contiguous and labelled in order of first appearance (a repeat of a rhythm as a', a''),
 * whose base harmony is given and one of the plans, whose a' has a's duration sequence, and whose last note is a chord tone
 * of at least a beat. The same seed gives the same motive. At least 60 percent have a repeated segment (a').
 */

#define SEEDS 200

typedef struct {
    KeyStructureMode mode;
    int beats;
    int unit;
} Scenario;

static const Scenario SCENARIOS[] = {
    {KEY_STRUCTURE_MODE_MAJOR, 2, 4}, {KEY_STRUCTURE_MODE_MINOR, 2, 4}, {KEY_STRUCTURE_MODE_MAJOR, 3, 4},
    {KEY_STRUCTURE_MODE_MINOR, 3, 4}, {KEY_STRUCTURE_MODE_MAJOR, 4, 4}, {KEY_STRUCTURE_MODE_MINOR, 4, 4},
    {KEY_STRUCTURE_MODE_MAJOR, 6, 8}, {KEY_STRUCTURE_MODE_MINOR, 6, 8},
};

/* The harmony plans the strategy draws from, one degree per segment. */
static const int PLANS[][3] = {{1, 5, 0}, {1, 4, 0}, {1, 1, 0}, {1, 5, 1}, {1, 4, 1}};

static MotiveRandomOptions options_of(const Scenario *scenario, unsigned long seed) {
    MotiveRandomOptions options;
    memset(&options, 0, sizeof(options));
    options.mode = scenario->mode;
    options.meter.beats_per_bar = scenario->beats;
    options.meter.beat_unit = scenario->unit;
    options.form = MOTIVE_SKETCH_FORM_FULL;
    options.count = 1;
    options.seed = seed;
    return options;
}

/* Does segment s have the same duration sequence as segment t (a pickup note is not part of segment 0's rhythm)? */
static bool same_rhythm(const Motive *motive, size_t s, size_t t) {
    size_t from_s = motive->features[s].first + (s == 0 && motive_pickup(motive).num != 0 ? 1 : 0);
    size_t from_t = motive->features[t].first + (t == 0 && motive_pickup(motive).num != 0 ? 1 : 0);
    size_t count_s = motive->features[s].last + 1 - from_s;
    size_t count_t = motive->features[t].last + 1 - from_t;
    if (count_s != count_t) {
        return false;
    }
    for (size_t i = 0; i < count_s; i++) {
        if (rational_cmp(motive->notes[from_s + i].duration, motive->notes[from_t + i].duration) != 0) {
            return false;
        }
    }
    return true;
}

/* The segments cover every note, contiguous, with labels by the shared rule. Returns true when one is a'. */
static bool check_segments(const Motive *motive) {
    assert(motive->feature_count >= 2 && motive->feature_count <= 3);
    assert(motive->features[0].first == 0);
    assert(motive->features[motive->feature_count - 1].last == motive->note_count - 1);
    char letter = 'a';
    bool repeated = false;
    for (size_t s = 0; s < motive->feature_count; s++) {
        const MotiveSketchFeature *feature = &motive->features[s];
        assert(feature->first <= feature->last);
        assert(s == 0 || feature->first == motive->features[s - 1].last + 1);
        size_t earlier = s;
        for (size_t t = 0; t < s; t++) {
            if (same_rhythm(motive, s, t)) {
                earlier = t;
                break;
            }
        }
        char expected[MOTIVE_FEATURE_LABEL_MAX + 1];
        if (earlier == s) {
            expected[0] = letter++;
            expected[1] = '\0';
        } else {
            size_t primes = 0;
            for (size_t t = earlier + 1; t < s; t++) {
                primes += same_rhythm(motive, t, earlier);
            }
            expected[0] = motive->features[earlier].label[0];
            for (size_t p = 0; p <= primes; p++) {
                expected[1 + p] = '\'';
            }
            expected[2 + primes] = '\0';
            repeated = true;
        }
        if (strcmp(feature->label, expected) != 0) {
            fprintf(stderr, "segment %zu is '%s', expected '%s'\n", s, feature->label, expected);
        }
        assert(strcmp(feature->label, expected) == 0);
    }
    return repeated;
}

static void check_harmony(const Motive *motive, const MotiveAnalysis *analysis) {
    assert(analysis->base_harmony_known && analysis->base_harmony.given);
    assert(analysis->base_harmony.count == motive->feature_count);
    bool plan_found = false;
    for (size_t p = 0; p < sizeof(PLANS) / sizeof(PLANS[0]); p++) {
        bool same = PLANS[p][motive->feature_count - 1] != 0;
        for (size_t s = 0; same && s < motive->feature_count; s++) {
            same = analysis->base_harmony.chords[s].degree == PLANS[p][s];
        }
        plan_found = plan_found || same;
    }
    assert(plan_found);
    /* A chord on each segment's first beat. */
    for (size_t s = 1; s < motive->feature_count; s++) {
        assert(rational_cmp(analysis->base_harmony.chords[s].onset, motive->notes[motive->features[s].first].onset) == 0);
    }
}

static void test_motives(void) {
    size_t total = 0;
    size_t with_repeat = 0;
    for (size_t s = 0; s < sizeof(SCENARIOS) / sizeof(SCENARIOS[0]); s++) {
        /* Each scenario has its own 200 seeds: a seed draws the same shape in major and minor, so shared seeds would count
           the same draws twice. */
        for (unsigned long seed = 1000 * (unsigned long)s + 1; seed <= 1000 * (unsigned long)s + SEEDS; seed++) {
            MotiveRandomOptions options = options_of(&SCENARIOS[s], seed);
            Motive motive;
            size_t made = 0;
            MotiveRandomResult result = motive_random(&options, &motive, &made);
            if (result != MOTIVE_RANDOM_OK) {
                fprintf(stderr, "scenario %zu seed %lu: %s\n", s, seed, motive_random_result_name(result));
            }
            assert(result == MOTIVE_RANDOM_OK && made == 1);

            const char *fault = "unchecked";
            assert(motive_melodic_fault(&motive, &fault));
            assert(fault == NULL);

            total++;
            with_repeat += check_segments(&motive);

            MotiveAnalysis analysis;
            assert(motive_analyze(&motive, &analysis));
            check_harmony(&motive, &analysis);

            /* An a' has a's duration sequence. */
            for (size_t f = 1; f < motive.feature_count; f++) {
                if (strcmp(motive.features[f].label, "a'") == 0) {
                    assert(same_rhythm(&motive, f, 0));
                }
            }

            /* The ending: a chord tone of at least a beat. */
            size_t last = motive.note_count - 1;
            assert(analysis.tones_known && analysis.tones[last].role == MOTIVE_TONE_CHORD);
            assert(rational_cmp(motive.notes[last].duration, motive.beat) >= 0);

            /* The same seed gives the same motive. */
            Motive again;
            assert(motive_random(&options, &again, &made) == MOTIVE_RANDOM_OK && motive_equal(&motive, &again));
        }
    }
    double share = 100.0 * (double)with_repeat / (double)total;
    printf("motives with a repeated segment: %zu of %zu (%.1f percent)\n", with_repeat, total, share);
    assert(share >= 60.0);
}

/* The strategy: the zero value is segments, and the names read both ways. */
static void test_strategy(void) {
    MotiveRandomOptions options;
    memset(&options, 0, sizeof(options));
    assert(options.strategy == MOTIVE_RANDOM_STRATEGY_SEGMENTS);
    assert(strcmp(motive_random_strategy_name(MOTIVE_RANDOM_STRATEGY_SEGMENTS), "segments") == 0);
    assert(strcmp(motive_random_strategy_name(MOTIVE_RANDOM_STRATEGY_DRAWN), "drawn") == 0);
    MotiveRandomStrategy strategy;
    assert(motive_random_strategy_parse("drawn", &strategy) && strategy == MOTIVE_RANDOM_STRATEGY_DRAWN);
    assert(motive_random_strategy_parse("segments", &strategy) && strategy == MOTIVE_RANDOM_STRATEGY_SEGMENTS);
    assert(!motive_random_strategy_parse("random", &strategy) && !motive_random_strategy_parse(NULL, &strategy));

    /* Segments and drawn make different motives from one seed, and drawn has no harmony of its own. */
    MotiveRandomOptions segments = options_of(&SCENARIOS[4], 3);
    MotiveRandomOptions drawn = segments;
    drawn.strategy = MOTIVE_RANDOM_STRATEGY_DRAWN;
    Motive a;
    Motive b;
    size_t made = 0;
    assert(motive_random(&segments, &a, &made) == MOTIVE_RANDOM_OK);
    assert(motive_random(&drawn, &b, &made) == MOTIVE_RANDOM_OK);
    assert(a.chord_count >= 2 && b.chord_count == 0 && b.feature_count == 0);
    drawn.strategy = (MotiveRandomStrategy)7;
    assert(motive_random(&drawn, &b, &made) == MOTIVE_RANDOM_ERR_ARGUMENT);
}

/* A rhythm and a contour motive keep the segments and the harmony of the full one. */
static void test_forms(void) {
    for (unsigned long seed = 1; seed <= 20; seed++) {
        MotiveRandomOptions options = options_of(&SCENARIOS[4], seed);
        Motive full;
        size_t made = 0;
        assert(motive_random(&options, &full, &made) == MOTIVE_RANDOM_OK);
        options.form = MOTIVE_SKETCH_FORM_RHYTHM;
        Motive rhythm;
        assert(motive_random(&options, &rhythm, &made) == MOTIVE_RANDOM_OK);
        options.form = MOTIVE_SKETCH_FORM_CONTOUR;
        Motive contour;
        assert(motive_random(&options, &contour, &made) == MOTIVE_RANDOM_OK);
        assert(rhythm.form == MOTIVE_SKETCH_FORM_RHYTHM && rhythm.feature_count == full.feature_count);
        assert(contour.form == MOTIVE_SKETCH_FORM_CONTOUR && contour.feature_count == full.feature_count);
        assert(rhythm.chord_count == full.chord_count && contour.chord_count == full.chord_count);
        check_segments(&rhythm);
    }
}

int main(void) {
    test_strategy();
    test_motives();
    test_forms();
    printf("test_motive_random_segments: ok\n");
    return 0;
}
