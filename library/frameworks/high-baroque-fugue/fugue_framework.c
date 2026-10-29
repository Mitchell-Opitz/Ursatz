#include "fugue_framework.h"

#include "theory/framework_singleton.h"

static FrameworkSingleton g_singleton = FRAMEWORK_SINGLETON_INIT("high-baroque-fugue-v1");

const Framework *high_baroque_fugue_framework(void) {
    return framework_singleton_get(&g_singleton);
}
