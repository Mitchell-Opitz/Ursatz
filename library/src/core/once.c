#include "core/once.h"

#include <pthread.h>

/*
 * Helgrind does not model C11 atomics, so it reports the state word and the data `init` wrote as races. When its header
 * is installed the acquire/release pair is described to it; otherwise these expand to nothing.
 */
#if defined(__has_include)
#if __has_include(<valgrind/helgrind.h>)
#include <valgrind/helgrind.h>
#define ONCE_RELEASE(once)                                                                                             \
    do {                                                                                                               \
        ANNOTATE_BENIGN_RACE_SIZED(&(once)->state, sizeof((once)->state), "UrsatzOnce state is atomic");               \
        ANNOTATE_HAPPENS_BEFORE(once);                                                                                 \
    } while (0)
#define ONCE_ACQUIRE(once) ANNOTATE_HAPPENS_AFTER(once)
#endif
#endif
#ifndef ONCE_RELEASE
#define ONCE_RELEASE(once) ((void)0)
#define ONCE_ACQUIRE(once) ((void)0)
#endif

enum { ONCE_NEW = 0, ONCE_RUNNING = 1, ONCE_DONE = 2 };

static pthread_once_t g_mutex_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_mutex;

static void create_mutex(void) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

void ursatz_once(UrsatzOnce *once, void (*init)(void *arg), void *arg) {
    if (once == NULL || init == NULL) {
        return;
    }
    if (atomic_load_explicit(&once->state, memory_order_acquire) == ONCE_DONE) {
        ONCE_ACQUIRE(once);
        return;
    }
    pthread_once(&g_mutex_once, create_mutex);
    pthread_mutex_lock(&g_mutex);
    if (atomic_load_explicit(&once->state, memory_order_relaxed) == ONCE_NEW) {
        ONCE_RELEASE(once);
        atomic_store_explicit(&once->state, ONCE_RUNNING, memory_order_relaxed);
        init(arg);
        ONCE_RELEASE(once);
        atomic_store_explicit(&once->state, ONCE_DONE, memory_order_release);
    }
    pthread_mutex_unlock(&g_mutex);
}

bool ursatz_once_is_done(const UrsatzOnce *once) {
    return once != NULL && atomic_load_explicit(&((UrsatzOnce *)once)->state, memory_order_acquire) == ONCE_DONE;
}
