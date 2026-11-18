#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "theory/style/style_labels.h"

#include "ursatz_test.h"

/* The labels a style profile counts (theory/style/style_labels.h): numerals, intervals, durations, contour, progressions, the
   categories and dimensions, and the tempo and density bands and the motive label builders the Composer shares with the profile. */

#define CHECK_TEXT(actual, expected) \
    CHECKF(strcmp((actual), (expected)) == 0, "got '%s', expected '%s'", (actual), (expected))

static void check_numeral(const char *input, const char *expected) {
    char *label = style_label_numeral(input);
    CHECKF((label == NULL) == (expected == NULL) && (!label || strcmp(label, expected) == 0), "numeral '%s' gives '%s', expected '%s'",
           input, label ? label : "(none)", expected ? expected : "(none)");
    free(label);
}

static void check_interval(int semitones, const char *expected) {
    char label[16];
    style_label_interval(semitones, label, sizeof(label));
    CHECKF(strcmp(label, expected) == 0, "interval %d gives '%s', expected '%s'", semitones, label, expected);
}

static void check_duration(unsigned long numerator, unsigned long denominator, const char *expected) {
    char label[48];
    style_label_duration(numerator, denominator, label, sizeof(label));
    CHECKF(strcmp(label, expected) == 0, "duration %lu/%lu gives '%s', expected '%s'", numerator, denominator, label, expected);
}

static void test_labels(void) {
    check_numeral("V", "V");
    check_numeral("V7", "V7");
    check_numeral("V6/5", "V7");
    check_numeral("V4/3", "V7");
    check_numeral("V4/2", "V7");
    check_numeral("I6/4", "I");
    check_numeral("I6", "I");
    check_numeral("ii6/5", "ii7");
    check_numeral("bVI", "bVI");
    check_numeral("viio6", "viio");
    check_numeral("V6/5/V", "V7/V");
    check_numeral("V/V", "V/V");
    check_numeral("", NULL);
    check_numeral("hello", NULL);
    check_numeral("V/", NULL);
    check_numeral("V/x", NULL);
    check_numeral("6/4", NULL);

    check_interval(0, "0");
    check_interval(2, "+2");
    check_interval(-5, "-5");
    check_interval(12, "+12");
    check_interval(-12, "-12");
    check_interval(13, "+13+");
    check_interval(-24, "-13+");

    check_duration(1, 8, "1/8");
    check_duration(2, 16, "1/8");
    check_duration(3, 16, "3/16");
    check_duration(4, 4, "1/1");
    check_duration(1, 0, "0/1");

    CHECKF(strcmp(style_label_contour(3), "up") == 0 && strcmp(style_label_contour(-1), "down") == 0 &&
               strcmp(style_label_contour(0), "same") == 0,
           "contour labels");

    const char *pair[] = {"ii", "V7", "I"};
    char *progression = style_label_progression(pair, 3);
    CHECKF(progression && strcmp(progression, "ii-V7-I") == 0, "progression label '%s'", progression ? progression : "(none)");
    free(progression);
    CHECKF(style_label_progression(pair, 0) == NULL, "empty progression has no label");

    StyleDimension dimension;
    CHECKF(style_category_dimension("chord-progression-3", &dimension) && dimension == STYLE_DIMENSION_HARMONY, "harmony category");
    CHECKF(style_category_dimension("syncopation-per-phrase", &dimension) && dimension == STYLE_DIMENSION_RHYTHM, "rhythm category");
    CHECKF(style_category_dimension("phrase-range", &dimension) && dimension == STYLE_DIMENSION_MELODY, "melody category");
    CHECKF(style_category_dimension("mode", &dimension) && dimension == STYLE_DIMENSION_FORM, "form category");
    CHECKF(style_category_dimension("motive-degrees", &dimension) && dimension == STYLE_DIMENSION_MELODY &&
               style_category_dimension("motive-intervals", &dimension) && dimension == STYLE_DIMENSION_MELODY &&
               style_category_dimension("motive-rhythm", &dimension) && dimension == STYLE_DIMENSION_MELODY &&
               style_category_dimension("motive-nct-figure", &dimension) && dimension == STYLE_DIMENSION_MELODY,
           "motive categories are melody");
    CHECKF(style_category_dimension("tempo", &dimension) && dimension == STYLE_DIMENSION_RHYTHM &&
               style_category_dimension("note-density", &dimension) && dimension == STYLE_DIMENSION_RHYTHM,
           "tempo and note density are rhythm");
    CHECKF(!style_category_dimension("no-such-category", &dimension), "unknown category");
    CHECKF(style_dimension_from_name("melody", &dimension) && dimension == STYLE_DIMENSION_MELODY, "dimension from name");
    CHECKF(!style_dimension_from_name("timbre", &dimension), "unknown dimension name");
}

