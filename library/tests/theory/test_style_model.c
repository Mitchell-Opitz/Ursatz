#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "theory/style_model.h"

/*
 * Construction/shape test: a StyleModel holds identity, style_name,
 * style_features (opaque, corpus-derived), training_corpus (opaque
 * label, not a music-corpus reference), and model_version -- all
 * round-trip unchanged. Shape only in this pass; no fitting/comparison
 * logic exists to test.
 */

int main(void) {
    Value *features;
    assert(value_create("style-features", "voice-leading-profile", 22, &features) == VALUE_OK);

    EntityID *id;
    assert(entity_id_create("style-model-bach-chorale", &id) == ENTITY_ID_OK);

    StyleModel *model;
    assert(style_model_create(id, "Bach chorale style", features, "bach-chorales-v1", "v0.1.0", &model) ==
           STYLE_MODEL_OK);
    assert(strcmp(style_model_style_name(model), "Bach chorale style") == 0);
    assert(style_model_style_features(model) == features);
    assert(strcmp(style_model_training_corpus(model), "bach-chorales-v1") == 0);
    assert(strcmp(style_model_model_version(model), "v0.1.0") == 0);

    EntityID *other_id;
    assert(entity_id_create("style-model-other", &other_id) == ENTITY_ID_OK);
    Value *other_features;
    assert(value_create("style-features", "x", 1, &other_features) == VALUE_OK);
    StyleModel *other;
    assert(style_model_create(other_id, "other style", other_features, "corpus-b", "v0.1.0", &other) ==
           STYLE_MODEL_OK);

    assert(style_model_identity_equals(model, model));
    assert(!style_model_identity_equals(model, other));

    style_model_destroy(model);
    style_model_destroy(other);

    printf("test_style_model: ok\n");
    return 0;
}