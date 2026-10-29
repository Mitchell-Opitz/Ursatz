#include "core/once.h"

#include <pthread.h>

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
        return;
    }
    pthread_once(&g_mutex_once, create_mutex);
    pthread_mutex_lock(&g_mutex);
    if (atomic_load_explicit(&once->state, memory_order_relaxed) == ONCE_NEW) {
        atomic_store_explicit(&once->state, ONCE_RUNNING, memory_order_relaxed);
        init(arg);
        atomic_store_explicit(&once->state, ONCE_DONE, memory_order_release);
    }
    pthread_mutex_unlock(&g_mutex);
}
