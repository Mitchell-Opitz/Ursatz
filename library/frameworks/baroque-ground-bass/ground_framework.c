#include "ground_framework.h"

#include "theory/framework_singleton.h"

static FrameworkSingleton g_singleton = FRAMEWORK_SINGLETON_INIT("baroque-ground-bass-v1");

const Framework *baroque_ground_bass_framework(void) {
    return framework_singleton_get(&g_singleton);
}
