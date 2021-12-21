#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fifth_species_figure.h"
#include "fifth_species_test_util.h"

#define FIGURE(name) STANDARD_COUNTERPOINT_FIFTH_FIGURE_##name
#define SUSPENSION(name) STANDARD_COUNTERPOINT_SUSPENSION_##name

static bool figure_at(const StandardCounterpointFifthSpeciesExercise *exercise, size_t slot,
                      StandardCounterpointFifthSpeciesFigure *figure) {
    return standard_counterpoint_fifth_species_dissonance_figure(exercise, slot, figure);
}

static void test_names_and_non_chord_tones(void) {
    assert(strcmp(standard_counterpoint_fifth_species_figure_name(FIGURE(PASSING_TONE)), "passing-tone") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_figure_name(FIGURE(DOUBLE_PASSING_TONE)),
                  "double-passing-tone") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_figure_name(FIGURE(NEIGHBOR_TONE)), "neighbor-tone") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_figure_name(FIGURE(DOUBLE_NEIGHBOR)), "double-neighbor") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_figure_name(FIGURE(NOTA_CAMBIATA_DOWN)),
                  "nota-cambiata-down") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_figure_name(FIGURE(NOTA_CAMBIATA_UP)), "nota-cambiata-up") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_figure_name(FIGURE(SUSPENSION)), "suspension") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_figure_name(FIGURE(ANTICIPATION)), "anticipation") == 0);
    assert(strcmp(standard_counterpoint_fifth_species_figure_name(FIGURE(ANTICIPATION_TURN)), "anticipation-turn") ==
           0);
    assert(standard_counterpoint_fifth_species_figure_name(FIGURE(COUNT)) == NULL);

    assert(standard_counterpoint_fifth_species_figure_non_chord_tone(FIGURE(PASSING_TONE)) == NON_CHORD_TONE_PASSING);
    assert(standard_counterpoint_fifth_species_figure_non_chord_tone(FIGURE(DOUBLE_PASSING_TONE)) ==
           NON_CHORD_TONE_PASSING);
    assert(standard_counterpoint_fifth_species_figure_non_chord_tone(FIGURE(NEIGHBOR_TONE)) == NON_CHORD_TONE_NEIGHBOR);
    assert(standard_counterpoint_fifth_species_figure_non_chord_tone(FIGURE(DOUBLE_NEIGHBOR)) ==
           NON_CHORD_TONE_DOUBLE_NEIGHBOR);
    assert(standard_counterpoint_fifth_species_figure_non_chord_tone(FIGURE(NOTA_CAMBIATA_DOWN)) ==
           NON_CHORD_TONE_CHANGING_TONE);
    assert(standard_counterpoint_fifth_species_figure_non_chord_tone(FIGURE(NOTA_CAMBIATA_UP)) ==
           NON_CHORD_TONE_CHANGING_TONE);
    assert(standard_counterpoint_fifth_species_figure_non_chord_tone(FIGURE(SUSPENSION)) == NON_CHORD_TONE_SUSPENSION);
    assert(standard_counterpoint_fifth_species_figure_non_chord_tone(FIGURE(ANTICIPATION)) ==
           NON_CHORD_TONE_ANTICIPATION);
    assert(standard_counterpoint_fifth_species_figure_non_chord_tone(FIGURE(ANTICIPATION_TURN)) ==
           NON_CHORD_TONE_ANTICIPATION);
    assert(standard_counterpoint_fifth_species_figure_non_chord_tone(FIGURE(COUNT)) == NON_CHORD_TONE_COUNT);
}