static void check_tempo(double value, const char *expected) {
    char label[32];
    style_label_tempo(value, label, sizeof label);
    CHECK_TEXT(label, expected);
}

static void check_density(double value, const char *expected) {
    char label[32];
    style_label_density(value, label, sizeof label);
    CHECK_TEXT(label, expected);
}

static void check_owned(char *actual, const char *expected) {
    if (!actual) {
        CHECKF(false, "NULL, expected '%s'", expected);
        return;
    }
    CHECK_TEXT(actual, expected);
    free(actual);
}

int main(void) {
    test_labels();

    check_tempo(39.9, "<40");
    check_tempo(40, "40-59");
    check_tempo(59.99, "40-59");
    check_tempo(60, "60-79");
    check_tempo(140, "140-159");
    check_tempo(219.9, "200-219");
    check_tempo(220, "220+");
    check_tempo(400, "220+");

    check_density(0, "0");
    check_density(0.24, "0");
    check_density(0.25, "1/2");
    check_density(1, "1");
    check_density(1.25, "3/2");
    check_density(1.74, "3/2");
    check_density(3.9, "4+");
    check_density(4, "4+");

    const char *const degrees[12] = {"1", "b2", "2", "b3", "3", "4", "#4", "5", "b6", "6", "b7", "7"};
    for (int pc = 0; pc < 12; pc++) {
        CHECK_TEXT(style_label_degree(pc, 2), degrees[((pc - 2) % 12 + 12) % 12]);
    }
    CHECK_TEXT(style_label_degree(2, 2), "1");
    CHECK_TEXT(style_label_degree(1, 2), "7");
    CHECK_TEXT(style_label_degree(14, 2), "1");

    const char *const parts[3] = {"1", "b6", "5"};
    check_owned(style_label_join(parts, 3), "1 b6 5");
    check_owned(style_label_join(parts, 1), "1");
    if (style_label_join(parts, 0)) {
        CHECKF(false, "join of nothing");
    }

    const int steps[2] = {8, -1};
    const int unison[1] = {0};
    const int leap[1] = {15};
    check_owned(style_label_motive_intervals(steps, 2), "+8 -1");
    check_owned(style_label_motive_intervals(unison, 1), "0");
    check_owned(style_label_motive_intervals(leap, 1), "+15");
    if (style_label_motive_intervals(steps, 0)) {
        CHECKF(false, "intervals of nothing");
    }

    char figure[64];
    if (!style_label_nct_figure("1", "b6", "appoggiatura", "5", figure, sizeof figure)) {
        CHECKF(false, "figure did not fit");
    }
    CHECK_TEXT(figure, "1 b6:appoggiatura 5");
    style_label_nct_figure(NULL, "b6", "appoggiatura", NULL, figure, sizeof figure);
    CHECK_TEXT(figure, "- b6:appoggiatura -");
    if (style_label_nct_figure("1", "b6", "appoggiatura", "5", figure, 8)) {
        CHECKF(false, "a short buffer was accepted");
    }

    return test_finish("theory_style_labels");
}
