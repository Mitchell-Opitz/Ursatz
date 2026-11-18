#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "theory/style/style_labels.h"

#include "style/style_profile_internal.h"
#include "ursatz_test.h"

/* The tempo and density bands and the motive label builders the Composer shares with the style profile. */

#define CHECK_TEXT(actual, expected) \
    CHECKF(strcmp((actual), (expected)) == 0, "got '%s', expected '%s'", (actual), (expected))

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

    return test_finish("test_style_labels");
}
