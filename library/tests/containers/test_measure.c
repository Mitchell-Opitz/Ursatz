#include "containers/measure.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static Measure *make_measure(const char *id_value, const char *region_value) {
    EntityID *id = NULL;
    entity_id_create(id_value, &id);
    EntityID *region_ref = NULL;
    entity_id_create(region_value, &region_ref);

    Measure *measure = NULL;
    measure_create(id, region_ref, &measure);
    return measure;
}

static void test_create_rejects_null_id(void) {
    EntityID *region_ref = NULL;
    entity_id_create("meter-region-1", &region_ref);

    Measure *measure = NULL;
    MeasureResult r = measure_create(NULL, region_ref, &measure);
    assert(r == MEASURE_ERR_NULL_ID);
    assert(measure == NULL);

    entity_id_destroy(region_ref);
}

static void test_create_rejects_null_meter_map_region_ref(void) {
    EntityID *id = NULL;
    entity_id_create("measure-1", &id);

    Measure *measure = NULL;
    MeasureResult r = measure_create(id, NULL, &measure);
    assert(r == MEASURE_ERR_NULL_METER_MAP_REGION_REF);
    assert(measure == NULL);

    entity_id_destroy(id);
}

static void test_create_accepts_valid(void) {
    Measure *measure = make_measure("measure-1", "meter-region-1");
    assert(measure != NULL);
    measure_destroy(measure);
}

static void test_id_accessor_returns_constructed_id(void) {
    Measure *measure = make_measure("measure-1", "meter-region-1");
    const EntityID *id = measure_id(measure);
    assert(id != NULL);
    assert(strcmp(entity_id_value(id), "measure-1") == 0);
    measure_destroy(measure);
}

static void test_meter_map_region_ref_accessor(void) {
    Measure *measure = make_measure("measure-1", "meter-region-1");
    const EntityID *region_ref = measure_meter_map_region_ref(measure);
    assert(region_ref != NULL);
    assert(strcmp(entity_id_value(region_ref), "meter-region-1") == 0);
    measure_destroy(measure);
}

static void test_identity_equals_same_id(void) {
    Measure *a = make_measure("measure-1", "meter-region-1");
    Measure *b = make_measure("measure-1", "meter-region-2");
    assert(measure_identity_equals(a, b));
    measure_destroy(a);
    measure_destroy(b);
}

static void test_identity_equals_regardless_of_instance(void) {
    Measure *a = make_measure("measure-1", "meter-region-1");
    Measure *b = make_measure("measure-1", "meter-region-1");
    assert(a != b);
    assert(measure_identity_equals(a, b));
    measure_destroy(a);
    measure_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    Measure *a = make_measure("measure-1", "meter-region-1");
    Measure *b = make_measure("measure-2", "meter-region-1");
    assert(!measure_identity_equals(a, b));
    measure_destroy(a);
    measure_destroy(b);
}

/* --- resolving against a MeterMap --- */

static TimeSignature *make_signature(unsigned top, int bottom_exponent) {
    NoteValue *unit = NULL;
    note_value_create(bottom_exponent, &unit);
    TimeSignature *ts = NULL;
    time_signature_create(top, unit, &ts);
    note_value_destroy(unit);
    return ts;
}

static MusicalTime *make_time(long n, unsigned long d) {
    MusicalTime *t = NULL;
    musical_time_create(n, d, &t);
    return t;
}

/* 4/4 from 0, 3/4 from four whole notes (measure 5). */
static MeterMap *make_map(void) {
    TimeSignature *four_four = make_signature(4, 2);
    TimeSignature *three_four = make_signature(3, 2);
    MusicalTime *zero = make_time(0, 1);
    MusicalTime *four = make_time(4, 1);
    MeterMapChangePoint points[2] = {{four_four, zero}, {three_four, four}};
    EntityID *id = NULL;
    entity_id_create("map", &id);
    MeterMap *map = NULL;
    MeterMapResult r = meter_map_create(id, points, 2, &map);
    assert(r == METER_MAP_OK);
    time_signature_destroy(four_four);
    time_signature_destroy(three_four);
    musical_time_destroy(zero);
    musical_time_destroy(four);
    return map;
}

