#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "voicing_rules.h"
#include "voicing_test_util.h"

#define BIT(rule) (1u << (rule))

static const StandardVoicingStemDirection STEMS[VOICE_PART_COUNT] = {
    STANDARD_VOICING_STEM_UP, STANDARD_VOICING_STEM_DOWN, STANDARD_VOICING_STEM_UP, STANDARD_VOICING_STEM_DOWN};

static void c_major(const PitchSpelling *tones[3]) {
    tones[0] = P(C, 0, 3);
    tones[1] = P(E, 0, 3);
    tones[2] = P(G, 0, 3);
}

static void g_major(const PitchSpelling *tones[3]) {
    tones[0] = P(G, 0, 2);
    tones[1] = P(B, 0, 2);
    tones[2] = P(D, 0, 3);
}

static void g7(const PitchSpelling *tones[4]) {
    tones[0] = P(G, 0, 2);
    tones[1] = P(B, 0, 2);
    tones[2] = P(D, 0, 3);
    tones[3] = P(F, 0, 3);
}

static StandardVoicingInput input_for(const StandardVoicingChord *chord, const PitchSpelling *const *tones,
                                      size_t tone_count, int inversion) {
    StandardVoicingInput input = {chord, tones, tone_count, inversion, P(B, 0, 3), STEMS};
    return input;
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_VOICING_RULE_COUNT; i++) {
        const Rule *rule = standard_voicing_rule((StandardVoicingRule)i);
        const Constraint *constraint = standard_voicing_constraint((StandardVoicingRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_voicing_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_voicing_rule(STANDARD_VOICING_RULE_COUNT) == NULL);
    assert(standard_voicing_constraint(STANDARD_VOICING_RULE_COUNT) == NULL);
}

static void test_rule_names(void) {
    assert(strcmp(standard_voicing_rule_name(STANDARD_VOICING_RULE_STEM_DIRECTION), "stem-direction") == 0);
    assert(strcmp(standard_voicing_rule_name(STANDARD_VOICING_RULE_CHORD_CONSTRUCTION), "chord-construction") == 0);
    assert(strcmp(standard_voicing_rule_name(STANDARD_VOICING_RULE_RANGE), "range") == 0);
    assert(strcmp(standard_voicing_rule_name(STANDARD_VOICING_RULE_SPACING), "spacing") == 0);
    assert(strcmp(standard_voicing_rule_name(STANDARD_VOICING_RULE_VOICE_CROSSING), "voice-crossing") == 0);
    assert(strcmp(standard_voicing_rule_name(STANDARD_VOICING_RULE_DOUBLING), "doubling") == 0);
    assert(standard_voicing_rule_name(STANDARD_VOICING_RULE_COUNT) == NULL);
    assert(standard_voicing_rule_name((StandardVoicingRule)-1) == NULL);
}

static void test_valid_triad(void) {
    const PitchSpelling *tones[3];
    c_major(tones);
    StandardVoicingChord chord = make_chord(P(C, 0, 5), P(E, 0, 4), P(G, 0, 3), P(C, 0, 3));
    StandardVoicingInput input = input_for(&chord, tones, 3, 0);

    unsigned violations = 99;
    assert(standard_voicing_check(&input, &violations));
    assert(violations == 0);
    assert(standard_voicing_valid(&input));

    /* Stems are optional: an unengraved chord skips that rule. */
    input.stem_directions = NULL;
    assert(standard_voicing_valid(&input));
}

static void test_valid_seventh_chord(void) {
    const PitchSpelling *tones[4];
    g7(tones);
    StandardVoicingChord chord = make_chord(P(B, 0, 4), P(F, 0, 4), P(D, 0, 4), P(G, 0, 2));
    StandardVoicingInput input = input_for(&chord, tones, 4, 0);
    assert(standard_voicing_valid(&input));
}

static void test_each_rule_alone(void) {
    unsigned violations;

    const PitchSpelling *c_tones[3];
    c_major(c_tones);

    /* STEM_DIRECTION: the soprano's stem points down. */
    {
        StandardVoicingChord chord = make_chord(P(C, 0, 5), P(E, 0, 4), P(G, 0, 3), P(C, 0, 3));
        StandardVoicingInput input = input_for(&chord, c_tones, 3, 0);
        StandardVoicingStemDirection wrong[VOICE_PART_COUNT] = {STANDARD_VOICING_STEM_DOWN, STANDARD_VOICING_STEM_DOWN,
                                                                STANDARD_VOICING_STEM_UP, STANDARD_VOICING_STEM_DOWN};
        input.stem_directions = wrong;
        assert(standard_voicing_check(&input, &violations));
        assert(violations == BIT(STANDARD_VOICING_RULE_STEM_DIRECTION));
        assert(!standard_voicing_valid(&input));
    }

    /* CHORD_CONSTRUCTION: the bass is the third in a chord called root position. */
    {
        StandardVoicingChord chord = make_chord(P(C, 0, 5), P(E, 0, 4), P(G, 0, 3), P(E, 0, 3));
        StandardVoicingInput input = input_for(&chord, c_tones, 3, 0);
        assert(standard_voicing_check(&input, &violations));
        assert(violations == BIT(STANDARD_VOICING_RULE_CHORD_CONSTRUCTION));
    }

    /* RANGE: the bass is below its range (spacing breaks too). */
    {
        StandardVoicingChord chord = make_chord(P(C, 0, 5), P(E, 0, 4), P(G, 0, 3), P(C, 0, 2));
        StandardVoicingInput input = input_for(&chord, c_tones, 3, 0);
        assert(standard_voicing_check(&input, &violations));
        assert((violations & BIT(STANDARD_VOICING_RULE_RANGE)) != 0);
    }

    /* SPACING: the soprano and alto are more than an octave apart. */
    {
        StandardVoicingChord chord = make_chord(P(E, 0, 5), P(C, 0, 4), P(G, 0, 3), P(C, 0, 3));
        StandardVoicingInput input = input_for(&chord, c_tones, 3, 0);
        assert(standard_voicing_check(&input, &violations));
        assert(violations == BIT(STANDARD_VOICING_RULE_SPACING));
    }

    /* VOICE_CROSSING: the alto sounds below the tenor. */
    {
        StandardVoicingChord chord = make_chord(P(G, 0, 4), P(C, 0, 4), P(E, 0, 4), P(C, 0, 3));
        StandardVoicingInput input = input_for(&chord, c_tones, 3, 0);
        assert(standard_voicing_check(&input, &violations));
        assert(violations == BIT(STANDARD_VOICING_RULE_VOICE_CROSSING));
    }

    /* DOUBLING: the leading tone is doubled in the V chord. */
    {
        const PitchSpelling *g_tones[3];
        g_major(g_tones);
        StandardVoicingChord chord = make_chord(P(B, 0, 4), P(D, 0, 4), P(B, 0, 3), P(G, 0, 2));
        StandardVoicingInput input = input_for(&chord, g_tones, 3, 0);
        assert(standard_voicing_check(&input, &violations));
        assert(violations == BIT(STANDARD_VOICING_RULE_DOUBLING));

        /* The same notes in a key with no leading tone in play are fine. */
        input.leading_tone = NULL;
        assert(standard_voicing_valid(&input));
    }
}

static void test_several_rules_at_once(void) {
    const PitchSpelling *tones[4];
    g7(tones);

    /* The seventh doubled and the fifth missing. */
    StandardVoicingChord chord = make_chord(P(F, 0, 5), P(B, 0, 4), P(F, 0, 4), P(G, 0, 2));
    StandardVoicingInput input = input_for(&chord, tones, 4, 0);
    unsigned violations;
    assert(standard_voicing_check(&input, &violations));
    assert((violations & BIT(STANDARD_VOICING_RULE_CHORD_CONSTRUCTION)) != 0);
    assert((violations & BIT(STANDARD_VOICING_RULE_DOUBLING)) != 0);
    assert(!standard_voicing_valid(&input));
}

static void test_invalid_input(void) {
    const PitchSpelling *tones[3];
    c_major(tones);
    StandardVoicingChord chord = make_chord(P(C, 0, 5), P(E, 0, 4), P(G, 0, 3), P(C, 0, 3));
    StandardVoicingInput good = input_for(&chord, tones, 3, 0);
    unsigned violations = 7;

    assert(!standard_voicing_check(NULL, &violations));
    assert(!standard_voicing_check(&good, NULL));
    assert(!standard_voicing_valid(NULL));

    StandardVoicingInput no_chord = good;
    no_chord.chord = NULL;
    assert(!standard_voicing_check(&no_chord, &violations));

    StandardVoicingInput bad_count = good;
    bad_count.tone_count = 2;
    assert(!standard_voicing_check(&bad_count, &violations));

    StandardVoicingInput bad_inversion = good;
    bad_inversion.inversion = 5;
    assert(!standard_voicing_check(&bad_inversion, &violations));
    assert(!standard_voicing_valid(&bad_inversion));

    StandardVoicingChord incomplete = make_chord(P(C, 0, 5), NULL, P(G, 0, 3), P(C, 0, 3));
    StandardVoicingInput null_voice = good;
    null_voice.chord = &incomplete;
    assert(!standard_voicing_check(&null_voice, &violations));

    assert(violations == 7);
}

int main(void) {
    test_rules_reference_framework();
    test_rule_names();
    test_valid_triad();
    test_valid_seventh_chord();
    test_each_rule_alone();
    test_several_rules_at_once();
    test_invalid_input();
    printf("test_voicing_rules (standard-voicing): ok\n");
    return 0;
}
