#include "theory/heuristic.h"

#include <stdlib.h>
#include <string.h>

struct Heuristic {
    EntityID *id;
    char *label;
};

HeuristicResult heuristic_create(EntityID *id, const char *label, Heuristic **out) {
    if (id == NULL) {
        return HEURISTIC_ERR_NULL_ID;
    }
    if (label == NULL || label[0] == '\0') {
        return HEURISTIC_ERR_NULL_LABEL;
    }

    Heuristic *heuristic = (Heuristic *)malloc(sizeof(Heuristic));
    if (heuristic == NULL) {
        return HEURISTIC_ERR_ALLOC;
    }

    size_t len = strlen(label);
    heuristic->label = (char *)malloc(len + 1);
    if (heuristic->label == NULL) {
        free(heuristic);
        return HEURISTIC_ERR_ALLOC;
    }
    memcpy(heuristic->label, label, len + 1);

    heuristic->id = id;

    *out = heuristic;
    return HEURISTIC_OK;
}

void heuristic_destroy(Heuristic *heuristic) {
    if (heuristic == NULL) {
        return;
    }
    entity_id_destroy(heuristic->id);
    free(heuristic->label);
    free(heuristic);
}

const EntityID *heuristic_id(const Heuristic *heuristic) {
    return heuristic->id;
}

const char *heuristic_label(const Heuristic *heuristic) {
    return heuristic->label;
}

bool heuristic_identity_equals(const Heuristic *a, const Heuristic *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}