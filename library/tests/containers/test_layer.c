#include "containers/layer.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static Layer *make_layer(const char *id_value, const char *grouping) {
    EntityID *id = NULL;
    entity_id_create(id_value, &id);

    Layer *layer = NULL;
    layer_create(id, grouping, &layer);
    return layer;
}

static void test_create_rejects_null_id(void) {
    Layer *layer = NULL;
    LayerResult r = layer_create(NULL, "ossia", &layer);
    assert(r == LAYER_ERR_NULL_ID);
    assert(layer == NULL);
}

static void test_create_rejects_null_grouping(void) {
    EntityID *id = NULL;
    entity_id_create("layer-1", &id);

    Layer *layer = NULL;
    LayerResult r = layer_create(id, NULL, &layer);
    assert(r == LAYER_ERR_NULL_GROUPING);
    assert(layer == NULL);

    entity_id_destroy(id);
}

static void test_create_rejects_empty_grouping(void) {
    EntityID *id = NULL;
    entity_id_create("layer-1", &id);

    Layer *layer = NULL;
    LayerResult r = layer_create(id, "", &layer);
    assert(r == LAYER_ERR_EMPTY_GROUPING);
    assert(layer == NULL);

    entity_id_destroy(id);
}

static void test_create_accepts_valid(void) {
    Layer *layer = make_layer("layer-1", "ossia");
    assert(layer != NULL);
    layer_destroy(layer);
}

static void test_id_accessor_returns_constructed_id(void) {
    Layer *layer = make_layer("layer-1", "ossia");
    const EntityID *id = layer_id(layer);
    assert(id != NULL);
    assert(strcmp(entity_id_value(id), "layer-1") == 0);
    layer_destroy(layer);
}

static void test_grouping_accessor_returns_constructed_value(void) {
    Layer *layer = make_layer("layer-1", "ossia");
    assert(strcmp(layer_grouping(layer), "ossia") == 0);
    layer_destroy(layer);
}

static void test_identity_equals_same_id(void) {
    Layer *a = make_layer("layer-1", "ossia");
    Layer *b = make_layer("layer-1", "cue");
    assert(layer_identity_equals(a, b));
    layer_destroy(a);
    layer_destroy(b);
}

static void test_identity_equals_regardless_of_instance(void) {
    Layer *a = make_layer("layer-1", "ossia");
    Layer *b = make_layer("layer-1", "ossia");
    assert(a != b);
    assert(layer_identity_equals(a, b));
    layer_destroy(a);
    layer_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    Layer *a = make_layer("layer-1", "ossia");
    Layer *b = make_layer("layer-2", "ossia");
    assert(!layer_identity_equals(a, b));
    layer_destroy(a);
    layer_destroy(b);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_rejects_null_grouping();
    test_create_rejects_empty_grouping();
    test_create_accepts_valid();
    test_id_accessor_returns_constructed_id();
    test_grouping_accessor_returns_constructed_value();
    test_identity_equals_same_id();
    test_identity_equals_regardless_of_instance();
    test_identity_not_equal_with_different_id();
    return 0;
}