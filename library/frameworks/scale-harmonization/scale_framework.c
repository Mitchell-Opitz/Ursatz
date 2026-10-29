#include "scale_framework.h"

#include "theory/framework_singleton.h"

static FrameworkSingleton g_singleton = FRAMEWORK_SINGLETON_INIT("scale-harmonization-v1");

const Framework *scale_harmonization_framework(void) {
    return framework_singleton_get(&g_singleton);
}
