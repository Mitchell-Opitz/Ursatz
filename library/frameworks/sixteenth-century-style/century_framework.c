#include "century_framework.h"

#include "theory/framework_singleton.h"

static FrameworkSingleton g_singleton = FRAMEWORK_SINGLETON_INIT("sixteenth-century-style-v1");

const Framework *sixteenth_century_framework(void) {
    return framework_singleton_get(&g_singleton);
}
