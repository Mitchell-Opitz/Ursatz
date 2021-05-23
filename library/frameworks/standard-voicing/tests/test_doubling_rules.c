#include <assert.h>
#include <stdio.h>

#include "doubling_rules.h"
#include "framework.h"
#include "voicing_test_util.h"

static void c_major(const PitchSpelling *tones[3]) {
    tones[0] = P(C, 0, 3);
    tones[1] = P(E, 0, 3);
    tones[2] = P(G, 0, 3);
}

/* G major triad, the V of C major. */
static void g_major(const PitchSpelling *tones[3]) {
    tones[0] = P(G, 0, 2);
    tones[1] = P(B, 0, 2);
    tones[2] = P(D, 0, 3);
}

/* G dominant seventh. */
static void g7(const PitchSpelling *tones[4]) {
    tones[0] = P(G, 0, 2);
    tones[1] = P(B, 0, 2);
    tones[2] = P(D, 0, 3);
    tones[3] = P(F, 0, 3);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_VOICING_DOUBLING_RULE_COUNT; i++) {
        const Rule *rule = standard_voicing_doubling_rule((StandardVoicingDoublingRule)i);
        const Constraint *constraint = standard_voicing_doubling_constraint((StandardVoicingDoublingRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_voicing_framework()));
        /* The bass doubling is a preference; the rest are hard. */
        if (i == STANDARD_VOICING_DOUBLING_BASS_DOUBLED) {
            assert(constraint_type(constraint) == CONSTRAINT_TYPE_SOFT);
        } else {
            assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
        }
    }
    assert(standard_voicing_doubling_rule(STANDARD_VOICING_DOUBLING_RULE_COUNT) == NULL);
    assert(standard_voicing_doubling_constraint(STANDARD_VOICING_DOUBLING_RULE_COUNT) == NULL);
}

static void test_pitch_class_count(void) {
    StandardVoicingChord chord = make_chord(P(C, 0, 5), P(E, 0, 4), P(G, 0, 3), P(C, 0, 3));
    assert(standard_voicing_pitch_class_count(&chord, P(C, 0, 2)) == 2);
    assert(standard_voicing_pitch_class_count(&chord, P(E, 0, 4)) == 1);
    assert(standard_voicing_pitch_class_count(&chord, P(B, 0, 4)) == 0);
    /* Enharmonic spellings are not the same note. */
    assert(standard_voicing_pitch_class_count(&chord, P(B, 1, 3)) == 0);
    assert(standard_voicing_pitch_class_count(NULL, P(C, 0, 4)) == 0);
    assert(standard_voicing_pitch_class_count(&chord, NULL) == 0);
}

/* A triad usually doubles its bass note. */
static void test_triad_bass_doubled(void) {
    const PitchSpelling *tones[3];
    c_major(tones);

    StandardVoicingChord bass_doubled = make_chord(P(C, 0, 5), P(E, 0, 4), P(G, 0, 3), P(C, 0, 3));
    assert(standard_voicing_bass_doubled(&bass_doubled, tones, 3));

    /* The fifth doubled instead: allowed, just less usual. */
    StandardVoicingChord fifth_doubled = make_chord(P(G, 0, 4), P(E, 0, 4), P(G, 0, 3), P(C, 0, 3));
    assert(!standard_voicing_bass_doubled(&fifth_doubled, tones, 3));
    assert(standard_voicing_doubling_valid(&fifth_doubled, tones, 3, P(B, 0, 3)));
}

/* Example 7: the leading tone is doubled; then a corrected voicing. */
static void test_leading_tone_never_doubled(void) {
    const PitchSpelling *tones[3];
    g_major(tones);
    const PitchSpelling *leading_tone = P(B, 0, 3);

    StandardVoicingChord doubled = make_chord(P(B, 0, 4), P(D, 0, 4), P(B, 0, 3), P(G, 0, 2));
    assert(!standard_voicing_leading_tone_undoubled(&doubled, leading_tone));
    assert(!standard_voicing_doubling_valid(&doubled, tones, 3, leading_tone));
    /* With no leading tone in play there is nothing to forbid. */
    assert(standard_voicing_doubling_valid(&doubled, tones, 3, NULL));

    StandardVoicingChord corrected = make_chord(P(D, 0, 5), P(G, 0, 4), P(B, 0, 3), P(G, 0, 2));
    assert(standard_voicing_leading_tone_undoubled(&corrected, leading_tone));
    assert(standard_voicing_doubling_valid(&corrected, tones, 3, leading_tone));

    /* The leading tone absent from the chord is not doubled. */
    const PitchSpelling *c_tones[3];
    c_major(c_tones);
    StandardVoicingChord tonic = make_chord(P(C, 0, 5), P(E, 0, 4), P(G, 0, 3), P(C, 0, 3));
    assert(standard_voicing_leading_tone_undoubled(&tonic, leading_tone));

    assert(!standard_voicing_leading_tone_undoubled(&doubled, NULL));
    assert(!standard_voicing_leading_tone_undoubled(NULL, leading_tone));
}

