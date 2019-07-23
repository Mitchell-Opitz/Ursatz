#include "pitch/tuning_system.h"

#include <stdlib.h>
#include <string.h>

struct TuningSystem {
    EntityID *id;
    char *name;
};

TuningSystemResult tuning_system_create(EntityID *id, const char *name, TuningSystem **out) {
    if (id == NULL) {
        return TUNING_SYSTEM_ERR_NULL_ID;
    }
    if (name == NULL || name[0] == '\0') {
        return TUNING_SYSTEM_ERR_NULL_NAME;
    }

    TuningSystem *tuning_system = (TuningSystem *)malloc(sizeof(TuningSystem));
    if (tuning_system == NULL) {
        return TUNING_SYSTEM_ERR_ALLOC;
    }

    size_t len = strlen(name);
    tuning_system->name = (char *)malloc(len + 1);
    if (tuning_system->name == NULL) {
        free(tuning_system);
        return TUNING_SYSTEM_ERR_ALLOC;
    }
    memcpy(tuning_system->name, name, len + 1);

    tuning_system->id = id;

    *out = tuning_system;
    return TUNING_SYSTEM_OK;
}

void tuning_system_destroy(TuningSystem *tuning_system) {
    if (tuning_system == NULL) {
        return;
    }
    entity_id_destroy(tuning_system->id);
    free(tuning_system->name);
    free(tuning_system);
}

const EntityID *tuning_system_id(const TuningSystem *tuning_system) {
    return tuning_system->id;
}

const char *tuning_system_name(const TuningSystem *tuning_system) {
    return tuning_system->name;
}

bool tuning_system_identity_equals(const TuningSystem *a, const TuningSystem *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}