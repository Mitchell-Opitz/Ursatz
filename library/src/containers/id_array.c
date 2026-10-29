#include "containers/id_array.h"

#include <stdlib.h>

EntityID **id_array_copy(EntityID **ids, size_t count) {
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

void id_array_destroy(EntityID **ids, size_t count) {
    if (ids == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        entity_id_destroy(ids[i]);
    }
    free(ids);
}

ContainerShellStatus container_shell_create(const void *owner, EntityID **ids, size_t count, EntityID ***copy) {
    if (owner == NULL) {
        return CONTAINER_SHELL_ERR_NULL_OWNER;
    }
    for (size_t i = 0; i < count; i++) {
        if (ids[i] == NULL) {
            return CONTAINER_SHELL_ERR_NULL_ID;
        }
    }
    *copy = id_array_copy(ids, count);
    if (count > 0 && *copy == NULL) {
        return CONTAINER_SHELL_ERR_ALLOC;
    }
    return CONTAINER_SHELL_OK;
}
