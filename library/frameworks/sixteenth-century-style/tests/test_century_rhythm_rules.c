#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "century_rhythm_rules.h"
#include "century_rule_check.h"
#include "century_test_util.h"

#define RHYTHM(name) SIXTEENTH_CENTURY_RHYTHM_##name

static bool holds(SixteenthCenturyRhythmRule rule, const SixteenthCenturyScore *score) {
    return sixteenth_century_rhythm_rule_holds(rule, score);
}

#define ONE_VOICE(name, slots)                                                  \
    StandardCounterpointSlotLine name##_voices[] = {{(slots), COUNT_OF(slots)}}; \
    SCORE_OF(name, name##_voices)

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(sixteenth_century_rhythm_rule, sixteenth_century_rhythm_constraint,
                                    SIXTEENTH_CENTURY_RHYTHM_RULE_COUNT);
    for (int i = 0; i < SIXTEENTH_CENTURY_RHYTHM_RULE_COUNT; i++) {
        assert(constraint_type(sixteenth_century_rhythm_constraint((SixteenthCenturyRhythmRule)i)) ==
               CONSTRAINT_TYPE_SOFT);
    }
}

static void test_names(void) {
    assert(strcmp(sixteenth_century_rhythm_rule_name(RHYTHM(SHORT_NOTES_OFF_BEAT)), "short-notes-off-beat") == 0);
    assert(strcmp(sixteenth_century_rhythm_rule_name(RHYTHM(SLOW_START_END)), "slow-start-end") == 0);
    assert(sixteenth_century_rhythm_rule_name(RHYTHM(RULE_COUNT)) == NULL);
}

static void test_short_notes_off_beat(void) {
    const PitchSpelling *c4 = P(C, 0, 4);
    const PitchSpelling *d4 = P(D, 0, 4);
    const PitchSpelling *e4 = P(E, 0, 4);
    const PitchSpelling *f4 = P(F, 0, 4);

    /* A dotted half, then two eighths between tactus starts, then a half. */
    StandardCounterpointSlot dotted[] = {ATK(c4), T3(c4), T2(c4), EIGHTH(d4), EIGHTH(e4), HALF(f4)};
    ONE_VOICE(dotted_score, dotted);
    assert(holds(RHYTHM(SHORT_NOTES_OFF_BEAT), &dotted_score));

    /* Quarter notes on the tactus starts. */
    StandardCounterpointSlot quarters[] = {QUARTER(c4), QUARTER(d4), HALF(e4)};
    ONE_VOICE(quarters_score, quarters);
    assert(!holds(RHYTHM(SHORT_NOTES_OFF_BEAT), &quarters_score));
}

static void test_slow_start_end(void) {
    const PitchSpelling *c4 = P(C, 0, 4);
    const PitchSpelling *d4 = P(D, 0, 4);

    StandardCounterpointSlot slow[] = {REST4(), HALF(c4), QUARTER(d4), QUARTER(c4), HALF(d4)};
    ONE_VOICE(slow_score, slow);
    assert(holds(RHYTHM(SLOW_START_END), &slow_score));

    StandardCounterpointSlot quick_start[] = {QUARTER(c4), QUARTER(d4), HALF(c4)};
    ONE_VOICE(quick_start_score, quick_start);
    assert(!holds(RHYTHM(SLOW_START_END), &quick_start_score));

    StandardCounterpointSlot quick_end[] = {HALF(c4), QUARTER(d4), QUARTER(c4)};
    ONE_VOICE(quick_end_score, quick_end);
    assert(!holds(RHYTHM(SLOW_START_END), &quick_end_score));
}

static void test_invalid(void) {
    const PitchSpelling *c4 = P(C, 0, 4);
    StandardCounterpointSlot slots[] = {HALF(c4)};
    ONE_VOICE(score, slots);
    assert(!holds(RHYTHM(RULE_COUNT), &score));
    assert(!holds((SixteenthCenturyRhythmRule)-1, &score));
    assert(!holds(RHYTHM(SLOW_START_END), NULL));
    unsigned hard = 7, soft = 7;
    assert(!sixteenth_century_rhythm_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!sixteenth_century_rhythm_check(&score, NULL, &soft));
    assert(!sixteenth_century_rhythm_check(&score, &hard, NULL));
}

static void test_check(void) {
    const PitchSpelling *c4 = P(C, 0, 4);
    const PitchSpelling *d4 = P(D, 0, 4);
    StandardCounterpointSlot slots[] = {QUARTER(c4), QUARTER(d4), HALF(c4)};
    ONE_VOICE(score, slots);
    unsigned hard = 7, soft = 0;
    assert(sixteenth_century_rhythm_check(&score, &hard, &soft));
    assert(hard == 0);
    assert(soft == ((1u << RHYTHM(SHORT_NOTES_OFF_BEAT)) | (1u << RHYTHM(SLOW_START_END))));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_short_notes_off_beat();
    test_slow_start_end();
    test_invalid();
    test_check();
    printf("test_century_rhythm_rules: ok\n");
    return 0;
}
