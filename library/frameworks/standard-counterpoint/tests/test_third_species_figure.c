#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "third_species_figure.h"
#include "third_species_test_util.h"

#define FIGURE(name) STANDARD_COUNTERPOINT_FIGURE_##name

static bool figure_of(const StandardCounterpointThirdSpeciesExercise *exercise, size_t bar, size_t beat,
                      StandardCounterpointDissonanceFigure *figure) {
    return standard_counterpoint_third_species_dissonance_figure(exercise, bar, beat, figure);
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_dissonance_figure_name(FIGURE(PASSING_TONE)), "passing-tone") == 0);
    assert(strcmp(standard_counterpoint_dissonance_figure_name(FIGURE(NOTA_CAMBIATA_UP)), "nota-cambiata-up") == 0);
    assert(standard_counterpoint_dissonance_figure_name(FIGURE(COUNT)) == NULL);
}

static void test_non_chord_tones(void) {
    assert(standard_counterpoint_dissonance_figure_non_chord_tone(FIGURE(PASSING_TONE)) == NON_CHORD_TONE_PASSING);
    assert(standard_counterpoint_dissonance_figure_non_chord_tone(FIGURE(DOUBLE_PASSING_TONE)) ==
           NON_CHORD_TONE_PASSING);
    assert(standard_counterpoint_dissonance_figure_non_chord_tone(FIGURE(NEIGHBOR_TONE)) == NON_CHORD_TONE_NEIGHBOR);
    assert(standard_counterpoint_dissonance_figure_non_chord_tone(FIGURE(DOUBLE_NEIGHBOR)) ==
           NON_CHORD_TONE_DOUBLE_NEIGHBOR);
    assert(standard_counterpoint_dissonance_figure_non_chord_tone(FIGURE(NOTA_CAMBIATA_DOWN)) ==
           NON_CHORD_TONE_CHANGING_TONE);
    assert(standard_counterpoint_dissonance_figure_non_chord_tone(FIGURE(NOTA_CAMBIATA_UP)) ==
           NON_CHORD_TONE_CHANGING_TONE);
    assert(standard_counterpoint_dissonance_figure_non_chord_tone(FIGURE(COUNT)) == NON_CHORD_TONE_COUNT);
}

static void test_beat_is_dissonant(void) {
    /* Over C3: G3 is a fifth, F3 a fourth. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(E, 0, 3)), ATK(P(F, 0, 3)), ATK(P(G, 0, 3)), ATK(P(A, 0, 3)),
                                              WHOLE(P(G, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    assert(!standard_counterpoint_third_species_beat_is_dissonant(&exercise, 0, 0));
    assert(standard_counterpoint_third_species_beat_is_dissonant(&exercise, 0, 1));
    assert(!standard_counterpoint_third_species_beat_is_dissonant(&exercise, 0, 2));
    assert(!standard_counterpoint_third_species_beat_is_dissonant(&exercise, 5, 0));
}

static void test_passing_tone(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(E, 0, 3)), ATK(P(F, 0, 3)), ATK(P(G, 0, 3)), ATK(P(A, 0, 3)),
                                              WHOLE(P(G, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointDissonanceFigure figure;
    assert(figure_of(&exercise, 0, 1, &figure) && figure == FIGURE(PASSING_TONE));
    /* Consonant notes and downbeats have no figure. */
    assert(!figure_of(&exercise, 0, 2, &figure));
    assert(!figure_of(&exercise, 0, 0, &figure));
}

