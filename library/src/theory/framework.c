#include "theory/framework.h"

#include <stdlib.h>
#include <string.h>

struct Framework {
    EntityID *id;
    char *name;
};

FrameworkResult framework_create(EntityID *id, const char *name, Framework **out) {
    if (id == NULL) {
        return FRAMEWORK_ERR_NULL_ID;
    }
    if (name == NULL || name[0] == '\0') {
        return FRAMEWORK_ERR_NULL_NAME;
    }

    Framework *framework = (Framework *)malloc(sizeof(Framework));
    if (framework == NULL) {
        return FRAMEWORK_ERR_ALLOC;
    }

    size_t len = strlen(name);
    framework->name = (char *)malloc(len + 1);
    if (framework->name == NULL) {
        free(framework);
        return FRAMEWORK_ERR_ALLOC;
    }
    memcpy(framework->name, name, len + 1);

    framework->id = id;

    *out = framework;
    return FRAMEWORK_OK;
}

void framework_destroy(Framework *framework) {
    if (framework == NULL) {
        return;
    }
    entity_id_destroy(framework->id);
    free(framework->name);
    free(framework);
}

const EntityID *framework_id(const Framework *framework) {
    return framework->id;
}

const char *framework_name(const Framework *framework) {
    return framework->name;
}

bool framework_identity_equals(const Framework *a, const Framework *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}