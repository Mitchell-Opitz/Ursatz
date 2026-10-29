#ifndef URSATZ_THEORY_FRAMEWORK_SINGLETON_H
#define URSATZ_THEORY_FRAMEWORK_SINGLETON_H

#include "core/once.h"
#include "theory/framework.h"

/*
 * The process-lifetime Framework a framework module returns from its accessor. Declare one static
 * FrameworkSingleton per framework, initialized with FRAMEWORK_SINGLETON_INIT("<framework>-v1"), and return
 * framework_singleton_get(&it) from the accessor. The first call (from any thread) creates the Framework with that
 * id as both its entity id and its name; every caller gets the same pointer. A failed creation stays NULL for good.
 * The Framework is never destroyed: it lives as long as the process.
 */
typedef struct {
    UrsatzOnce once;
    const char *id;
    Framework *framework;
} FrameworkSingleton;

#define FRAMEWORK_SINGLETON_INIT(framework_id) {URSATZ_ONCE_INIT, (framework_id), NULL}

const Framework *framework_singleton_get(FrameworkSingleton *singleton);

#endif
