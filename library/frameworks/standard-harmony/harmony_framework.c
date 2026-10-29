#include "harmony_framework.h"

#include "theory/framework_singleton.h"

static FrameworkSingleton g_singleton = FRAMEWORK_SINGLETON_INIT("standard-harmony-v1");

const Framework *standard_harmony_framework(void) {
    return framework_singleton_get(&g_singleton);
}
