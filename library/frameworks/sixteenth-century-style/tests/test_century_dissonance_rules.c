#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "century_dissonance_rules.h"
#include "century_rule_check.h"
#include "century_test_util.h"

#define DISSONANCE(name) SIXTEENTH_CENTURY_DISSONANCE_##name

static bool holds(SixteenthCenturyDissonanceRule rule, const SixteenthCenturyScore *score) {
    return sixteenth_century_dissonance_rule_holds(rule, score);
}

/* A two-voice score over the slot arrays `up` and `low`. */
#define TWO_VOICES(name, up, low)                                                              \
    StandardCounterpointSlotLine name##_voices[] = {{(up), COUNT_OF(up)}, {(low), COUNT_OF(low)}}; \
    SCORE_OF(name, name##_voices)

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(sixteenth_century_dissonance_rule, sixteenth_century_dissonance_constraint,
                                    SIXTEENTH_CENTURY_DISSONANCE_RULE_COUNT);
    for (int i = 0; i < SIXTEENTH_CENTURY_DISSONANCE_RULE_COUNT; i++) {
        ConstraintType type =
            constraint_type(sixteenth_century_dissonance_constraint((SixteenthCenturyDissonanceRule)i));
        assert(type == (i <= DISSONANCE(PROPER_SUSPENSIONS) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(sixteenth_century_dissonance_rule_name(DISSONANCE(FIGURED)), "figured") == 0);
    assert(strcmp(sixteenth_century_dissonance_rule_name(DISSONANCE(PROPER_SUSPENSIONS)), "proper-suspensions") == 0);
    assert(strcmp(sixteenth_century_dissonance_rule_name(DISSONANCE(FIGURE_RESOLVES)), "figure-resolves") == 0);
    assert(sixteenth_century_dissonance_rule_name(DISSONANCE(RULE_COUNT)) == NULL);
}

static void test_consonant_at(void) {
    const PitchSpelling *c3 = P(C, 0, 3);
    struct {
        const PitchSpelling *upper;
        bool consonant;
    } cases[] = {
        {P(C, 0, 3), true},  /* unison */
        {P(E, -1, 4), true}, /* minor tenth */
        {P(E, 0, 4), true},  /* major tenth */
        {P(G, 0, 4), true},  /* twelfth */
        {P(A, 0, 4), true},  /* thirteenth */
        {P(C, 0, 5), true},  /* double octave */
        {P(D, 0, 4), false}, /* ninth */
        {P(B, 0, 4), false}, /* fourteenth, a seventh */
        {P(F, 0, 4), false}, /* fourth against the lowest voice */
        {P(F, 1, 4), false}, /* augmented fourth */
        {P(G, -1, 4), false} /* diminished fifth */
    };
    for (size_t i = 0; i < COUNT_OF(cases); i++) {
        StandardCounterpointSlot up[] = {ATK(cases[i].upper)};
        StandardCounterpointSlot low[] = {ATK(c3)};
        TWO_VOICES(score, up, low);
        assert(sixteenth_century_consonant_at(&score, 0, 1, 0) == cases[i].consonant);
    }

    /* A fourth between two upper voices is consonant. */
    StandardCounterpointSlot top[] = {ATK(P(C, 0, 4))};
    StandardCounterpointSlot mid[] = {ATK(P(G, 0, 3))};
    StandardCounterpointSlot bass[] = {ATK(c3)};
    StandardCounterpointSlotLine voices[] = {{top, 1}, {mid, 1}, {bass, 1}};
    SCORE_OF(three, voices);
    assert(sixteenth_century_consonant_at(&three, 0, 1, 0));
    assert(!sixteenth_century_consonant_at(&three, 0, 5, 0));
    assert(!sixteenth_century_consonant_at(&three, 0, 1, 1));

    /* A rest has no interval. */
    StandardCounterpointSlot resting[] = {REST()};
    StandardCounterpointSlot sounding[] = {ATK(c3)};
    TWO_VOICES(rested, resting, sounding);
    assert(!sixteenth_century_consonant_at(&rested, 0, 1, 0));
    assert(!sixteenth_century_consonant_at(NULL, 0, 1, 0));
}

static void test_passing_tone(void) {
    const PitchSpelling *c3 = P(C, 0, 3);
    const PitchSpelling *e4 = P(E, 0, 4);
    const PitchSpelling *d4 = P(D, 0, 4);
    const PitchSpelling *c4 = P(C, 0, 4);

    StandardCounterpointSlot up[] = {QUARTER(e4), QUARTER(d4), HALF(c4)};
    StandardCounterpointSlot low[] = {WHOLE(c3)};
    TWO_VOICES(score, up, low);
    for (int i = 0; i < SIXTEENTH_CENTURY_DISSONANCE_RULE_COUNT; i++) {
        assert(holds((SixteenthCenturyDissonanceRule)i, &score));
    }
}

static void test_neighbor_must_resolve(void) {
    const PitchSpelling *c3 = P(C, 0, 3);
    const PitchSpelling *d3 = P(D, 0, 3);
    const PitchSpelling *e4 = P(E, 0, 4);
    const PitchSpelling *d4 = P(D, 0, 4);

    /* The neighbor D4 against C3 is a figure, but the note after it begins a
       dissonance against D3. */
    StandardCounterpointSlot up[] = {QUARTER(e4), QUARTER(d4), HALF(e4)};
    StandardCounterpointSlot low[] = {HALF(c3), HALF(d3)};
    TWO_VOICES(score, up, low);
    assert(!holds(DISSONANCE(FIGURE_RESOLVES), &score));
}

static void test_nota_cambiata(void) {
    const PitchSpelling *c3 = P(C, 0, 3);
    const PitchSpelling *c4 = P(C, 0, 4);
    const PitchSpelling *b3 = P(B, 0, 3);
    const PitchSpelling *g3 = P(G, 0, 3);
    const PitchSpelling *a3 = P(A, 0, 3);

    /* An octave, a seventh, a fifth, a sixth over C3: do, ti, sol, la. */
    StandardCounterpointSlot up[] = {QUARTER(c4), QUARTER(b3), QUARTER(g3), QUARTER(a3)};
    StandardCounterpointSlot low[] = {WHOLE(c3)};
    TWO_VOICES(score, up, low);
    assert(holds(DISSONANCE(FIGURED), &score));
    assert(holds(DISSONANCE(FIGURE_RESOLVES), &score));

    /* The figure must climb back by step: here it leaps up. */
    const PitchSpelling *e4 = P(E, 0, 4);
    StandardCounterpointSlot up_bad[] = {QUARTER(c4), QUARTER(b3), QUARTER(g3), QUARTER(e4)};
    TWO_VOICES(bad, up_bad, low);
    assert(!holds(DISSONANCE(FIGURED), &bad));
}

static void test_unfigured_dissonance(void) {
    const PitchSpelling *c3 = P(C, 0, 3);
    const PitchSpelling *g4 = P(G, 0, 4);
    const PitchSpelling *d4 = P(D, 0, 4);
    const PitchSpelling *c4 = P(C, 0, 4);

    /* A dissonance left sub-tactus by a leap from G4 is no figure. */
    StandardCounterpointSlot leap_up[] = {QUARTER(g4), QUARTER(d4), HALF(c4)};
    StandardCounterpointSlot low[] = {WHOLE(c3)};
    TWO_VOICES(leaping, leap_up, low);
    assert(!holds(DISSONANCE(FIGURED), &leaping));

    /* A new dissonant note on a tactus start, with both voices attacking. */
    StandardCounterpointSlot beat_up[] = {HALF(d4), HALF(c4)};
    StandardCounterpointSlot beat_low[] = {HALF(c3), HALF(c3)};
    TWO_VOICES(on_beat, beat_up, beat_low);
    assert(!holds(DISSONANCE(FIGURED), &on_beat));
}

static void test_suspension(void) {
    const PitchSpelling *f4 = P(F, 0, 4);
    const PitchSpelling *e4 = P(E, 0, 4);
    const PitchSpelling *d5 = P(D, 0, 5);
    const PitchSpelling *c5 = P(C, 0, 5);
    const PitchSpelling *e5 = P(E, 0, 5);

    /* A 7-6: D5 prepared on the weak tactus over F4, held against E4, and
       resolved down to C5. */
    StandardCounterpointSlot up[] = {REST4(), ATK(d5), T7(d5), ATK(c5), T3(c5)};
    StandardCounterpointSlot low[] = {HALF(f4), HALF(f4), HALF(e4), HALF(e4)};
    TWO_VOICES(good, up, low);
    assert(holds(DISSONANCE(FIGURED), &good));
    assert(holds(DISSONANCE(PROPER_SUSPENSIONS), &good));
    assert(holds(DISSONANCE(FIGURE_RESOLVES), &good));

    /* Not prepared on a weaker position: the note began on the strong tactus. */
    StandardCounterpointSlot up_strong[] = {ATK(d5), T3(d5), T3(d5), T3(d5), T2(d5), ATK(c5), T3(c5)};
    TWO_VOICES(strong, up_strong, low);
    assert(holds(DISSONANCE(FIGURED), &strong));
    assert(!holds(DISSONANCE(PROPER_SUSPENSIONS), &strong));

    /* Not resolved down: it steps up. */
    StandardCounterpointSlot up_up[] = {REST4(), ATK(d5), T7(d5), ATK(e5), T3(e5)};
    TWO_VOICES(unresolved, up_up, low);
    assert(!holds(DISSONANCE(PROPER_SUSPENSIONS), &unresolved));

    /* Not resolved at all: the voice ends on the suspension. */
    StandardCounterpointSlot up_end[] = {REST4(), ATK(d5), T7(d5)};
    StandardCounterpointSlot low_end[] = {HALF(f4), HALF(f4), HALF(e4)};
    TWO_VOICES(ending, up_end, low_end);
    assert(!holds(DISSONANCE(PROPER_SUSPENSIONS), &ending));
}

static void test_invalid(void) {
    const PitchSpelling *c4 = P(C, 0, 4);
    StandardCounterpointSlot up[] = {HALF(c4)};
    StandardCounterpointSlot low[] = {HALF(c4)};
    TWO_VOICES(score, up, low);
    assert(!holds(DISSONANCE(RULE_COUNT), &score));
    assert(!holds((SixteenthCenturyDissonanceRule)-1, &score));
    assert(!holds(DISSONANCE(FIGURED), NULL));
    unsigned hard = 7, soft = 7;
    assert(!sixteenth_century_dissonance_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!sixteenth_century_dissonance_check(&score, NULL, &soft));
    assert(!sixteenth_century_dissonance_check(&score, &hard, NULL));
}

static void test_check(void) {
    const PitchSpelling *c3 = P(C, 0, 3);
    const PitchSpelling *d4 = P(D, 0, 4);
    const PitchSpelling *c4 = P(C, 0, 4);
    StandardCounterpointSlot up[] = {HALF(d4), HALF(c4)};
    StandardCounterpointSlot low[] = {HALF(c3), HALF(c3)};
    TWO_VOICES(score, up, low);
    unsigned hard = 0, soft = 0;
    assert(sixteenth_century_dissonance_check(&score, &hard, &soft));
    assert(hard == (1u << DISSONANCE(FIGURED)));
    assert(soft == 0);
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_consonant_at();
    test_passing_tone();
    test_neighbor_must_resolve();
    test_nota_cambiata();
    test_unfigured_dissonance();
    test_suspension();
    test_invalid();
    test_check();
    printf("test_century_dissonance_rules: ok\n");
    return 0;
}
