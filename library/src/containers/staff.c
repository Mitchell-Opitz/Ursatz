#include "containers/staff.h"

#include <stdlib.h>

#include "containers/id_array.h"

struct Staff {
    EntityID *id;
    EntityID **voice_ids;
    size_t voice_id_count;
    EntityID **layer_ids;
    size_t layer_id_count;
};

StaffResult staff_create(EntityID *id, EntityID **voice_ids, size_t voice_id_count, EntityID **layer_ids,
                          size_t layer_id_count, Staff **out) {
    if (id == NULL) {
        return STAFF_ERR_NULL_ID;
    }
    for (size_t i = 0; i < voice_id_count; i++) {
        if (voice_ids[i] == NULL) {
            return STAFF_ERR_NULL_VOICE_ID;
        }
    }
    for (size_t i = 0; i < layer_id_count; i++) {
        if (layer_ids[i] == NULL) {
            return STAFF_ERR_NULL_LAYER_ID;
        }
    }

    Staff *staff = (Staff *)malloc(sizeof(Staff));
    if (staff == NULL) {
        return STAFF_ERR_ALLOC;
    }

    staff->voice_ids = id_array_copy(voice_ids, voice_id_count);
    if (voice_id_count > 0 && staff->voice_ids == NULL) {
        free(staff);
        return STAFF_ERR_ALLOC;
    }

    staff->layer_ids = id_array_copy(layer_ids, layer_id_count);
    if (layer_id_count > 0 && staff->layer_ids == NULL) {
        free(staff->voice_ids);
        free(staff);
        return STAFF_ERR_ALLOC;
    }

    staff->id = id;
    staff->voice_id_count = voice_id_count;
    staff->layer_id_count = layer_id_count;

    *out = staff;
    return STAFF_OK;
}

void staff_destroy(Staff *staff) {
    if (staff == NULL) {
        return;
    }
    entity_id_destroy(staff->id);
    id_array_destroy(staff->voice_ids, staff->voice_id_count);
    id_array_destroy(staff->layer_ids, staff->layer_id_count);
    free(staff);
}

const EntityID *staff_id(const Staff *staff) {
    return staff->id;
}

size_t staff_voice_id_count(const Staff *staff) {
    return staff->voice_id_count;
}

const EntityID *staff_voice_id_at(const Staff *staff, size_t index) {
    return staff->voice_ids[index];
}

size_t staff_layer_id_count(const Staff *staff) {
    return staff->layer_id_count;
}

const EntityID *staff_layer_id_at(const Staff *staff, size_t index) {
    return staff->layer_ids[index];
}

bool staff_identity_equals(const Staff *a, const Staff *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}