#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fifth_species_pattern.h"
#include "fifth_species_test_util.h"

#define BAR_PATTERN(name) STANDARD_COUNTERPOINT_BAR_PATTERN_##name
#define VALUE(name) STANDARD_COUNTERPOINT_NOTE_VALUE_##name

static void test_note_values(void) {
    assert(strcmp(standard_counterpoint_note_value_name(VALUE(EIGHTH)), "eighth") == 0);
    assert(strcmp(standard_counterpoint_note_value_name(VALUE(QUARTER)), "quarter") == 0);
    assert(strcmp(standard_counterpoint_note_value_name(VALUE(HALF)), "half") == 0);
    assert(strcmp(standard_counterpoint_note_value_name(VALUE(DOTTED_HALF)), "dotted-half") == 0);
    assert(strcmp(standard_counterpoint_note_value_name(VALUE(WHOLE)), "whole") == 0);
    assert(standard_counterpoint_note_value_name(VALUE(COUNT)) == NULL);

    assert(standard_counterpoint_note_value_eighths(VALUE(EIGHTH)) == 1);
    assert(standard_counterpoint_note_value_eighths(VALUE(DOTTED_HALF)) == 6);
    assert(standard_counterpoint_note_value_eighths(VALUE(WHOLE)) == 8);
    assert(standard_counterpoint_note_value_eighths(VALUE(COUNT)) == 0);

    StandardCounterpointNoteValue value = VALUE(WHOLE);
    assert(standard_counterpoint_note_value_from_eighths(2, &value) && value == VALUE(QUARTER));
    assert(standard_counterpoint_note_value_from_eighths(6, &value) && value == VALUE(DOTTED_HALF));
    assert(!standard_counterpoint_note_value_from_eighths(3, &value) && value == VALUE(DOTTED_HALF));
    assert(!standard_counterpoint_note_value_from_eighths(1, NULL));
}

static void test_bar_pattern_names(void) {
    assert(strcmp(standard_counterpoint_bar_pattern_name(BAR_PATTERN(WHOLE)), "whole") == 0);
    assert(strcmp(standard_counterpoint_bar_pattern_name(BAR_PATTERN(HALVES)), "halves") == 0);
    assert(strcmp(standard_counterpoint_bar_pattern_name(BAR_PATTERN(HALF_QUARTERS)), "half-quarters") == 0);
    assert(strcmp(standard_counterpoint_bar_pattern_name(BAR_PATTERN(QUARTERS_HALF)), "quarters-half") == 0);
    assert(strcmp(standard_counterpoint_bar_pattern_name(BAR_PATTERN(QUARTERS)), "quarters") == 0);
    assert(strcmp(standard_counterpoint_bar_pattern_name(BAR_PATTERN(EIGHTHS)), "eighths") == 0);
    assert(strcmp(standard_counterpoint_bar_pattern_name(BAR_PATTERN(SYNCOPATED)), "syncopated") == 0);
    assert(strcmp(standard_counterpoint_bar_pattern_name(BAR_PATTERN(REST_START)), "rest-start") == 0);
    assert(strcmp(standard_counterpoint_bar_pattern_name(BAR_PATTERN(OTHER)), "other") == 0);
    assert(standard_counterpoint_bar_pattern_name(BAR_PATTERN(COUNT)) == NULL);
}

static void test_good_exercise_patterns(void) {
    GOOD_FIFTH_SPECIES_CANTUS(cantus);
    GOOD_FIFTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    const StandardCounterpointBarPattern expected[] = {
        BAR_PATTERN(REST_START), BAR_PATTERN(SYNCOPATED),     BAR_PATTERN(HALF_QUARTERS), BAR_PATTERN(SYNCOPATED),
        BAR_PATTERN(WHOLE),      BAR_PATTERN(QUARTERS),       BAR_PATTERN(EIGHTHS),       BAR_PATTERN(HALF_QUARTERS),
        BAR_PATTERN(HALF_QUARTERS), BAR_PATTERN(SYNCOPATED), BAR_PATTERN(WHOLE)};
    for (size_t bar = 0; bar < 11; bar++) {
        StandardCounterpointBarPattern pattern = BAR_PATTERN(OTHER);
        assert(standard_counterpoint_fifth_species_bar_pattern(&exercise, bar, &pattern));
        assert(pattern == expected[bar]);
    }
    StandardCounterpointBarPattern pattern = BAR_PATTERN(WHOLE);
    assert(!standard_counterpoint_fifth_species_bar_pattern(&exercise, 11, &pattern));
    assert(pattern == BAR_PATTERN(WHOLE));
    assert(!standard_counterpoint_fifth_species_bar_pattern(NULL, 0, &pattern));
    assert(!standard_counterpoint_fifth_species_bar_pattern(&exercise, 0, NULL));
}

static void test_other_patterns(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {
        WHOLE(P(E, 0, 3)),
        HALF(P(E, 0, 3)), HALF(P(G, 0, 3)),
        QUARTER(P(E, 0, 3)), QUARTER(P(G, 0, 3)), HALF(P(E, 0, 3)),
        HALF(P(E, 0, 3)), QUARTER(P(G, 0, 3)), QUARTER(P(E, 0, 3)),
        ATK(P(G, 0, 3)), T5(P(G, 0, 3)), QUARTER(P(E, 0, 3)),
        WHOLE(P(C, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_exercise_valid(&exercise));
    const StandardCounterpointBarPattern expected[] = {BAR_PATTERN(WHOLE),         BAR_PATTERN(HALVES),
                                                       BAR_PATTERN(QUARTERS_HALF), BAR_PATTERN(HALF_QUARTERS),
                                                       BAR_PATTERN(OTHER),         BAR_PATTERN(WHOLE)};
    for (size_t bar = 0; bar < 6; bar++) {
        StandardCounterpointBarPattern pattern;
        assert(standard_counterpoint_fifth_species_bar_pattern(&exercise, bar, &pattern));
        assert(pattern == expected[bar]);
    }
}

static void test_value_counts(void) {
    GOOD_FIFTH_SPECIES_CANTUS(cantus);
    GOOD_FIFTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    size_t counts[STANDARD_COUNTERPOINT_NOTE_VALUE_COUNT] = {99, 99, 99, 99, 99};

    assert(standard_counterpoint_fifth_species_value_counts(&exercise, counts));
    assert(counts[VALUE(EIGHTH)] == 2);
    assert(counts[VALUE(QUARTER)] == 12);
    assert(counts[VALUE(HALF)] == 7);
    assert(counts[VALUE(DOTTED_HALF)] == 1);
    assert(counts[VALUE(WHOLE)] == 3);

    counts[0] = 99;
    assert(!standard_counterpoint_fifth_species_value_counts(NULL, counts));
    assert(!standard_counterpoint_fifth_species_value_counts(&exercise, NULL));
    assert(counts[0] == 99);
}

int main(void) {
    test_note_values();
    test_bar_pattern_names();
    test_good_exercise_patterns();
    test_other_patterns();
    test_value_counts();
    printf("test_fifth_species_pattern (standard-counterpoint): ok\n");
    return 0;
}
