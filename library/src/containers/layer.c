#include "containers/layer.h"

#include <stdlib.h>
#include <string.h>

struct Layer {
    EntityID *id;
    char *grouping;
};

LayerResult layer_create(EntityID *id, const char *grouping, Layer **out) {
    if (id == NULL) {
        return LAYER_ERR_NULL_ID;
    }
    if (grouping == NULL) {
        return LAYER_ERR_NULL_GROUPING;
    }
    if (grouping[0] == '\0') {
        return LAYER_ERR_EMPTY_GROUPING;
    }

    Layer *layer = (Layer *)malloc(sizeof(Layer));
    if (layer == NULL) {
        return LAYER_ERR_ALLOC;
    }

    size_t grouping_len = strlen(grouping);
    layer->grouping = (char *)malloc(grouping_len + 1);
    if (layer->grouping == NULL) {
        free(layer);
        return LAYER_ERR_ALLOC;
    }
    memcpy(layer->grouping, grouping, grouping_len + 1);

    layer->id = id;

    *out = layer;
    return LAYER_OK;
}

void layer_destroy(Layer *layer) {
    if (layer == NULL) {
        return;
    }
    entity_id_destroy(layer->id);
    free(layer->grouping);
    free(layer);
}

const EntityID *layer_id(const Layer *layer) {
    return layer->id;
}

const char *layer_grouping(const Layer *layer) {
    return layer->grouping;
}

bool layer_identity_equals(const Layer *a, const Layer *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}