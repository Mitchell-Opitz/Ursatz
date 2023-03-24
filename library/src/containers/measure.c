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

MeasureResult measure_resolve_region(const Measure *measure, const MeterMap *map, const MeterMapRegion **out) {
    if (measure == NULL || map == NULL || out == NULL) {
        return MEASURE_ERR_NULL_ARGUMENT;
    }
    MeterMapResult found = meter_map_region_by_id(map, measure->meter_map_region_ref, out);
    if (found == METER_MAP_OK) {
        return MEASURE_OK;
    }
    return found == METER_MAP_ERR_NOT_FOUND ? MEASURE_ERR_REGION_NOT_FOUND : MEASURE_ERR_METER_MAP;
}

void measure_entries_destroy(MeasureEntries *entries) {
    if (entries == NULL) {
        return;
    }
    for (size_t i = 0; i < entries->count; i++) {
        time_span_destroy(entries->entries[i].span);
        entity_id_destroy(entries->entries[i].region_ref);
    }
    free(entries->entries);
    entries->entries = NULL;
    entries->count = 0;
}

MeasureResult measure_enumerate(const MeterMap *map, const MusicalTime *end_time, MeasureEntries *out) {
    if (map == NULL || end_time == NULL || out == NULL) {
        return MEASURE_ERR_NULL_ARGUMENT;
    }

    size_t count = 0;
    if (meter_map_measure_count(map, end_time, &count) != METER_MAP_OK) {
        return MEASURE_ERR_METER_MAP;
    }

    MeasureEntries result = {NULL, 0};
    if (count > 0) {
        result.entries = (MeasureEntry *)calloc(count, sizeof(MeasureEntry));
        if (result.entries == NULL) {
            return MEASURE_ERR_ALLOC;
        }
    }

    int first = meter_map_first_measure_number(map);
    for (size_t i = 0; i < count; i++) {
        MeasureEntry *entry = &result.entries[i];
        entry->index = first + (int)i;

        const MeterMapRegion *region = NULL;
        if (meter_map_measure_span(map, entry->index, &entry->span) != METER_MAP_OK ||
            meter_map_region_of_measure(map, entry->index, &region) != METER_MAP_OK) {
            measure_entries_destroy(&result);
            return MEASURE_ERR_METER_MAP;
        }
        if (entity_id_create(entity_id_value(meter_map_region_id(region)), &entry->region_ref) != ENTITY_ID_OK) {
            measure_entries_destroy(&result);
            return MEASURE_ERR_ALLOC;
        }
        result.count = i + 1;
    }

    *out = result;
    return MEASURE_OK;
}
