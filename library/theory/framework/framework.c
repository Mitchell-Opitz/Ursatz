#include "theory/framework/framework.h"
#include "theory/framework/framework_internal.h"

#include <stdlib.h>
#include <string.h>

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

const EntityID *framework_id(const Framework *framework) {
    return framework->id;
}

const char *framework_name(const Framework *framework) {
    return framework->name;
}
