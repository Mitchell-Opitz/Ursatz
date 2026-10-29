#include "framework.h"

#include "theory/framework_singleton.h"

static FrameworkSingleton g_singleton = FRAMEWORK_SINGLETON_INIT("common-practice-minimal-v1");

const Framework *common_practice_minimal_framework(void) {
    return framework_singleton_get(&g_singleton);
}
