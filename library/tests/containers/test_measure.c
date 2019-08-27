#include "containers/measure.h"

#include <assert.h>
#include <stddef.h>
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

int main(void) {
    test_create_rejects_null_id();
    test_create_rejects_null_meter_map_region_ref();
    test_create_accepts_valid();
    test_id_accessor_returns_constructed_id();
    test_meter_map_region_ref_accessor();
    test_identity_equals_same_id();
    test_identity_equals_regardless_of_instance();
    test_identity_not_equal_with_different_id();
    return 0;
}