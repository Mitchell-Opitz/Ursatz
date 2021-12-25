#include <assert.h>
#include <stdio.h>

#include "cantus_firmus_rules.h"
#include "counterpoint_limits.h"
#include "melodic_rules.h"
#include "counterpoint_test_util.h"

/*
 * Conformance of the cantus firmus rules to Fux's own cantus firmi (Gradus ad
 * Parnassum, 1725): the distinct melodies of the two-voice exercises, in the
 * order and pitches of the Open Music Theory chapter "Gradus ad Parnassum
 * Exercises". The rules are the textbook ideal and Fux's melodies sometimes
 * depart from it, so each melody's exact hard and soft masks are pinned here
 * and a melody that breaks a rule says which rule and why. A change to a rule
 * that alters a result for one of Fux's melodies fails this test.
 *
 * Each melody is checked twice: against the cantus firmus rules with its modal
 * final as the tonic, and against the melodic rules with the default limits.
 *
 * The rules ignore the octave, so the melodies Fux also uses an octave away,
 * and the cantus firmi of the three and four voice parts, are not repeated.
 */

#define RULE(name) STANDARD_COUNTERPOINT_CANTUS_RULE_##name
#define BIT(name) (1u << RULE(name))
#define MELODIC(name) (1u << STANDARD_COUNTERPOINT_MELODIC_RULE_##name)

typedef struct {
    const char *name;
    const PitchSpelling *const *notes;
    size_t count;
    const PitchSpelling *tonic;
    unsigned hard;
    unsigned soft;
    unsigned melodic_hard;
    unsigned melodic_soft;
} Melody;

static void expect(const Melody *melody) {
    StandardCounterpointLine line = {melody->notes, melody->count};
    unsigned hard = 0xFFFF, soft = 0xFFFF;
    assert(standard_counterpoint_cantus_check(&line, melody->tonic, &hard, &soft));
    if (hard != melody->hard || soft != melody->soft) {
        fprintf(stderr, "%s: hard %#x (expected %#x), soft %#x (expected %#x)\n", melody->name, hard, melody->hard,
                soft, melody->soft);
    }
    assert(hard == melody->hard);
    assert(soft == melody->soft);

    StandardCounterpointLimits limits = standard_counterpoint_default_limits();
    assert(standard_counterpoint_melodic_check(&line, &limits, &hard, &soft));
    if (hard != melody->melodic_hard || soft != melody->melodic_soft) {
        fprintf(stderr, "%s: melodic hard %#x (expected %#x), soft %#x (expected %#x)\n", melody->name, hard,
                melody->melodic_hard, soft, melody->melodic_soft);
    }
    assert(hard == melody->melodic_hard);
    assert(soft == melody->melodic_soft);
}

