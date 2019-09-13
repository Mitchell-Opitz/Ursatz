#include "containers/source_variant.h"

#include <stdlib.h>
#include <string.h>

struct SourceVariant {
    EntityID *source_id;
    char *descriptor;
};

SourceVariantResult source_variant_create(EntityID *source_id, const char *descriptor, SourceVariant **out) {
    if (source_id == NULL) {
        return SOURCE_VARIANT_ERR_NULL_SOURCE_ID;
    }
    if (descriptor == NULL) {
        return SOURCE_VARIANT_ERR_NULL_DESCRIPTOR;
    }
    if (descriptor[0] == '\0') {
        return SOURCE_VARIANT_ERR_EMPTY_DESCRIPTOR;
    }

    SourceVariant *variant = (SourceVariant *)malloc(sizeof(SourceVariant));
    if (variant == NULL) {
        return SOURCE_VARIANT_ERR_ALLOC;
    }

    size_t descriptor_len = strlen(descriptor);
    variant->descriptor = (char *)malloc(descriptor_len + 1);
    if (variant->descriptor == NULL) {
        free(variant);
        return SOURCE_VARIANT_ERR_ALLOC;
    }
    memcpy(variant->descriptor, descriptor, descriptor_len + 1);

    variant->source_id = source_id;

    *out = variant;
    return SOURCE_VARIANT_OK;
}

void source_variant_destroy(SourceVariant *variant) {
    if (variant == NULL) {
        return;
    }
    entity_id_destroy(variant->source_id);
    free(variant->descriptor);
    free(variant);
}

const EntityID *source_variant_source_id(const SourceVariant *variant) {
    return variant->source_id;
}

const char *source_variant_descriptor(const SourceVariant *variant) {
    return variant->descriptor;
}

bool source_variant_equals(const SourceVariant *a, const SourceVariant *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->source_id, b->source_id) && strcmp(a->descriptor, b->descriptor) == 0;
}