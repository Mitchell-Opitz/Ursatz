#include <assert.h>
#include <stdio.h>

#include "tonal_key.h"

/*
 * standard_tonality_degree_of_semitone for the four modes, and the comparison with the Composer's
 * tables (melody_check.c pitch_degree, counterpoint_write.c scale_degree). Where they differ the
 * difference is written here as the record P25 declares.
 */

static int floor_mod12(int value) {
    return ((value % 12) + 12) % 12;
}

static void test_scale_notes(void) {
    for (int mode = 0; mode <= STANDARD_TONALITY_MODE_MELODIC_MINOR; mode++) {
        for (int degree = 1; degree <= 7; degree++) {
            int semitone = standard_tonality_degree_semitone((StandardTonalityMode)mode, degree);
            int got = 0;
            int alteration = 9;
            assert(standard_tonality_degree_of_semitone(semitone, (StandardTonalityMode)mode, &got, &alteration));
            assert(got == degree && alteration == 0);
            /* Octaves above and below read the same. */
            assert(standard_tonality_degree_of_semitone(semitone + 24, (StandardTonalityMode)mode, &got, &alteration));
            assert(got == degree && alteration == 0);
            assert(standard_tonality_degree_of_semitone(semitone - 12, (StandardTonalityMode)mode, &got, &alteration));
            assert(got == degree && alteration == 0);
        }
    }
}

static void test_chromatic_notes(void) {
    int degree;
    int alteration;
    /* Major: semitones 1, 3, 8, 10 are the next degree lowered; 6 is the fourth raised. */
    assert(standard_tonality_degree_of_semitone(1, STANDARD_TONALITY_MODE_MAJOR, &degree, &alteration));
    assert(degree == 2 && alteration == -1);
    assert(standard_tonality_degree_of_semitone(3, STANDARD_TONALITY_MODE_MAJOR, &degree, &alteration));
    assert(degree == 3 && alteration == -1);
    assert(standard_tonality_degree_of_semitone(6, STANDARD_TONALITY_MODE_MAJOR, &degree, &alteration));
    assert(degree == 4 && alteration == 1);
    assert(standard_tonality_degree_of_semitone(8, STANDARD_TONALITY_MODE_MAJOR, &degree, &alteration));
    assert(degree == 6 && alteration == -1);
    assert(standard_tonality_degree_of_semitone(10, STANDARD_TONALITY_MODE_MAJOR, &degree, &alteration));
    assert(degree == 7 && alteration == -1);
    /* Natural minor: the leading tone is the seventh raised. */
    assert(standard_tonality_degree_of_semitone(1, STANDARD_TONALITY_MODE_NATURAL_MINOR, &degree, &alteration));
    assert(degree == 2 && alteration == -1);
    assert(standard_tonality_degree_of_semitone(11, STANDARD_TONALITY_MODE_NATURAL_MINOR, &degree, &alteration));
    assert(degree == 7 && alteration == 1);
    assert(standard_tonality_degree_of_semitone(4, STANDARD_TONALITY_MODE_NATURAL_MINOR, &degree, &alteration));
    assert(degree == 3 && alteration == 1);
    /* Harmonic minor: the minor third between its sixth and seventh. */
    assert(standard_tonality_degree_of_semitone(9, STANDARD_TONALITY_MODE_HARMONIC_MINOR, &degree, &alteration));
    assert(degree == 6 && alteration == 1);
    assert(standard_tonality_degree_of_semitone(10, STANDARD_TONALITY_MODE_HARMONIC_MINOR, &degree, &alteration));
    assert(degree == 6 && alteration == 2);
    /* Melodic minor: its tenth semitone is the sixth raised. */
    assert(standard_tonality_degree_of_semitone(10, STANDARD_TONALITY_MODE_MELODIC_MINOR, &degree, &alteration));
    assert(degree == 6 && alteration == 1);
}

static void test_every_semitone_resolves(void) {
    for (int mode = 0; mode <= STANDARD_TONALITY_MODE_MELODIC_MINOR; mode++) {
        for (int s = -24; s <= 24; s++) {
            int degree = 0;
            int alteration = -1;
            assert(standard_tonality_degree_of_semitone(s, (StandardTonalityMode)mode, &degree, &alteration));
            assert(degree >= 1 && degree <= 7 && alteration >= -1 && alteration <= 2);
            assert(standard_tonality_degree_semitone((StandardTonalityMode)mode, degree) + alteration ==
                   floor_mod12(s));
        }
    }
}

