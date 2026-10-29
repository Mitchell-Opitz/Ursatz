#include "framework.h"

#include "theory/framework_singleton.h"

static FrameworkSingleton g_singleton = FRAMEWORK_SINGLETON_INIT("standard-notation-v1");

const Framework *standard_notation_framework(void) {
    return framework_singleton_get(&g_singleton);
}
