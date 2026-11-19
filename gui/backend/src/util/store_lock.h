#ifndef URSATZ_GUI_STORE_LOCK_H
#define URSATZ_GUI_STORE_LOCK_H

/* One exclusive lock per store: a pthread mutex. */
#include <pthread.h>
typedef pthread_mutex_t UrsatzGuiLock;
#define URSATZ_GUI_LOCK_INIT PTHREAD_MUTEX_INITIALIZER
static inline void ursatz_gui_lock_acquire(UrsatzGuiLock *lock) {
    pthread_mutex_lock(lock);
}
static inline void ursatz_gui_lock_release(UrsatzGuiLock *lock) {
    pthread_mutex_unlock(lock);
}

#endif
