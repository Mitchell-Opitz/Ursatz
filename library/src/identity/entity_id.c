#include "identity/entity_id.h"

#include <stdlib.h>
#include <string.h>

struct EntityID {
    char *value;
};

EntityIDResult entity_id_create(const char *value, EntityID **out) {
    if (value == NULL || value[0] == '\0') {
        return ENTITY_ID_ERR_EMPTY;
    }

    EntityID *id = (EntityID *)malloc(sizeof(EntityID));
    if (id == NULL) {
        return ENTITY_ID_ERR_ALLOC;
    }

    size_t len = strlen(value);
    id->value = (char *)malloc(len + 1);
    if (id->value == NULL) {
        free(id);
        return ENTITY_ID_ERR_ALLOC;
    }

    memcpy(id->value, value, len + 1);

    *out = id;
    return ENTITY_ID_OK;
}

void entity_id_destroy(EntityID *id) {
    if (id == NULL) {
        return;
    }
    free(id->value);
    free(id);
}

bool entity_id_equals(const EntityID *a, const EntityID *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return strcmp(a->value, b->value) == 0;
}

const char *entity_id_value(const EntityID *id) {
    return id->value;
}