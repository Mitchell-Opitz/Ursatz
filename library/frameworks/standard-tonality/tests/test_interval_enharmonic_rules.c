#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "interval_enharmonic_rules.h"
#include "spelling_test_util.h"

typedef StandardTonalityIntervalQuality Quality;

#define P STANDARD_TONALITY_QUALITY_PERFECT
#define MA STANDARD_TONALITY_QUALITY_MAJOR
#define MI STANDARD_TONALITY_QUALITY_MINOR
#define AUG STANDARD_TONALITY_QUALITY_AUGMENTED
#define DIM STANDARD_TONALITY_QUALITY_DIMINISHED

typedef struct {
    int size;
    Quality quality;
} Cell;

/* Example 16: the intervals with each number of half steps. */
static const struct {
    int semitones;
    Cell cells[2];
} CHART[13] = {
    {0, {{1, P}, {2, DIM}}},   {1, {{1, AUG}, {2, MI}}},  {2, {{2, MA}, {3, DIM}}},  {3, {{2, AUG}, {3, MI}}},
    {4, {{3, MA}, {4, DIM}}},  {5, {{3, AUG}, {4, P}}},   {6, {{4, AUG}, {5, DIM}}}, {7, {{5, P}, {6, DIM}}},
    {8, {{5, AUG}, {6, MI}}},  {9, {{6, MA}, {7, DIM}}},  {10, {{6, AUG}, {7, MI}}}, {11, {{7, MA}, {8, DIM}}},
    {12, {{7, AUG}, {8, P}}},
};

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_INTERVAL_ENHARMONIC_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_interval_enharmonic_rule((StandardTonalityIntervalEnharmonicRule)i);
        const Constraint *constraint =
            standard_tonality_interval_enharmonic_constraint((StandardTonalityIntervalEnharmonicRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_interval_enharmonic_rule(STANDARD_TONALITY_INTERVAL_ENHARMONIC_RULE_COUNT) == NULL);
}

/* Every cell of the chart has its row's number of half steps, and
   looking up that count for the size gives the cell back. */
static void test_chart_cells(void) {
    for (size_t row = 0; row < 13; row++) {
        for (size_t c = 0; c < 2; c++) {
            Cell cell = CHART[row].cells[c];
            int semitones = 0;
            assert(standard_tonality_interval_semitones(cell.size, cell.quality, &semitones));
            assert(semitones == CHART[row].semitones);

            Quality found = (Quality)42;
            assert(standard_tonality_interval_at_semitones(cell.size, CHART[row].semitones, &found));
            assert(found == cell.quality);
        }
    }
}

/* Each row is enharmonically equivalent; different rows are not. */
static void test_rows_are_equivalent(void) {
    for (size_t row = 0; row < 13; row++) {
        assert(standard_tonality_intervals_enharmonic(CHART[row].cells[0].size, CHART[row].cells[0].quality,
                                                      CHART[row].cells[1].size, CHART[row].cells[1].quality));
        assert(standard_tonality_intervals_enharmonic(CHART[row].cells[1].size, CHART[row].cells[1].quality,
                                                      CHART[row].cells[0].size, CHART[row].cells[0].quality));
        for (size_t other = 0; other < 13; other++) {
            if (other == row) {
                continue;
            }
            assert(!standard_tonality_intervals_enharmonic(CHART[row].cells[0].size, CHART[row].cells[0].quality,
                                                           CHART[other].cells[1].size, CHART[other].cells[1].quality));
        }
    }
}

/* The chart is complete: every singly altered simple interval with 0 to
   12 half steps appears in it, and nothing else does. */
static void test_chart_is_complete(void) {
    Quality qualities[] = {DIM, MI, P, MA, AUG};
    int found_in_chart = 0;
    for (int size = 1; size <= 8; size++) {
        for (size_t q = 0; q < 5; q++) {
            int semitones = 0;
            if (!standard_tonality_interval_semitones(size, qualities[q], &semitones)) {
                continue; /* the quality does not fit this size */
            }
            if (semitones < 0 || semitones > 12) {
                continue;
            }
            bool listed = false;
            for (size_t c = 0; c < 2; c++) {
                Cell cell = CHART[semitones].cells[c];
                if (cell.size == size && cell.quality == qualities[q]) {
                    listed = true;
                }
            }
            assert(listed);
            found_in_chart++;
        }
    }
    assert(found_in_chart == 26);
}

/* The source's examples: ma2 and d3, A4 and d5. */
static void test_source_examples(void) {
    assert(standard_tonality_intervals_enharmonic(2, MA, 3, DIM));
    assert(standard_tonality_intervals_enharmonic(4, AUG, 5, DIM));
    assert(standard_tonality_intervals_enharmonic(1, P, 2, DIM));
    assert(standard_tonality_intervals_enharmonic(7, AUG, 8, P));
}

static void test_not_enharmonic(void) {
    assert(!standard_tonality_intervals_enharmonic(3, MA, 3, MA)); /* the same interval */
    assert(!standard_tonality_intervals_enharmonic(3, MA, 3, MI));
    assert(!standard_tonality_intervals_enharmonic(5, MA, 4, DIM)); /* no major fifth */
    assert(!standard_tonality_intervals_enharmonic(0, P, 2, DIM));
}

/* A compound interval is enharmonic to another with the same half steps. */
static void test_compound(void) {
    assert(standard_tonality_intervals_enharmonic(9, MA, 10, DIM)); /* ninth and tenth, 14 half steps */
    assert(!standard_tonality_intervals_enharmonic(9, MA, 3, DIM)); /* an octave apart */
}

