#include "galant_framework.h"

#include "theory/framework_singleton.h"

static FrameworkSingleton g_singleton = FRAMEWORK_SINGLETON_INIT("galant-schemas-v1");

const Framework *galant_schemas_framework(void) {
    return framework_singleton_get(&g_singleton);
}
