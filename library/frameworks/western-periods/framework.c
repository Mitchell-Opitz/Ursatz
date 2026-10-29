#include "framework.h"

#include "theory/framework_singleton.h"

static FrameworkSingleton g_singleton = FRAMEWORK_SINGLETON_INIT("western-periods-v1");

const Framework *western_periods_framework(void) {
    return framework_singleton_get(&g_singleton);
}
