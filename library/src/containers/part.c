#include "containers/part.h"

#include <stdlib.h>

#include "containers/id_array.h"

struct Part {
    EntityID *id;
    EntityID **staff_ids;
    size_t staff_id_count;
};

PartResult part_create(EntityID *id, EntityID **staff_ids, size_t staff_id_count, Part **out) {
    EntityID **copy = NULL;
    switch (container_shell_create(id, staff_ids, staff_id_count, &copy)) {
        case CONTAINER_SHELL_OK:
            break;
        case CONTAINER_SHELL_ERR_NULL_OWNER:
            return PART_ERR_NULL_ID;
        case CONTAINER_SHELL_ERR_NULL_ID:
            return PART_ERR_NULL_STAFF_ID;
        default:
            return PART_ERR_ALLOC;
    }

    Part *part = (Part *)malloc(sizeof(Part));
    if (part == NULL) {
        free(copy);
        return PART_ERR_ALLOC;
    }

    part->staff_ids = copy;
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
    id_array_destroy(part->staff_ids, part->staff_id_count);
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