static void test_good_exercise(void) {
    GOOD_FIFTH_SPECIES_CANTUS(cantus);
    GOOD_FIFTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointFifthSpeciesFigure figure = FIGURE(COUNT);

    /* A consonance has no figure; the first note, tied into bar 1, is one. */
    assert(!standard_counterpoint_fifth_species_slot_is_dissonant(&exercise, 4));
    assert(!standard_counterpoint_fifth_species_slot_is_dissonant(&exercise, 8));
    assert(!figure_at(&exercise, 4, &figure));
    assert(figure == FIGURE(COUNT));

    assert(standard_counterpoint_fifth_species_slot_is_dissonant(&exercise, 20));
    assert(figure_at(&exercise, 20, &figure) && figure == FIGURE(PASSING_TONE));
    assert(figure_at(&exercise, 21, &figure) && figure == FIGURE(PASSING_TONE));
    assert(figure_at(&exercise, 24, &figure) && figure == FIGURE(SUSPENSION));
    assert(figure_at(&exercise, 42, &figure) && figure == FIGURE(PASSING_TONE));
    assert(figure_at(&exercise, 55, &figure) && figure == FIGURE(NEIGHBOR_TONE));
    assert(figure_at(&exercise, 68, &figure) && figure == FIGURE(PASSING_TONE));
    assert(figure_at(&exercise, 72, &figure) && figure == FIGURE(SUSPENSION));

    assert(!figure_at(&exercise, 88, &figure));
    assert(!figure_at(NULL, 20, &figure));
    assert(!figure_at(&exercise, 20, NULL));
    assert(!standard_counterpoint_fifth_species_slot_is_dissonant(NULL, 20));
}

