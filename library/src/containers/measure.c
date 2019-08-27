#include "containers/measure.h"

#include <stdlib.h>

struct Measure {
    EntityID *id;
    EntityID *meter_map_region_ref;
};

MeasureResult measure_create(EntityID *id, EntityID *meter_map_region_ref, Measure **out) {
    if (id == NULL) {
        return MEASURE_ERR_NULL_ID;
    }
    if (meter_map_region_ref == NULL) {
        return MEASURE_ERR_NULL_METER_MAP_REGION_REF;
    }

    Measure *measure = (Measure *)malloc(sizeof(Measure));
    if (measure == NULL) {
        return MEASURE_ERR_ALLOC;
    }

    measure->id = id;
    measure->meter_map_region_ref = meter_map_region_ref;

    *out = measure;
    return MEASURE_OK;
}

void measure_destroy(Measure *measure) {
    if (measure == NULL) {
        return;
    }
    entity_id_destroy(measure->id);
    entity_id_destroy(measure->meter_map_region_ref);
    free(measure);
}

const EntityID *measure_id(const Measure *measure) {
    return measure->id;
}

const EntityID *measure_meter_map_region_ref(const Measure *measure) {
    return measure->meter_map_region_ref;
}

bool measure_identity_equals(const Measure *a, const Measure *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}