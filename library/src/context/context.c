#include "context/context.h"

#include <stdlib.h>
#include <string.h>

struct Context {
    EntityID *id;
    char *label;
    const Context *parent;
};

ContextResult context_create(EntityID *id, const char *label, const Context *parent, Context **out) {
    if (id == NULL) {
        return CONTEXT_ERR_NULL_ID;
    }
    if (label == NULL || label[0] == '\0') {
        return CONTEXT_ERR_NULL_LABEL;
    }

    Context *context = (Context *)malloc(sizeof(Context));
    if (context == NULL) {
        return CONTEXT_ERR_ALLOC;
    }

    size_t len = strlen(label);
    context->label = (char *)malloc(len + 1);
    if (context->label == NULL) {
        free(context);
        return CONTEXT_ERR_ALLOC;
    }
    memcpy(context->label, label, len + 1);

    context->id = id;
    context->parent = parent;

    *out = context;
    return CONTEXT_OK;
}

void context_destroy(Context *context) {
    if (context == NULL) {
        return;
    }
    entity_id_destroy(context->id);
    free(context->label);
    free(context);
}

const EntityID *context_id(const Context *context) {
    return context->id;
}

const char *context_label(const Context *context) {
    return context->label;
}

const Context *context_parent(const Context *context) {
    return context->parent;
}

bool context_identity_equals(const Context *a, const Context *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}