static void test_passing_tones(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    StandardCounterpointFifthSpeciesFigure figure;

    /* A quarter note on the third beat, filling in a third. */
    const StandardCounterpointSlot quarter[] = {REST4(), HALF(P(E, 0, 4)), HALF(P(E, 0, 4)), QUARTER(P(D, 0, 4)),
                                                QUARTER(P(C, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(quarter_exercise, cantus, quarter, CANTUS_LOWER);
    assert(figure_at(&quarter_exercise, 12, &figure) && figure == FIGURE(PASSING_TONE));

    /* The second eighth of a pair. */
    const StandardCounterpointSlot eighth[] = {REST4(), HALF(P(E, 0, 4)), QUARTER(P(E, 0, 4)), EIGHTH(P(E, 0, 4)),
                                               EIGHTH(P(D, 0, 4)), HALF(P(C, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(eighth_exercise, cantus, eighth, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_slot_is_dissonant(&eighth_exercise, 11));
    assert(figure_at(&eighth_exercise, 11, &figure) && figure == FIGURE(PASSING_TONE));
    assert(!standard_counterpoint_fifth_species_slot_is_dissonant(&eighth_exercise, 10));

    /* A dissonance on the downbeat, and one reached by leap. */
    const StandardCounterpointSlot downbeat[] = {REST4(), HALF(P(E, 0, 4)), HALF(P(D, 0, 4)), HALF(P(C, 0, 4)),
                                                 WHOLE(P(C, 0, 4))};
    EXERCISE(downbeat_exercise, cantus, downbeat, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_slot_is_dissonant(&downbeat_exercise, 8));
    assert(!figure_at(&downbeat_exercise, 8, &figure));

    const StandardCounterpointSlot leap[] = {REST4(), HALF(P(E, 0, 4)), HALF(P(C, 0, 4)), QUARTER(P(F, 0, 4)),
                                             QUARTER(P(E, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(leap_exercise, cantus, leap, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_slot_is_dissonant(&leap_exercise, 12));
    assert(!figure_at(&leap_exercise, 12, &figure));
}

static void test_neighbor_tones(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    StandardCounterpointFifthSpeciesFigure figure;

    const StandardCounterpointSlot neighbor[] = {REST4(), HALF(P(E, 0, 4)), HALF(P(E, 0, 4)), QUARTER(P(F, 0, 4)),
                                                 QUARTER(P(E, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(neighbor_exercise, cantus, neighbor, CANTUS_LOWER);
    assert(figure_at(&neighbor_exercise, 12, &figure) && figure == FIGURE(NEIGHBOR_TONE));

    /* The upper and lower neighbors of E4, one after the other. */
    const StandardCounterpointSlot double_neighbor[] = {REST4(), HALF(P(E, 0, 4)), QUARTER(P(E, 0, 4)),
                                                        QUARTER(P(F, 0, 4)), QUARTER(P(D, 0, 4)),
                                                        QUARTER(P(E, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(double_neighbor_exercise, cantus, double_neighbor, CANTUS_LOWER);
    assert(figure_at(&double_neighbor_exercise, 10, &figure) && figure == FIGURE(DOUBLE_NEIGHBOR));
    assert(figure_at(&double_neighbor_exercise, 12, &figure) && figure == FIGURE(DOUBLE_NEIGHBOR));
}

static void test_nota_cambiata(void) {
    StandardCounterpointFifthSpeciesFigure figure;

    /* B3 is a seventh over C3, a step below C4, and the figure fills in a step
       down to the B3 of the next bar, a sixth over D3. */
    const PitchSpelling *down_cantus[] = {P(C, 0, 3), P(C, 0, 3), P(D, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot down[] = {REST4(), HALF(P(E, 0, 4)), QUARTER(P(C, 0, 4)), QUARTER(P(B, 0, 3)),
                                             QUARTER(P(G, 0, 3)), QUARTER(P(A, 0, 3)), WHOLE(P(B, 0, 3)),
                                             WHOLE(P(C, 0, 4))};
    EXERCISE(down_exercise, down_cantus, down, CANTUS_LOWER);
    assert(figure_at(&down_exercise, 10, &figure) && figure == FIGURE(NOTA_CAMBIATA_DOWN));

    /* The inversion: F4 is a fourth over C3, and comes back as a sixth over A3. */
    const PitchSpelling *up_cantus[] = {P(C, 0, 3), P(C, 0, 3), P(A, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot up[] = {REST4(), HALF(P(E, 0, 4)), QUARTER(P(E, 0, 4)), QUARTER(P(F, 0, 4)),
                                           QUARTER(P(A, 0, 4)), QUARTER(P(G, 0, 4)), WHOLE(P(F, 0, 4)),
                                           WHOLE(P(C, 0, 4))};
    EXERCISE(up_exercise, up_cantus, up, CANTUS_LOWER);
    assert(figure_at(&up_exercise, 10, &figure) && figure == FIGURE(NOTA_CAMBIATA_UP));
}

static void test_anticipations(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3), P(D, 0, 3), P(C, 0, 3)};
    StandardCounterpointFifthSpeciesFigure figure;

    /* D4 is a ninth over C3 and comes again on the downbeat, over D3. */
    const StandardCounterpointSlot quarter[] = {REST4(), HALF(P(E, 0, 4)), ATK(P(E, 0, 4)), T5(P(E, 0, 4)),
                                                QUARTER(P(D, 0, 4)), WHOLE(P(D, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(quarter_exercise, cantus, quarter, CANTUS_LOWER);
    assert(figure_at(&quarter_exercise, 14, &figure) && figure == FIGURE(ANTICIPATION));

    /* The same with a pair of eighths on the fourth beat. */
    const StandardCounterpointSlot eighths[] = {REST4(), HALF(P(E, 0, 4)), ATK(P(E, 0, 4)), T5(P(E, 0, 4)),
                                                EIGHTH(P(E, 0, 4)), EIGHTH(P(D, 0, 4)), WHOLE(P(D, 0, 4)),
                                                WHOLE(P(C, 0, 4))};
    EXERCISE(eighths_exercise, cantus, eighths, CANTUS_LOWER);
    assert(figure_at(&eighths_exercise, 15, &figure) && figure == FIGURE(ANTICIPATION_TURN));

    /* A different pitch on the downbeat is not an anticipation. */
    const StandardCounterpointSlot other[] = {REST4(), HALF(P(E, 0, 4)), ATK(P(E, 0, 4)), T5(P(E, 0, 4)),
                                              QUARTER(P(D, 0, 4)), WHOLE(P(F, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(other_exercise, cantus, other, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_slot_is_dissonant(&other_exercise, 14));
    assert(!figure_at(&other_exercise, 14, &figure));
}

static void test_suspension_types(void) {
    GOOD_FIFTH_SPECIES_CANTUS(cantus);
    GOOD_FIFTH_SPECIES_SLOTS(slots);
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointSuspensionType type = SUSPENSION(COUNT);

    assert(standard_counterpoint_fifth_species_suspension_type(&exercise, 24, &type) && type == SUSPENSION(SEVEN_SIX));
    assert(standard_counterpoint_fifth_species_suspension_type(&exercise, 72, &type) && type == SUSPENSION(SEVEN_SIX));

    /* A consonant tie, a new note, and a slot that is not a downbeat. */
    type = SUSPENSION(COUNT);
    assert(!standard_counterpoint_fifth_species_suspension_type(&exercise, 8, &type));
    assert(!standard_counterpoint_fifth_species_suspension_type(&exercise, 20, &type));
    assert(!standard_counterpoint_fifth_species_suspension_type(&exercise, 25, &type));
    assert(type == SUSPENSION(COUNT));
    assert(!standard_counterpoint_fifth_species_suspension_type(NULL, 24, &type));
    assert(!standard_counterpoint_fifth_species_suspension_type(&exercise, 24, NULL));

    /* A 4-3 above the cantus firmus. */
    const PitchSpelling *four_cantus[] = {P(D, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot four[] = {REST4(), HALF(P(F, 0, 4)), T2(P(F, 0, 4)), ATK(P(E, 0, 4)),
                                             T5(P(E, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(four_exercise, four_cantus, four, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_suspension_type(&four_exercise, 8, &type) &&
           type == SUSPENSION(FOUR_THREE));

    /* A 9-8. */
    const PitchSpelling *nine_cantus[] = {P(G, 0, 3), P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot nine[] = {REST4(), HALF(P(D, 0, 4)), T2(P(D, 0, 4)), ATK(P(C, 0, 4)),
                                             T5(P(C, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(nine_exercise, nine_cantus, nine, CANTUS_LOWER);
    assert(standard_counterpoint_fifth_species_suspension_type(&nine_exercise, 8, &type) &&
           type == SUSPENSION(NINE_EIGHT));

    /* A 2-3 below the cantus firmus. */
    const PitchSpelling *below_cantus[] = {P(F, 0, 3), P(E, 0, 3), P(D, 0, 3)};
    const StandardCounterpointSlot below[] = {REST4(), HALF(P(D, 0, 3)), T2(P(D, 0, 3)), ATK(P(C, 0, 3)),
                                              T5(P(C, 0, 3)), WHOLE(P(C, 0, 3))};
    EXERCISE(below_exercise, below_cantus, below, CANTUS_UPPER);
    assert(standard_counterpoint_fifth_species_suspension_type(&below_exercise, 8, &type) &&
           type == SUSPENSION(TWO_THREE));

    /* A resolution by leap is not a suspension. */
    const StandardCounterpointSlot leap[] = {REST4(), HALF(P(F, 0, 4)), T2(P(F, 0, 4)), ATK(P(C, 0, 4)),
                                             T5(P(C, 0, 4)), WHOLE(P(C, 0, 4))};
    EXERCISE(leap_exercise, four_cantus, leap, CANTUS_LOWER);
    StandardCounterpointFifthSpeciesFigure figure;
    assert(standard_counterpoint_fifth_species_slot_is_dissonant(&leap_exercise, 8));
    assert(!standard_counterpoint_fifth_species_suspension_type(&leap_exercise, 8, &type));
    assert(!figure_at(&leap_exercise, 8, &figure));
}

int main(void) {
    test_names_and_non_chord_tones();
    test_good_exercise();
    test_passing_tones();
    test_neighbor_tones();
    test_nota_cambiata();
    test_anticipations();
    test_suspension_types();
    printf("test_fifth_species_figure (standard-counterpoint): ok\n");
    return 0;
}
