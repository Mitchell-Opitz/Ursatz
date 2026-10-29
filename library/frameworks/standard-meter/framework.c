#include "framework.h"

#include "theory/framework_singleton.h"

static FrameworkSingleton g_singleton = FRAMEWORK_SINGLETON_INIT("standard-meter-v1");

const Framework *standard_meter_framework(void) {
    return framework_singleton_get(&g_singleton);
}
