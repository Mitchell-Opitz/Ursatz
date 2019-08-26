#include "containers/part.h"

#include <stdlib.h>

struct Part {
    EntityID *id;
    EntityID **staff_ids;
    size_t staff_id_count;
};

static EntityID **copy_id_array(EntityID **ids, size_t count) {
    if (count == 0) {
        return NULL;
    }
    EntityID **copy = (EntityID **)malloc(sizeof(EntityID *) * count);
    if (copy == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        copy[i] = ids[i];
    }
    return copy;
}

static void destroy_id_array(EntityID **ids, size_t count) {
    if (ids == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        entity_id_destroy(ids[i]);
    }
    free(ids);
}

PartResult part_create(EntityID *id, EntityID **staff_ids, size_t staff_id_count, Part **out) {
    if (id == NULL) {
        return PART_ERR_NULL_ID;
    }
    for (size_t i = 0; i < staff_id_count; i++) {
        if (staff_ids[i] == NULL) {
            return PART_ERR_NULL_STAFF_ID;
        }
    }

    Part *part = (Part *)malloc(sizeof(Part));
    if (part == NULL) {
        return PART_ERR_ALLOC;
    }

    part->staff_ids = copy_id_array(staff_ids, staff_id_count);
    if (staff_id_count > 0 && part->staff_ids == NULL) {
        free(part);
        return PART_ERR_ALLOC;
    }

    part->id = id;
    part->staff_id_count = staff_id_count;

    *out = part;
    return PART_OK;
}

void part_destroy(Part *part) {
    if (part == NULL) {
        return;
    }
    entity_id_destroy(part->id);
    destroy_id_array(part->staff_ids, part->staff_id_count);
    free(part);
}

const EntityID *part_id(const Part *part) {
    return part->id;
}

size_t part_staff_id_count(const Part *part) {
    return part->staff_id_count;
}

const EntityID *part_staff_id_at(const Part *part, size_t index) {
    return part->staff_ids[index];
}

bool part_identity_equals(const Part *a, const Part *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}