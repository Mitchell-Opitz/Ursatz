#ifndef URSATZ_CORE_ONCE_H
#define URSATZ_CORE_ONCE_H

#include <stdatomic.h>

/*
 * One-time initialization that is safe when several threads make their first call at the same time. Declare a
 * static UrsatzOnce initialized with URSATZ_ONCE_INIT and call ursatz_once() before reading what `init` builds.
 *
 * `init` runs exactly once per UrsatzOnce; every caller returns only after it has finished, and sees everything it
 * wrote. The fast path is one acquire load. The slow path takes one process-wide recursive mutex, so an `init` that
 * itself triggers another ursatz_once() (a rule table asking for its framework) does not deadlock. A call made from
 * inside its own `init` returns at once without running it again. `init` has no result: record a failure in the
 * state it builds (a NULL pointer stays NULL for good).
 */
typedef struct {
    atomic_int state;
} UrsatzOnce;

#define URSATZ_ONCE_INIT {0}

void ursatz_once(UrsatzOnce *once, void (*init)(void *arg), void *arg);

#endif
