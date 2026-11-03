#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_connect.h>
#include <ursatz_composer/variant_registry.h>

/*
 * The connection score (R-145): (2 x common + rhythm + harmony) / 4, halved when the candidate is remote (over 3/4 from
 * the basic motive). The parts are checked on their own, and the score as exact fractions.
 */

static Motive load(const char *notes, const char *harmony) {
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = motive_parse_meter("4/4", &options.meter);
    options.has_key = motive_parse_key("C major", &options.key);
    if (harmony) {
        /* One chord at the start: the degree is the numeral's. */
        options.has_harmony = true;
        options.harmony_count = 1;
        options.harmony[0].onset = (Rational){0, 1};
        options.harmony[0].degree = atoi(harmony);
    }
    Motive motive;
    assert(motive_from_notes_text(notes, &options, &motive, NULL) == MOTIVE_OK);
    return motive;
}

static bool equals(MotiveScore score, unsigned long numerator, unsigned long denominator) {
    return score.numerator == numerator && score.denominator == denominator;
}

static MotiveScore fraction(unsigned long numerator, unsigned long denominator) {
    MotiveScore score = {numerator, denominator};
    return score;
}

/* The same form scores highest: nothing common is lost, the rhythm is the same, and the chord only repeats. */
static void test_same_form_scores_highest(void) {
    Motive basic = load("C4:q E4:q G4:q C5:q", NULL);
    MotiveConnection same;
    assert(motive_connection(&basic, &basic, &basic, &(MotiveScore){0, 1}, &same));
    assert(equals(same.common, 1, 1) && equals(same.rhythm, 1, 1) && equals(same.harmony, 1, 2));
    assert(!same.halved && equals(same.total, 7, 8));

    /* Searched, the same motive is also at distance 0. */
    MotiveScore searched;
    assert(motive_connection_score(&basic, &basic, &basic, &searched) && equals(searched, 7, 8));

    /* A form with another rhythm, or further from the basic idea, scores lower with the same chord. */
    Motive other_rhythm = load("C4:e E4:e G4:q C5:h", NULL);
    MotiveConnection changed;
    assert(motive_connection(&basic, &basic, &other_rhythm, &(MotiveScore){0, 1}, &changed));
    assert(changed.total.numerator * same.total.denominator < same.total.numerator * changed.total.denominator);
    MotiveConnection far;
    assert(motive_connection(&basic, &basic, &basic, &(MotiveScore){2, 3}, &far));
    assert(equals(far.common, 1, 3) && far.total.numerator * same.total.denominator < same.total.numerator * far.total.denominator);
}

/* A remote form, over 3/4 from the basic motive, is halved. */
static void test_remote_form_is_halved(void) {
    Motive basic = load("C4:q E4:q G4:q C5:q", NULL);
    MotiveConnection near;
    MotiveConnection remote;
    assert(motive_connection(&basic, &basic, &basic, &(MotiveScore){3, 4}, &near));
    assert(!near.halved && equals(near.common, 1, 4) && equals(near.total, 1, 2));
    assert(motive_connection(&basic, &basic, &basic, &(MotiveScore){4, 5}, &remote));
    assert(remote.halved && equals(remote.common, 1, 5));
    /* (2/5 + 1 + 1/2) / 4 = 19/40, halved 19/80. */
    assert(equals(remote.total, 19, 80));
    /* No chain within the depth is distance 1: nothing in common. */
    assert(motive_connection(&basic, &basic, &basic, &(MotiveScore){1, 1}, &remote));
    assert(remote.halved && equals(remote.common, 0, 1) && equals(remote.total, 3, 16));
}

/* The harmony part: the idea before's last chord to the candidate's first. */
static void test_harmony_progression(void) {
    Motive basic = load("C4:q E4:q G4:q C5:q", NULL);
    static const struct {
        int from;
        int to;
        unsigned long numerator;
        unsigned long denominator;
    } CASES[] = {{5, 1, 1, 1},  /* V to I */
                 {1, 5, 1, 1},  /* I to V */
                 {1, 2, 1, 1},  /* I to ii */
                 {2, 5, 1, 1},  /* ii to V */
                 {5, 5, 1, 2},  /* a repeated chord */
                 {5, 2, 0, 1},  /* V to ii goes back */
                 {2, 1, 0, 1}}; /* ii to I is not a progression the phrase model allows */
    for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
        char from[4];
        char to[4];
        snprintf(from, sizeof(from), "%d", CASES[i].from);
        snprintf(to, sizeof(to), "%d", CASES[i].to);
        Motive before = load("G4:q B4:q D5:q G5:q", from);
        Motive after = load("G4:q B4:q D5:q G5:q", to);
        MotiveConnection connection;
        assert(motive_connection(&basic, &before, &after, &(MotiveScore){0, 1}, &connection));
        assert(equals(connection.harmony, CASES[i].numerator, CASES[i].denominator));
    }
}

/* Without a key a motive has no chord, so nothing is coherent. */
static void test_no_chord_no_harmony(void) {
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = motive_parse_meter("4/4", &options.meter);
    Motive rhythm;
    assert(motive_from_rhythm_text("x:q x:q x:q x:q", &options, &rhythm, NULL) == MOTIVE_OK);
    rhythm.key_source = MOTIVE_SKETCH_KEY_UNSPECIFIED;
    Motive basic = load("C4:q E4:q G4:q C5:q", NULL);
    MotiveConnection connection;
    assert(motive_connection(&basic, &basic, &rhythm, &(MotiveScore){1, 3}, &connection));
    assert(equals(connection.harmony, 0, 1));
}

/* The distance of a chain: its relations' costs over the sum plus 4. */
static void test_chain_distance(void) {
    VariantChain chain;
    MotiveScore distance;
    memset(&chain, 0, sizeof(chain));
    assert(motive_chain_distance(&chain, &distance) && equals(distance, 0, 1));
    assert(variant_step_init(&chain.steps[0], "transpose") && variant_step_init(&chain.steps[1], "invert"));
    chain.step_count = 1;
    assert(motive_chain_distance(&chain, &distance) && equals(distance, 1, 5));
    chain.step_count = 2;
    assert(motive_chain_distance(&chain, &distance) && equals(distance, 3, 7));
    strcpy(chain.steps[1].op, "no-such-row");
    assert(!motive_chain_distance(&chain, &distance));
    assert(!motive_chain_distance(NULL, &distance) && !motive_chain_distance(&chain, NULL));
    (void)fraction;
}

static void test_bad_arguments(void) {
    Motive basic = load("C4:q E4:q G4:q C5:q", NULL);
    MotiveConnection connection;
    MotiveScore score;
    assert(!motive_connection(NULL, &basic, &basic, &(MotiveScore){0, 1}, &connection));
    assert(!motive_connection(&basic, &basic, NULL, &(MotiveScore){0, 1}, &connection));
    assert(!motive_connection(&basic, &basic, &basic, &(MotiveScore){0, 1}, NULL));
    assert(!motive_connection_score(&basic, &basic, &basic, NULL));
    /* previous NULL is the basic idea. */
    assert(motive_connection(&basic, NULL, &basic, &(MotiveScore){0, 1}, &connection) && equals(connection.total, 7, 8));
    (void)score;
}

int main(void) {
    test_same_form_scores_highest();
    test_remote_form_is_halved();
    test_harmony_progression();
    test_no_chord_no_harmony();
    test_chain_distance();
    test_bad_arguments();
    puts("test_motive_connect: ok");
    return 0;
}
