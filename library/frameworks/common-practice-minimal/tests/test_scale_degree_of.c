#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "outer_scale_degrees.h"

/*
 * Scale degree number tests: all 12 pitch classes in major and minor over every tonic agree with the label, and
 * invalid arguments leave the outputs untouched.
 */

static void test_every_pitch_class_matches_the_label(void) {
    static const char SYMBOLS[3] = {'b', '\0', '#'};
    for (int mode = COMMON_PRACTICE_KEY_MAJOR; mode <= COMMON_PRACTICE_KEY_MINOR; mode++) {
        for (int tonic = 0; tonic < 12; tonic++) {
            for (int pc = 0; pc < 12; pc++) {
                int degree = 0;
                int alteration = 9;
                assert(common_practice_scale_degree_of(pc, tonic, (CommonPracticeKeyMode)mode, &degree, &alteration));
                assert(degree >= 1 && degree <= 7 && alteration >= -1 && alteration <= 1);
                char expected[3];
                size_t at = 0;
                if (alteration != 0) {
                    expected[at++] = SYMBOLS[alteration + 1];
                }
                expected[at++] = (char)('0' + degree);
                expected[at] = '\0';
                const char *label = common_practice_scale_degree_label(pc, tonic, (CommonPracticeKeyMode)mode);
                assert(label != NULL && strcmp(label, expected) == 0);
            }
        }
    }
}

static void test_known_degrees(void) {
    int degree = 0;
    int alteration = 0;
    /* C major: F# is #4, Bb is b7. */
    assert(common_practice_scale_degree_of(6, 0, COMMON_PRACTICE_KEY_MAJOR, &degree, &alteration));
    assert(degree == 4 && alteration == 1);
    assert(common_practice_scale_degree_of(10, 0, COMMON_PRACTICE_KEY_MAJOR, &degree, &alteration));
    assert(degree == 7 && alteration == -1);
    /* A minor: G is 7, G# is #7, F# is #6. */
    assert(common_practice_scale_degree_of(7, 9, COMMON_PRACTICE_KEY_MINOR, &degree, &alteration));
    assert(degree == 7 && alteration == 0);
    assert(common_practice_scale_degree_of(8, 9, COMMON_PRACTICE_KEY_MINOR, &degree, &alteration));
    assert(degree == 7 && alteration == 1);
    assert(common_practice_scale_degree_of(6, 9, COMMON_PRACTICE_KEY_MINOR, &degree, &alteration));
    assert(degree == 6 && alteration == 1);
}

static void test_invalid_arguments(void) {
    int degree = 42;
    int alteration = 43;
    assert(!common_practice_scale_degree_of(-1, 0, COMMON_PRACTICE_KEY_MAJOR, &degree, &alteration));
    assert(!common_practice_scale_degree_of(12, 0, COMMON_PRACTICE_KEY_MAJOR, &degree, &alteration));
    assert(!common_practice_scale_degree_of(0, -1, COMMON_PRACTICE_KEY_MAJOR, &degree, &alteration));
    assert(!common_practice_scale_degree_of(0, 12, COMMON_PRACTICE_KEY_MAJOR, &degree, &alteration));
    assert(!common_practice_scale_degree_of(0, 0, (CommonPracticeKeyMode)5, &degree, &alteration));
    assert(!common_practice_scale_degree_of(0, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &alteration));
    assert(!common_practice_scale_degree_of(0, 0, COMMON_PRACTICE_KEY_MAJOR, &degree, NULL));
    assert(degree == 42 && alteration == 43);
    assert(common_practice_scale_degree_label(0, 0, (CommonPracticeKeyMode)5) == NULL);
}

int main(void) {
    test_every_pitch_class_matches_the_label();
    test_known_degrees();
    test_invalid_arguments();
    printf("All scale degree number tests passed.\n");
    return 0;
}
