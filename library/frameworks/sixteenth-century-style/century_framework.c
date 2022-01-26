#include "century_framework.h"

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"

static Framework *g_framework = NULL;
static bool g_init_attempted = false;

const Framework *sixteenth_century_framework(void) {
    if (g_init_attempted) {
        return g_framework;
    }
    g_init_attempted = true;

    EntityID *id;
    if (entity_id_create("sixteenth-century-style-v1", &id) != ENTITY_ID_OK) {
        return NULL;
    }

    Framework *framework;
    if (framework_create(id, "sixteenth-century-style-v1", &framework) != FRAMEWORK_OK) {
        entity_id_destroy(id);
        return NULL;
    }

    g_framework = framework;
    return g_framework;
}