/* The Composer's tables, copied: melody_check.c pitch_degree and counterpoint_write.c scale_degree
   (0 = not in the scale). */
static const int COMPOSER_MAJOR[12] = {1, 2, 2, 3, 3, 4, 4, 5, 6, 6, 7, 7};
static const int COMPOSER_MINOR[12] = {1, 2, 2, 3, 3, 4, 5, 5, 6, 6, 7, 7};
static const int COMPOSER_WRITE_MAJOR[12] = {1, 0, 2, 0, 3, 4, 0, 5, 0, 6, 0, 7};
static const int COMPOSER_WRITE_MINOR[12] = {1, 0, 2, 3, 0, 4, 0, 5, 6, 6, 7, 7};

static void test_composer_tables(void) {
    int degree;
    int alteration;
    for (int s = 0; s < 12; s++) {
        /* scale_degree (write): a note the scale has reads as its own degree, and the library gives
           the same degree with no alteration. The minor table is natural with the raised sixth and
           seventh, so it is read in the melodic mode for the raised notes and the natural for the rest. */
        if (COMPOSER_WRITE_MAJOR[s] != 0) {
            assert(standard_tonality_degree_of_semitone(s, STANDARD_TONALITY_MODE_MAJOR, &degree, &alteration));
            assert(degree == COMPOSER_WRITE_MAJOR[s] && alteration == 0);
        }
        if (COMPOSER_WRITE_MINOR[s] != 0) {
            int natural_degree, natural_alteration, melodic_degree, melodic_alteration;
            assert(standard_tonality_degree_of_semitone(s, STANDARD_TONALITY_MODE_NATURAL_MINOR, &natural_degree,
                                                        &natural_alteration));
            assert(standard_tonality_degree_of_semitone(s, STANDARD_TONALITY_MODE_MELODIC_MINOR, &melodic_degree,
                                                        &melodic_alteration));
            assert((natural_alteration == 0 && natural_degree == COMPOSER_WRITE_MINOR[s]) ||
                   (melodic_alteration == 0 && melodic_degree == COMPOSER_WRITE_MINOR[s]));
        }
    }
    /* pitch_degree (melody_check.c) against the library, semitone by semitone. The Composer reads a
       chromatic note as the degree it is spelled for (flats up, F# as a raised fourth). Since R-285
       the library agrees on every major semitone and on natural minor 1; the one difference left is
       natural minor 6 (the Composer reads the fifth, the library the fourth raised). */
    unsigned major_differs = 0;
    unsigned minor_differs = 0;
    for (int s = 0; s < 12; s++) {
        assert(standard_tonality_degree_of_semitone(s, STANDARD_TONALITY_MODE_MAJOR, &degree, &alteration));
        if (degree != COMPOSER_MAJOR[s]) {
            major_differs |= 1u << s;
            assert(alteration == 1 && COMPOSER_MAJOR[s] == degree + 1);
        }
        assert(standard_tonality_degree_of_semitone(s, STANDARD_TONALITY_MODE_NATURAL_MINOR, &degree, &alteration));
        if (degree != COMPOSER_MINOR[s]) {
            minor_differs |= 1u << s;
            assert(alteration == 1 && COMPOSER_MINOR[s] == degree + 1);
        }
    }
    assert(major_differs == 0);
    assert(minor_differs == (1u << 6));
}

static void test_invalid(void) {
    int degree = 99;
    int alteration = 99;
    assert(!standard_tonality_degree_of_semitone(0, STANDARD_TONALITY_MODE_MAJOR, NULL, &alteration));
    assert(!standard_tonality_degree_of_semitone(0, STANDARD_TONALITY_MODE_MAJOR, &degree, NULL));
    assert(!standard_tonality_degree_of_semitone(0, (StandardTonalityMode)99, &degree, &alteration));
    assert(degree == 99 && alteration == 99);
}

int main(void) {
    test_scale_notes();
    test_chromatic_notes();
    test_every_semitone_resolves();
    test_composer_tables();
    test_invalid();
    printf("standard-tonality degree of semitone tests passed\n");
    return 0;
}
