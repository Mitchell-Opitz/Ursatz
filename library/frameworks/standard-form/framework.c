#include "framework.h"

#include "theory/framework_singleton.h"

static FrameworkSingleton g_singleton = FRAMEWORK_SINGLETON_INIT("standard-form-v1");

const Framework *standard_form_framework(void) {
    return framework_singleton_get(&g_singleton);
}