static void test_resolves_region_ref_against_meter_map(void) {
    MeterMap *map = make_map();
    const MeterMapRegion *second = meter_map_region(map, 1);

    EntityID *id = NULL;
    entity_id_create("measure-5", &id);
    EntityID *ref = NULL;
    entity_id_create(entity_id_value(meter_map_region_id(second)), &ref);
    Measure *measure = NULL;
    assert(measure_create(id, ref, &measure) == MEASURE_OK);

    const MeterMapRegion *resolved = NULL;
    assert(measure_resolve_region(measure, map, &resolved) == MEASURE_OK);
    assert(resolved == second);
    assert(time_signature_top_number(meter_map_region_time_signature(resolved)) == 3);

    measure_destroy(measure);
    meter_map_destroy(map);
}

static void test_unknown_region_ref_does_not_resolve(void) {
    MeterMap *map = make_map();
    Measure *measure = make_measure("measure-1", "meter-region-1");
    const MeterMapRegion *resolved = NULL;
    assert(measure_resolve_region(measure, map, &resolved) == MEASURE_ERR_REGION_NOT_FOUND);
    assert(measure_resolve_region(NULL, map, &resolved) == MEASURE_ERR_NULL_ARGUMENT);
    measure_destroy(measure);
    meter_map_destroy(map);
}

static void test_enumerates_measures_with_index_span_and_region(void) {
    MeterMap *map = make_map();

    /* To 4 + 3/2: measures 1-4 in 4/4, then 3/4 measures 5 and 6. */
    MusicalTime *end = make_time(11, 2);
    MeasureEntries entries = {0};
    assert(measure_enumerate(map, end, &entries) == MEASURE_OK);
    assert(entries.count == 6 + 0);
    for (size_t i = 0; i < entries.count; i++) {
        assert(entries.entries[i].index == (int)i + 1);
    }

    MusicalTime *start4 = make_time(3, 1);
    MusicalTime *end4 = make_time(4, 1);
    assert(musical_time_equals(time_span_start(entries.entries[3].span), start4));
    assert(musical_time_equals(time_span_end(entries.entries[3].span), end4));
    MusicalTime *end5 = make_time(19, 4);
    assert(musical_time_equals(time_span_end(entries.entries[4].span), end5));

    assert(entity_id_equals(entries.entries[0].region_ref, meter_map_region_id(meter_map_region(map, 0))));
    assert(entity_id_equals(entries.entries[3].region_ref, meter_map_region_id(meter_map_region(map, 0))));
    assert(entity_id_equals(entries.entries[4].region_ref, meter_map_region_id(meter_map_region(map, 1))));

    musical_time_destroy(start4);
    musical_time_destroy(end4);
    musical_time_destroy(end5);
    measure_entries_destroy(&entries);
    musical_time_destroy(end);
    meter_map_destroy(map);
}

static void test_enumerate_on_a_barline_adds_no_empty_measure(void) {
    MeterMap *map = make_map();
    MusicalTime *end = make_time(4, 1);
    MeasureEntries entries = {0};
    assert(measure_enumerate(map, end, &entries) == MEASURE_OK);
    assert(entries.count == 4);
    measure_entries_destroy(&entries);

    MusicalTime *zero = make_time(0, 1);
    assert(measure_enumerate(map, zero, &entries) == MEASURE_OK);
    assert(entries.count == 0 && entries.entries == NULL);
    assert(measure_enumerate(NULL, zero, &entries) == MEASURE_ERR_NULL_ARGUMENT);

    musical_time_destroy(zero);
    musical_time_destroy(end);
    meter_map_destroy(map);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_rejects_null_meter_map_region_ref();
    test_create_accepts_valid();
    test_id_accessor_returns_constructed_id();
    test_meter_map_region_ref_accessor();
    test_identity_equals_same_id();
    test_identity_equals_regardless_of_instance();
    test_identity_not_equal_with_different_id();
    test_resolves_region_ref_against_meter_map();
    test_unknown_region_ref_does_not_resolve();
    test_enumerates_measures_with_index_span_and_region();
    test_enumerate_on_a_barline_adds_no_empty_measure();
    return 0;
}