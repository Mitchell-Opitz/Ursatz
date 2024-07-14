#include <assert.h>
#include <stdio.h>

#include "hemiola_rules.h"
#include "meter_test_util.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_meter_hemiola_rule, standard_meter_hemiola_constraint,
                                    STANDARD_METER_HEMIOLA_RULE_COUNT);
}

static void window(unsigned beats, int exponent, bool expected, long pulse_n, unsigned long pulse_d, unsigned measures) {
    TimeSignature *signature = make_signature(beats, exponent);
    Duration *pulse = NULL;
    unsigned got_measures = 99;
    bool ok = standard_meter_hemiola_window(signature, &pulse, &got_measures);
    assert(ok == expected);
    if (expected) {
        assert_duration(pulse, pulse_n, pulse_d);
        assert(got_measures == measures);
        duration_destroy(pulse);
    } else {
        assert(pulse == NULL && got_measures == 99);
    }
    time_signature_destroy(signature);
}

static void test_window(void) {
    window(3, 2, true, 1, 4, 2);  /* 3/4: two measures of quarter-note pulses */
    window(3, 1, true, 1, 2, 2);  /* 3/2 */
    window(6, 3, true, 1, 8, 1);  /* 6/8: one measure of eighth-note pulses */
    window(4, 2, false, 0, 1, 0); /* 4/4 */
    window(12, 3, false, 0, 1, 0); /* 12/8 */
    window(9, 3, false, 0, 1, 0); /* 9/8 */
    window(2, 2, false, 0, 1, 0); /* 2/4 */
    Duration *pulse = NULL;
    unsigned measures = 0;
    assert(!standard_meter_hemiola_window(NULL, &pulse, &measures));
}

static void test_evidence(void) {
    const bool duple[6] = {true, false, true, false, true, false};
    const bool triple[6] = {true, false, false, true, false, false};
    const bool walking[6] = {true, true, true, true, true, true};
    const bool none[6] = {false, false, false, false, false, false};
    assert(standard_meter_hemiola_duration_evidence(duple));
    assert(!standard_meter_hemiola_duration_evidence(triple));
    assert(!standard_meter_hemiola_duration_evidence(walking));
    assert(!standard_meter_hemiola_duration_evidence(none));
    assert(standard_meter_hemiola_chord_evidence(duple));
    assert(!standard_meter_hemiola_chord_evidence(triple));
    assert(!standard_meter_hemiola_chord_evidence(NULL));
    const bool long_notes[6] = {true, false, true, false, true, false};
    const bool attacks_without_three[6] = {true, true, true, false, true, true}; /* ornaments around the long notes */
    assert(standard_meter_hemiola_accent_evidence(long_notes, duple));
    assert(standard_meter_hemiola_accent_evidence(long_notes, attacks_without_three));
    assert(!standard_meter_hemiola_accent_evidence(long_notes, walking));
    assert(!standard_meter_hemiola_accent_evidence(none, duple));
}

static void test_evidence_count(void) {
    assert(standard_meter_hemiola_evidence_met(2, 3, 2));
    assert(standard_meter_hemiola_evidence_met(2, 2, 2));
    assert(standard_meter_hemiola_evidence_met(3, 3, 2));
    assert(!standard_meter_hemiola_evidence_met(1, 3, 2));
    assert(!standard_meter_hemiola_evidence_met(1, 1, 2)); /* one type alone is never enough */
    assert(!standard_meter_hemiola_evidence_met(2, 1, 2));
    assert(standard_meter_hemiola_evidence_met(1, 1, 0));  /* a minimum of 0 reads as 1 */
    assert(!standard_meter_hemiola_evidence_met(0, 3, 0));
}

int main(void) {
    test_rules_reference_framework();
    test_window();
    test_evidence();
    test_evidence_count();
    printf("test_hemiola_rules (standard-meter): ok\n");
    return 0;
}
