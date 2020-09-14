#include <assert.h>
#include <stdio.h>

#include "beaming_rules.h"
#include "meter_test_util.h"
#include "rule_check.h"

static NoteValue *value(int exponent) {
    NoteValue *note_value = NULL;
    assert(note_value_create(exponent, &note_value) == NOTE_VALUE_OK);
    return note_value;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_meter_beaming_rule, standard_meter_beaming_constraint,
                                    STANDARD_METER_BEAMING_RULE_COUNT);
}

/* Quarter notes and longer are never beamed. */
static void test_beamable(void) {
    for (int exponent = NOTE_VALUE_MIN_EXPONENT; exponent <= NOTE_VALUE_MAX_EXPONENT; exponent++) {
        NoteValue *note_value = value(exponent);
        assert(standard_meter_value_beamable(note_value) == (exponent >= 3));
        note_value_destroy(note_value);
    }
    assert(!standard_meter_value_beamable(NULL));
}

/* Four sixteenths per quarter-note beat, two per eighth-note beat. */
static void test_notes_per_beat(void) {
    NoteValue *sixteenth = value(4);
    NoteValue *eighth = value(3);
    NoteValue *quarter = value(2);
    unsigned long count = 0;

    TimeSignature *four_four = make_signature(4, 2);
    assert(standard_meter_notes_per_beat(four_four, sixteenth, &count) && count == 4);
    assert(standard_meter_notes_per_beat(four_four, eighth, &count) && count == 2);
    assert(standard_meter_notes_per_beat(four_four, quarter, &count) && count == 1);

    TimeSignature *four_eight = make_signature(4, 3);
    assert(standard_meter_notes_per_beat(four_eight, sixteenth, &count) && count == 2);
    assert(standard_meter_notes_per_beat(four_eight, eighth, &count) && count == 1);

    /* a value longer than the beat does not divide it */
    count = 9;
    assert(!standard_meter_notes_per_beat(four_eight, quarter, &count));
    assert(count == 9);

    NoteValue *dotted = NULL;
    assert(note_value_create_dotted(4, 1, &dotted) == NOTE_VALUE_OK);
    assert(!standard_meter_notes_per_beat(four_four, dotted, &count));
    assert(!standard_meter_notes_per_beat(NULL, sixteenth, &count));

    note_value_destroy(sixteenth);
    note_value_destroy(eighth);
    note_value_destroy(quarter);
    note_value_destroy(dotted);
    time_signature_destroy(four_four);
    time_signature_destroy(four_eight);
}

static bool within(const TimeSignature *time_signature, long start_n, unsigned long start_d, long length_n,
                   unsigned long length_d) {
    Duration *start = make_duration(start_n, start_d);
    Duration *length = make_duration(length_n, length_d);
    bool result = standard_meter_beam_group_within_beat(time_signature, start, length);
    duration_destroy(start);
    duration_destroy(length);
    return result;
}

/* Four sixteenths make one beat in x/4; in x/8 they are two beats. */
static void test_beam_group_within_beat(void) {
    TimeSignature *four_four = make_signature(4, 2);
    assert(within(four_four, 0, 1, 1, 4));  /* four sixteenths, beat 1 */
    assert(within(four_four, 1, 4, 1, 4));  /* beat 2 */
    assert(within(four_four, 1, 4, 1, 8));  /* two sixteenths inside beat 2 */
    assert(!within(four_four, 1, 8, 1, 4)); /* crosses the boundary into beat 2 */
    assert(!within(four_four, 0, 1, 3, 8)); /* three eighths span beats 1-2 */
    /* mixed values: an eighth and two sixteenths share a quarter-note beat */
    assert(within(four_four, 1, 2, 1, 4));
    time_signature_destroy(four_four);

    TimeSignature *four_eight = make_signature(4, 3);
    assert(within(four_eight, 0, 1, 1, 8));  /* two sixteenths, beat 1 */
    assert(!within(four_eight, 0, 1, 1, 4)); /* four sixteenths cross into beat 2 */
    assert(within(four_eight, 1, 8, 1, 8));
    time_signature_destroy(four_eight);
}

static void test_beam_group_declines(void) {
    TimeSignature *four_four = make_signature(4, 2);
    assert(!within(four_four, 1, 1, 1, 8)); /* starts after the measure */
    assert(!within(four_four, 0, 1, 0, 1)); /* empty group */
    assert(!standard_meter_beam_group_within_beat(NULL, NULL, NULL));
    time_signature_destroy(four_four);
}

int main(void) {
    test_rules_reference_framework();
    test_beamable();
    test_notes_per_beat();
    test_beam_group_within_beat();
    test_beam_group_declines();
    printf("test_beaming_rules (standard-meter): ok\n");
    return 0;
}
