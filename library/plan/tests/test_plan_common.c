#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <model/time/time_signature.h>

#include "plan/plan_common.h"
#include "ursatz_test_knowledge.h"

/*
 * The helpers the plan's files share (plan_common.h), each called here: the meter's signature, grid and pickup grid, the cadence
 * and lane tables, the rational result, the key helpers and the span overlap.
 */

static Rational fraction(long long num, long long den) {
    Rational value;
    assert(rational_make(num, den, &value) == RATIONAL_OK);
    return value;
}

static PlanMeter meter_of(int beats, int unit) {
    PlanMeter meter;
    memset(&meter, 0, sizeof(meter));
    meter.beats_per_bar = beats;
    meter.beat_unit = unit;
    return meter;
}

static PlanSpan span_of(long long start_num, long long start_den, long long length_num, long long length_den) {
    PlanSpan span = {fraction(start_num, start_den), fraction(length_num, length_den)};
    return span;
}

static void test_meter_signature(void) {
    TimeSignature *signature = NULL;
    assert(plan_meter_signature(meter_of(4, 4), &signature) && signature != NULL);
    time_signature_destroy(signature);
    signature = NULL;
    assert(plan_meter_signature(meter_of(3, 4), &signature) && signature != NULL);
    time_signature_destroy(signature);
    signature = NULL;
    assert(!plan_meter_signature(meter_of(4, 3), &signature) && signature == NULL);
    assert(!plan_meter_signature(meter_of(0, 4), &signature) && signature == NULL);
    assert(!plan_meter_signature(meter_of(4, 4), NULL));
}

static void test_meter_grid(void) {
    Rational beat;
    Rational division;
    assert(plan_meter_grid(meter_of(4, 4), &beat, &division));
    assert(rational_cmp(beat, fraction(1, 4)) == 0 && rational_cmp(division, fraction(1, 8)) == 0);
    assert(!plan_meter_grid(meter_of(4, 3), &beat, &division));
}

static void test_pickup_on_grid(void) {
    const PlanMeter four_four = meter_of(4, 4);
    assert(plan_meter_pickup_on_grid(four_four, fraction(0, 1)));
    assert(plan_meter_pickup_on_grid(four_four, fraction(1, 4)));
    assert(plan_meter_pickup_on_grid(four_four, fraction(1, 8)));
    assert(plan_meter_pickup_on_grid(four_four, fraction(1, 32)));
    assert(plan_meter_pickup_on_grid(four_four, fraction(1, 6)));
    assert(plan_meter_pickup_on_grid(four_four, fraction(1, 12)));
    assert(!plan_meter_pickup_on_grid(four_four, fraction(1, 48)));
    assert(!plan_meter_pickup_on_grid(four_four, fraction(1, 64)));
    assert(!plan_meter_pickup_on_grid(four_four, fraction(-1, 4)));
    const Rational no_denominator = {1, 0};
    assert(!plan_meter_pickup_on_grid(four_four, no_denominator));
    assert(!plan_meter_pickup_on_grid(meter_of(4, 3), fraction(1, 4)));
}

static void test_cadences(void) {
    static const CadenceType HELD[PLAN_CADENCE_COUNT] = {CADENCE_TYPE_PERFECT_AUTHENTIC, CADENCE_TYPE_IMPERFECT_AUTHENTIC,
                                                         CADENCE_TYPE_HALF, CADENCE_TYPE_DECEPTIVE, CADENCE_TYPE_PLAGAL};
    static const char *const NAMES[PLAN_CADENCE_COUNT] = {"PAC", "IAC", "HC", "DC", "PC"};
    for (size_t i = 0; i < PLAN_CADENCE_COUNT; i++) {
        size_t index = 99;
        assert(plan_cadence_index(HELD[i], &index) && index == i);
        assert(strcmp(plan_cadence_name(HELD[i]), NAMES[i]) == 0);
        assert(plan_cadence_index(HELD[i], NULL));
    }
    size_t index = 99;
    assert(!plan_cadence_index(CADENCE_TYPE_SUBVERTED, &index) && index == 99);
    assert(!plan_cadence_index(CADENCE_TYPE_AUTHENTIC_UNSPECIFIED, &index));
    assert(!plan_cadence_index(CADENCE_TYPE_UNKNOWN, &index));
    assert(strcmp(plan_cadence_name(CADENCE_TYPE_SUBVERTED), "") == 0);
}

static void test_keys(void) {
    const CommonPracticeMaterials *materials = test_materials();
    const SpelledKey major = plan_key_of_pitch_class(0, materials->major);
    const SpelledKey minor = plan_key_of_pitch_class(9, materials->natural_minor);
    SpelledKey none = major;
    none.material = NULL;

    assert(strcmp(plan_key_material_id(NULL), "omt.material.major") == 0);
    assert(strcmp(plan_key_material_id(&none), "omt.material.major") == 0);
    assert(strcmp(plan_key_material_id(&minor), materials->natural_minor->id) == 0);
    assert(strcmp(plan_key_word(NULL), "major") == 0);
    assert(strcmp(plan_key_word(&none), "major") == 0);
    assert(strcmp(plan_key_word(&minor), materials->natural_minor->key_word) == 0);

    assert(!plan_key_valid(NULL));
    assert(plan_key_valid(&none));
    assert(plan_key_valid(&major));
    assert(plan_key_valid(&minor));
    /* A material whose third is neither major nor minor is not one the plan holds. */
    PitchMaterial odd = *materials->major;
    odd.id = "omt.material.unknown";
    odd.steps[2] = 5;
    SpelledKey odd_key = major;
    odd_key.material = &odd;
    assert(!plan_key_valid(&odd_key));
}

static void test_lane_names(void) {
    static const char *const NAMES[PLAN_LANE_NAME_COUNT] = {"meter", "key", "harmony", "bass", "melody", "inner_voice"};
    for (int lane = 0; lane < PLAN_LANE_NAME_COUNT; lane++) {
        assert(strcmp(plan_lane_name((PlanLaneKind)lane), NAMES[lane]) == 0);
    }
    assert(strcmp(plan_lane_name((PlanLaneKind)-1), "") == 0);
    assert(strcmp(plan_lane_name((PlanLaneKind)PLAN_LANE_NAME_COUNT), "") == 0);
}

static void test_result_of_rational(void) {
    assert(plan_result_of_rational(RATIONAL_OK) == PLAN_OK);
    assert(plan_result_of_rational(RATIONAL_ERR_NULL) == PLAN_ERR_NULL);
    assert(plan_result_of_rational(RATIONAL_ERR_ZERO_DENOMINATOR) == PLAN_ERR_BAD_TIME);
    assert(plan_result_of_rational(RATIONAL_ERR_OVERFLOW) == PLAN_ERR_BAD_TIME);
}

static void test_spans_overlap(void) {
    assert(plan_spans_overlap(span_of(0, 1, 1, 2), span_of(1, 4, 1, 2)));
    assert(!plan_spans_overlap(span_of(0, 1, 1, 2), span_of(1, 2, 1, 2)));
    assert(plan_spans_overlap(span_of(0, 1, 1, 1), span_of(1, 4, 1, 4)));
    PlanSpan bad = span_of(0, 1, 1, 2);
    bad.length.den = 0;
    assert(!plan_spans_overlap(bad, span_of(0, 1, 1, 2)));
}

int main(void) {
    test_meter_signature();
    test_meter_grid();
    test_pickup_on_grid();
    test_cadences();
    test_keys();
    test_lane_names();
    test_result_of_rational();
    test_spans_overlap();
    printf("test_plan_common: ok\n");
    return 0;
}
