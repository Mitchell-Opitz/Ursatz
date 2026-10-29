#include "theory/framework_singleton.h"

#include <stddef.h>

#include "identity/entity_id.h"

static void create_framework(void *arg) {
    FrameworkSingleton *singleton = arg;

    EntityID *id;
    if (entity_id_create(singleton->id, &id) != ENTITY_ID_OK) {
        return;
    }

    Framework *framework;
    if (framework_create(id, singleton->id, &framework) != FRAMEWORK_OK) {
        entity_id_destroy(id);
        return;
    }

    singleton->framework = framework;
}

const Framework *framework_singleton_get(FrameworkSingleton *singleton) {
    if (singleton == NULL) {
        return NULL;
    }
    ursatz_once(&singleton->once, create_framework, singleton);
    return singleton->framework;
}
