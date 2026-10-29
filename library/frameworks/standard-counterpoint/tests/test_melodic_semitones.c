#include <assert.h>
#include <stdio.h>

#include "counterpoint_test_util.h"
#include "melodic_rules.h"

/*
 * The semitone forms of the melodic rules. -24 to 24 semitones from C4 are compared with the
 * Composer's move_ok (counterpoint_species.c, copied below as the record P25 uses) and with the
 * spelled FORBIDDEN_LEAP rule on C major spellings. A step is compared with the Composer's is_step.
 */

static int absolute(int value) {
    return value < 0 ? -value : value;
}

/* Composer counterpoint_species.c move_ok, on the move alone. */
static bool composer_move_ok(int move) {
    int size = absolute(move);
    return size <= 12 && size != 6 && size != 9 && size != 10 && size != 11 && !(size == 8 && move < 0);
}

/* Composer melody_nct.c is_step. */
static bool composer_is_step(int move) {
    return absolute(move) >= 1 && absolute(move) <= 2;
}

/* The pitch 60 + move as a spelling in the key of C major, the chromatic notes spelled as the
   interval from C4 would be (minor second, minor third, diminished fifth, minor sixth, minor
   seventh). */
static const PitchSpelling *spelled(int midi) {
    static const PitchLetter LETTER[12] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_D, PITCH_LETTER_E,
                                           PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_G, PITCH_LETTER_G,
                                           PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_B};
    static const int ACCIDENTAL[12] = {0, -1, 0, -1, 0, 0, -1, 0, -1, 0, -1, 0};
    int pc = ((midi % 12) + 12) % 12;
    int octave = (midi - pc) / 12 - 1;
    return note(LETTER[pc], ACCIDENTAL[pc], octave);
}

static void test_matches_composer_and_spelled_rules(void) {
    StandardCounterpointLimits limits = standard_counterpoint_default_limits();
    for (int move = -24; move <= 24; move++) {
        assert(standard_counterpoint_melodic_semitones_allowed(move) == composer_move_ok(move));
        const PitchSpelling *line_notes[2] = {spelled(60), spelled(60 + move)};
        StandardCounterpointLine line = LINE_OF(line_notes);
        bool holds = standard_counterpoint_melodic_rule_holds(STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_LEAP,
                                                              &line, &limits);
        /* The descending minor sixth is spelled from C4 down to E3 (a minor sixth); a
           spelled interval of the same size in the other direction is the major sixth. */
        assert(holds == standard_counterpoint_melodic_semitones_allowed(move));
        assert(standard_counterpoint_is_step(move) == composer_is_step(move));
    }
}

static void test_values(void) {
    assert(standard_counterpoint_melodic_semitones_allowed(0));
    assert(standard_counterpoint_melodic_semitones_allowed(7));
    assert(standard_counterpoint_melodic_semitones_allowed(8));
    assert(!standard_counterpoint_melodic_semitones_allowed(-8));
    assert(!standard_counterpoint_melodic_semitones_allowed(6));
    assert(!standard_counterpoint_melodic_semitones_allowed(-6));
    assert(standard_counterpoint_melodic_semitones_allowed(-12));
    assert(!standard_counterpoint_melodic_semitones_allowed(13));
    assert(!standard_counterpoint_is_step(0));
    assert(standard_counterpoint_is_step(-1));
    assert(standard_counterpoint_is_step(2));
    assert(!standard_counterpoint_is_step(3));
}

int main(void) {
    test_matches_composer_and_spelled_rules();
    test_values();
    printf("standard-counterpoint melodic semitones tests passed\n");
    return 0;
}
