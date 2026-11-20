#ifndef WORKSHOP_STORE_LOCK_H
#define WORKSHOP_STORE_LOCK_H

/* One exclusive lock per store: a pthread mutex. */
#include <pthread.h>
typedef pthread_mutex_t WorkshopLock;
#define WORKSHOP_LOCK_INIT PTHREAD_MUTEX_INITIALIZER
static inline void workshop_lock_acquire(WorkshopLock *lock) {
    pthread_mutex_lock(lock);
}
static inline void workshop_lock_release(WorkshopLock *lock) {
    pthread_mutex_unlock(lock);
}

#endif