static void test_chordal_seventh_never_doubled(void) {
    const PitchSpelling *tones[4];
    g7(tones);

    StandardVoicingChord doubled_seventh = make_chord(P(F, 0, 5), P(B, 0, 4), P(F, 0, 4), P(G, 0, 2));
    assert(!standard_voicing_seventh_undoubled(&doubled_seventh, tones, 4));
    assert(!standard_voicing_doubling_valid(&doubled_seventh, tones, 4, NULL));

    /* The seventh sounds once. */
    StandardVoicingChord single = make_chord(P(B, 0, 4), P(F, 0, 4), P(D, 0, 4), P(G, 0, 2));
    assert(standard_voicing_seventh_undoubled(&single, tones, 4));

    /* A triad has no seventh. */
    const PitchSpelling *triad[3];
    g_major(triad);
    StandardVoicingChord g_triad = make_chord(P(D, 0, 5), P(G, 0, 4), P(B, 0, 3), P(G, 0, 2));
    assert(standard_voicing_seventh_undoubled(&g_triad, triad, 3));
}

/* A seventh chord has four notes, one for each voice: nothing is doubled. */
static void test_seventh_chord_has_no_doubling(void) {
    const PitchSpelling *tones[4];
    g7(tones);

    StandardVoicingChord complete = make_chord(P(B, 0, 4), P(F, 0, 4), P(D, 0, 4), P(G, 0, 2));
    assert(standard_voicing_seventh_chord_undoubled(&complete, tones, 4));
    assert(standard_voicing_doubling_valid(&complete, tones, 4, P(B, 0, 3)));
    /* No bass preference applies to a seventh chord. */
    assert(standard_voicing_bass_doubled(&complete, tones, 4));

    /* The root doubled, the seventh missing: the seventh itself is not
       doubled, but the chord has a doubled note. */
    StandardVoicingChord doubled_root = make_chord(P(B, 0, 4), P(G, 0, 4), P(D, 0, 4), P(G, 0, 2));
    assert(standard_voicing_seventh_undoubled(&doubled_root, tones, 4));
    assert(!standard_voicing_seventh_chord_undoubled(&doubled_root, tones, 4));
    assert(!standard_voicing_doubling_valid(&doubled_root, tones, 4, NULL));

    /* A triad is not subject to this rule. */
    const PitchSpelling *triad[3];
    c_major(triad);
    StandardVoicingChord c_triad = make_chord(P(C, 0, 5), P(E, 0, 4), P(G, 0, 3), P(C, 0, 3));
    assert(standard_voicing_seventh_chord_undoubled(&c_triad, triad, 3));
}

static void test_invalid_input(void) {
    const PitchSpelling *tones[3];
    c_major(tones);
    StandardVoicingChord chord = make_chord(P(C, 0, 5), P(E, 0, 4), P(G, 0, 3), P(C, 0, 3));
    StandardVoicingChord incomplete = make_chord(P(C, 0, 5), NULL, P(G, 0, 3), P(C, 0, 3));

    assert(!standard_voicing_seventh_undoubled(NULL, tones, 3));
    assert(!standard_voicing_seventh_undoubled(&chord, NULL, 3));
    assert(!standard_voicing_seventh_undoubled(&chord, tones, 2));
    assert(!standard_voicing_seventh_undoubled(&incomplete, tones, 3));
    assert(!standard_voicing_seventh_chord_undoubled(&chord, tones, 5));
    assert(!standard_voicing_bass_doubled(&incomplete, tones, 3));
    assert(!standard_voicing_doubling_valid(&chord, tones, 1, NULL));
    assert(!standard_voicing_doubling_valid(NULL, tones, 3, NULL));

    const PitchSpelling *with_null[3] = {P(C, 0, 3), NULL, P(G, 0, 3)};
    assert(!standard_voicing_bass_doubled(&chord, with_null, 3));
}

int main(void) {
    test_rules_reference_framework();
    test_pitch_class_count();
    test_triad_bass_doubled();
    test_leading_tone_never_doubled();
    test_chordal_seventh_never_doubled();
    test_seventh_chord_has_no_doubling();
    test_invalid_input();
    printf("test_doubling_rules (standard-voicing): ok\n");
    return 0;
}