static void test_passing_tone_on_beat_four(void) {
    /* A3, B3 (a seventh), C4: the dissonance on beat 4 passes into the next
       downbeat. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(E, 0, 3)), ATK(P(G, 0, 3)), ATK(P(A, 0, 3)), ATK(P(B, 0, 3)),
                                              WHOLE(P(C, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointDissonanceFigure figure;
    assert(figure_of(&exercise, 0, 3, &figure) && figure == FIGURE(PASSING_TONE));
}

static void test_double_passing_tone(void) {
    /* Over E3: G3, A3 (a fourth), B-flat 3 (a diminished fifth), C4. */
    const PitchSpelling *cantus[] = {P(E, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(G, 0, 3)), ATK(P(A, 0, 3)), ATK(P(B, -1, 3)), ATK(P(C, 0, 4)),
                                              WHOLE(P(C, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointDissonanceFigure figure;
    assert(figure_of(&exercise, 0, 1, &figure) && figure == FIGURE(DOUBLE_PASSING_TONE));
    assert(figure_of(&exercise, 0, 2, &figure) && figure == FIGURE(DOUBLE_PASSING_TONE));
}

static void test_three_dissonances_are_no_figure(void) {
    /* Over E3: G3, then A3, B-flat 3, A3 (a fourth, a diminished fifth, a
       fourth) before G3. Three dissonances in a row are more than two. */
    const PitchSpelling *cantus[] = {P(E, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(G, 0, 3)), ATK(P(A, 0, 3)), ATK(P(B, -1, 3)), ATK(P(A, 0, 3)),
                                              WHOLE(P(G, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointDissonanceFigure figure;
    assert(!figure_of(&exercise, 0, 1, &figure));
    assert(!figure_of(&exercise, 0, 2, &figure));
    assert(!figure_of(&exercise, 0, 3, &figure));
}

static void test_neighbor_tone(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3)};
    /* A lower neighbor: G3, F3, G3. */
    const StandardCounterpointSlot lower[] = {ATK(P(G, 0, 3)), ATK(P(F, 0, 3)), ATK(P(G, 0, 3)), ATK(P(E, 0, 3)),
                                              WHOLE(P(E, 0, 3))};
    /* An upper neighbor: A3, B3, A3. */
    const StandardCounterpointSlot upper[] = {ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)), ATK(P(G, 0, 3)),
                                              WHOLE(P(G, 0, 3))};
    EXERCISE(lower_exercise, cantus, lower, CANTUS_LOWER);
    EXERCISE(upper_exercise, cantus, upper, CANTUS_LOWER);
    StandardCounterpointDissonanceFigure figure;
    assert(figure_of(&lower_exercise, 0, 1, &figure) && figure == FIGURE(NEIGHBOR_TONE));
    assert(figure_of(&upper_exercise, 0, 1, &figure) && figure == FIGURE(NEIGHBOR_TONE));
}

static void test_double_neighbor(void) {
    /* C4, D4, B3, C4 over C3, then D4 over G3: the upper neighbor first. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(G, 0, 3)};
    const StandardCounterpointSlot upper_first[] = {ATK(P(C, 0, 4)), ATK(P(D, 0, 4)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)),
                                                    WHOLE(P(D, 0, 4))};
    /* C4, B3, D4, C4 over C3, then B3 over G3: the lower neighbor first. */
    const StandardCounterpointSlot lower_first[] = {ATK(P(C, 0, 4)), ATK(P(B, 0, 3)), ATK(P(D, 0, 4)), ATK(P(C, 0, 4)),
                                                    WHOLE(P(B, 0, 3))};
    /* The motion across the bar line is the other way. */
    const StandardCounterpointSlot wrong_way[] = {ATK(P(C, 0, 4)), ATK(P(D, 0, 4)), ATK(P(B, 0, 3)), ATK(P(C, 0, 4)),
                                                  WHOLE(P(B, 0, 3))};
    EXERCISE(upper_exercise, cantus, upper_first, CANTUS_LOWER);
    EXERCISE(lower_exercise, cantus, lower_first, CANTUS_LOWER);
    EXERCISE(wrong_exercise, cantus, wrong_way, CANTUS_LOWER);
    StandardCounterpointDissonanceFigure figure;
    assert(figure_of(&upper_exercise, 0, 1, &figure) && figure == FIGURE(DOUBLE_NEIGHBOR));
    assert(figure_of(&upper_exercise, 0, 2, &figure) && figure == FIGURE(DOUBLE_NEIGHBOR));
    assert(figure_of(&lower_exercise, 0, 1, &figure) && figure == FIGURE(DOUBLE_NEIGHBOR));
    assert(figure_of(&lower_exercise, 0, 2, &figure) && figure == FIGURE(DOUBLE_NEIGHBOR));
    assert(!figure_of(&wrong_exercise, 0, 1, &figure));
    assert(!figure_of(&wrong_exercise, 0, 2, &figure));
}

static void test_nota_cambiata_down(void) {
    /* C4, B3 (a seventh), G3, A3 over C3, then B3 over E3: fills in a step
       down from C4 to B3. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(E, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(C, 0, 4)), ATK(P(B, 0, 3)), ATK(P(G, 0, 3)), ATK(P(A, 0, 3)),
                                              WHOLE(P(B, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointDissonanceFigure figure;
    assert(figure_of(&exercise, 0, 1, &figure) && figure == FIGURE(NOTA_CAMBIATA_DOWN));

    /* The dissonant pitch must come back as the next downbeat. */
    const StandardCounterpointSlot not_back[] = {ATK(P(C, 0, 4)), ATK(P(B, 0, 3)), ATK(P(G, 0, 3)), ATK(P(A, 0, 3)),
                                                 WHOLE(P(G, 0, 3))};
    EXERCISE(not_back_exercise, cantus, not_back, CANTUS_LOWER);
    assert(!figure_of(&not_back_exercise, 0, 1, &figure));
}

static void test_nota_cambiata_up(void) {
    /* E4, F4 (a fourth over an octave), A4, G4 over C3, then F4 over F3. */
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(F, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(E, 0, 4)), ATK(P(F, 0, 4)), ATK(P(A, 0, 4)), ATK(P(G, 0, 4)),
                                              WHOLE(P(F, 0, 4))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointDissonanceFigure figure;
    assert(figure_of(&exercise, 0, 1, &figure) && figure == FIGURE(NOTA_CAMBIATA_UP));
}

static void test_dissonance_by_leap_is_no_figure(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(G, 0, 3)), ATK(P(B, 0, 3)), ATK(P(A, 0, 3)), ATK(P(G, 0, 3)),
                                              WHOLE(P(G, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointDissonanceFigure figure;
    assert(!figure_of(&exercise, 0, 1, &figure));
}

static void test_dissonance_after_a_rest(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {REST(), ATK(P(F, 0, 3)), ATK(P(G, 0, 3)), ATK(P(A, 0, 3)),
                                              WHOLE(P(G, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointDissonanceFigure figure;
    assert(!figure_of(&exercise, 0, 1, &figure));
}

static void test_invalid_input(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(C, 0, 3)};
    const StandardCounterpointSlot slots[] = {ATK(P(E, 0, 3)), ATK(P(F, 0, 3)), ATK(P(G, 0, 3)), ATK(P(A, 0, 3)),
                                              WHOLE(P(G, 0, 3))};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    StandardCounterpointDissonanceFigure figure = FIGURE(COUNT);
    assert(!figure_of(NULL, 0, 1, &figure));
    assert(!figure_of(&exercise, 9, 1, &figure));
    assert(!figure_of(&exercise, 0, 4, &figure));
    assert(!standard_counterpoint_third_species_dissonance_figure(&exercise, 0, 1, NULL));
    assert(figure == FIGURE(COUNT));
    assert(!standard_counterpoint_third_species_beat_is_dissonant(NULL, 0, 1));
}

int main(void) {
    test_names();
    test_non_chord_tones();
    test_beat_is_dissonant();
    test_passing_tone();
    test_passing_tone_on_beat_four();
    test_double_passing_tone();
    test_three_dissonances_are_no_figure();
    test_neighbor_tone();
    test_double_neighbor();
    test_nota_cambiata_down();
    test_nota_cambiata_up();
    test_dissonance_by_leap_is_no_figure();
    test_dissonance_after_a_rest();
    test_invalid_input();
    printf("test_third_species_figure (standard-counterpoint): ok\n");
    return 0;
}