#define MELODY(id, array, tonic, hard, soft, melodic_hard, melodic_soft) \
    Melody id = {#id, array, sizeof(array) / sizeof((array)[0]), tonic, hard, soft, melodic_hard, melodic_soft}

static void test_dorian(void) {
    /* D: 10 uses. A fourth up is recovered by a step down. */
    const PitchSpelling *notes[] = {P(D, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4), P(G, 0, 4), P(F, 0, 4),
                                    P(A, 0, 4), P(G, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    MELODY(dorian, notes, P(D, 0, 4), 0, 0, 0, MELODIC(REPEATED_GENERIC));
    expect(&dorian);
}

static void test_phrygian(void) {
    /* E: 10 uses. The final approach is a half step from above. */
    const PitchSpelling *notes[] = {P(E, 0, 4), P(C, 0, 4), P(D, 0, 4), P(C, 0, 4), P(A, 0, 3),
                                    P(A, 0, 4), P(G, 0, 4), P(E, 0, 4), P(F, 0, 4), P(E, 0, 4)};
    MELODY(phrygian, notes, P(E, 0, 4), 0, 0, 0, 0);
    expect(&phrygian);
}

static void test_lydian_final_f(void) {
    /* F: 10 uses. After the leap up a fifth to C4 come two more leaps down:
       three leaps in a row, the first not recovered by a step. The run of
       notes D3 E3 F3 C4 outlines a seventh. */
    const PitchSpelling *notes[] = {P(F, 0, 3), P(G, 0, 3), P(A, 0, 3), P(F, 0, 3), P(D, 0, 3), P(E, 0, 3),
                                    P(F, 0, 3), P(C, 0, 4), P(A, 0, 3), P(F, 0, 3), P(G, 0, 3), P(F, 0, 3)};
    MELODY(final_f, notes, P(F, 0, 3), BIT(LEAP_RECOVERY) | BIT(CONSECUTIVE_LEAPS), 0,
           MELODIC(FORBIDDEN_OUTLINE), MELODIC(PITCH_OCCURRENCES) | MELODIC(CONSECUTIVE_LEAPS));
    expect(&final_f);
}

static void test_mixolydian(void) {
    /* G: 6 uses. Runs of leaps (B3 G3 C4 E4, and D4 G4 E4 C4) and a fourth
       up that is followed by a leap, and few steps. */
    const PitchSpelling *notes[] = {P(G, 0, 3), P(C, 0, 4), P(B, 0, 3), P(G, 0, 3), P(C, 0, 4),
                                    P(E, 0, 4), P(D, 0, 4), P(G, 0, 4), P(E, 0, 4), P(C, 0, 4),
                                    P(D, 0, 4), P(B, 0, 3), P(A, 0, 3), P(G, 0, 3)};
    MELODY(mixolydian, notes, P(G, 0, 3), BIT(LEAP_RECOVERY) | BIT(CONSECUTIVE_LEAPS), BIT(MOSTLY_STEPWISE), 0,
           MELODIC(CONSECUTIVE_LEAPS) | MELODIC(LEAP_DENSITY));
    expect(&mixolydian);
}

static void test_aeolian(void) {
    /* A: 6 uses. The highest note, F4, is reached by step. */
    const PitchSpelling *notes[] = {P(A, 0, 3), P(C, 0, 4), P(B, 0, 3), P(D, 0, 4), P(C, 0, 4), P(E, 0, 4),
                                    P(F, 0, 4), P(E, 0, 4), P(D, 0, 4), P(C, 0, 4), P(B, 0, 3), P(A, 0, 3)};
    MELODY(aeolian, notes, P(A, 0, 3), 0, 0, 0, MELODIC(REPEATED_GENERIC) | MELODIC(CLIMAX_BY_LEAP));
    expect(&aeolian);

    /* The variant without the first D (fig. 42). Its rising run B3 C4 E4 F4
       outlines a diminished fifth, which the full form avoids with its D4. */
    const PitchSpelling *variant_notes[] = {P(A, 0, 3), P(C, 0, 4), P(B, 0, 3), P(C, 0, 4), P(E, 0, 4), P(F, 0, 4),
                                            P(E, 0, 4), P(D, 0, 4), P(C, 0, 4), P(B, 0, 3), P(A, 0, 3)};
    MELODY(aeolian_variant, variant_notes, P(A, 0, 3), 0, 0, MELODIC(FORBIDDEN_OUTLINE),
           MELODIC(REPEATED_GENERIC) | MELODIC(CLIMAX_BY_LEAP));
    expect(&aeolian_variant);
}

static void test_ionian(void) {
    /* C: 2 uses; the first form, the same line used in Part II. */
    const PitchSpelling *notes[] = {P(C, 0, 4), P(E, 0, 4), P(F, 0, 4), P(G, 0, 4), P(E, 0, 4), P(A, 0, 4),
                                    P(G, 0, 4), P(E, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    MELODY(ionian, notes, P(C, 0, 4), 0, 0, 0, MELODIC(PITCH_OCCURRENCES) | MELODIC(REPEATED_GENERIC));
    expect(&ionian);

    /* C: 2 uses; the second, shorter form. */
    const PitchSpelling *short_notes[] = {P(C, 0, 4), P(D, 0, 4), P(F, 0, 4), P(E, 0, 4), P(G, 0, 4),
                                          P(E, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    MELODY(ionian_short, short_notes, P(C, 0, 4), 0, 0, 0, MELODIC(REPEATED_GENERIC));
    expect(&ionian_short);
}

int main(void) {
    test_dorian();
    test_phrygian();
    test_lydian_final_f();
    test_mixolydian();
    test_aeolian();
    test_ionian();
    printf("test_fux_cantus_conformance (standard-counterpoint): ok\n");
    return 0;
}