static void test_at_semitones_rejects_invalid_arguments(void) {
    Quality quality = (Quality)42;
    assert(!standard_tonality_interval_at_semitones(0, 3, &quality));
    assert(!standard_tonality_interval_at_semitones(5, 20, &quality)); /* beyond triply augmented */
    assert(!standard_tonality_interval_at_semitones(5, -10, &quality));
    assert(!standard_tonality_interval_at_semitones(5, 7, NULL));
    assert(quality == (Quality)42);
}

static void test_spellings_enharmonic(void) {
    PitchSpelling *c_sharp = spell(PITCH_LETTER_C, 1, 4);
    PitchSpelling *d_flat = spell(PITCH_LETTER_D, -1, 4);
    PitchSpelling *c = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *d = spell(PITCH_LETTER_D, 0, 4);
    PitchSpelling *b_sharp3 = spell(PITCH_LETTER_B, 1, 3);
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *e_double_flat3 = spell(PITCH_LETTER_E, -2, 3);
    PitchSpelling *d3 = spell(PITCH_LETTER_D, 0, 3);
    PitchSpelling *a_flat3 = spell(PITCH_LETTER_A, -1, 3);
    PitchSpelling *g_sharp3 = spell(PITCH_LETTER_G, 1, 3);

    assert(standard_tonality_spellings_enharmonic(c_sharp, d_flat));
    assert(standard_tonality_spellings_enharmonic(b_sharp3, c4)); /* crosses an octave number */
    assert(standard_tonality_spellings_enharmonic(e_double_flat3, d3));
    assert(standard_tonality_spellings_enharmonic(a_flat3, g_sharp3));
    assert(!standard_tonality_spellings_enharmonic(c, c));
    assert(!standard_tonality_spellings_enharmonic(c, d));
    assert(!standard_tonality_spellings_enharmonic(c, NULL));

    pitch_spelling_destroy(c_sharp);
    pitch_spelling_destroy(d_flat);
    pitch_spelling_destroy(c);
    pitch_spelling_destroy(d);
    pitch_spelling_destroy(b_sharp3);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(e_double_flat3);
    pitch_spelling_destroy(d3);
    pitch_spelling_destroy(a_flat3);
    pitch_spelling_destroy(g_sharp3);
}

/* Example 17: E-double-flat up to A-flat is read as D up to G-sharp, an
   augmented fourth. */
static void test_example_17(void) {
    PitchSpelling *e_double_flat3 = spell(PITCH_LETTER_E, -2, 3);
    PitchSpelling *a_flat3 = spell(PITCH_LETTER_A, -1, 3);
    int size = 0;
    Quality quality = P;
    assert(standard_tonality_interval_quality_via_enharmonic(e_double_flat3, a_flat3, &size, &quality));
    assert(size == 4 && quality == AUG);

    PitchSpelling *d3 = spell(PITCH_LETTER_D, 0, 3);
    PitchSpelling *g_sharp3 = spell(PITCH_LETTER_G, 1, 3);
    assert(standard_tonality_interval_quality_via_enharmonic(d3, g_sharp3, &size, &quality));
    assert(size == 4 && quality == AUG);

    pitch_spelling_destroy(e_double_flat3);
    pitch_spelling_destroy(a_flat3);
    pitch_spelling_destroy(d3);
    pitch_spelling_destroy(g_sharp3);
}

/* Property: replacing the notes enharmonically gives the same answer as
   identifying the interval as written. */
static void test_enharmonic_agrees_with_direct_identification(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int compared = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int lower_accidental = -2; lower_accidental <= 2; lower_accidental++) {
            PitchSpelling *lower = spell(letters[l], lower_accidental, 3);
            for (size_t u = 0; u < 8; u++) {
                for (int upper_accidental = -2; upper_accidental <= 2; upper_accidental++) {
                    PitchSpelling *upper = spell(letters[(l + u) % 7], upper_accidental, 3 + (int)((l + u) / 7));

                    int direct_size = 0;
                    int enharmonic_size = 0;
                    Quality direct_quality = P;
                    Quality enharmonic_quality = P;
                    bool direct = standard_tonality_interval_quality(lower, upper, &direct_size, &direct_quality);
                    bool enharmonic = standard_tonality_interval_quality_via_enharmonic(lower, upper, &enharmonic_size,
                                                                                         &enharmonic_quality);
                    if (direct && enharmonic) {
                        assert(direct_size == enharmonic_size);
                        assert(direct_quality == enharmonic_quality);
                        compared++;
                    }
                    pitch_spelling_destroy(upper);
                }
            }
            pitch_spelling_destroy(lower);
        }
    }
    assert(compared > 500);
}

static void test_via_enharmonic_rejects_invalid_arguments(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *b3 = spell(PITCH_LETTER_B, 0, 3);
    int size = 42;
    Quality quality = (Quality)42;
    assert(!standard_tonality_interval_quality_via_enharmonic(c4, b3, &size, &quality)); /* reversed */
    assert(!standard_tonality_interval_quality_via_enharmonic(NULL, c4, &size, &quality));
    assert(!standard_tonality_interval_quality_via_enharmonic(c4, NULL, &size, &quality));
    assert(!standard_tonality_interval_quality_via_enharmonic(c4, c4, NULL, &quality));
    assert(!standard_tonality_interval_quality_via_enharmonic(c4, c4, &size, NULL));
    assert(size == 42 && quality == (Quality)42);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(b3);
}

int main(void) {
    test_rules_reference_framework();
    test_chart_cells();
    test_rows_are_equivalent();
    test_chart_is_complete();
    test_source_examples();
    test_not_enharmonic();
    test_compound();
    test_at_semitones_rejects_invalid_arguments();
    test_spellings_enharmonic();
    test_example_17();
    test_enharmonic_agrees_with_direct_identification();
    test_via_enharmonic_rejects_invalid_arguments();
    printf("test_interval_enharmonic_rules (standard-tonality): ok\n");
    return 0;
}
