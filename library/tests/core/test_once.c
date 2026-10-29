#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>

#include "core/once.h"

#define THREAD_COUNT 16

static UrsatzOnce g_race = URSATZ_ONCE_INIT;
static atomic_int g_race_runs;
static int g_race_value;

static void race_init(void *arg) {
    (void)arg;
    atomic_fetch_add(&g_race_runs, 1);
    /* a slow init widens the window in which other threads arrive while it runs */
    for (volatile int i = 0; i < 2000000; i++) {
    }
    g_race_value = 42;
}

static void *race_thread(void *arg) {
    (void)arg;
    ursatz_once(&g_race, race_init, NULL);
    /* every caller sees what init wrote */
    assert(g_race_value == 42);
    return NULL;
}

static void test_threads_racing_one_init_run_it_once(void) {
    pthread_t threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        assert(pthread_create(&threads[i], NULL, race_thread, NULL) == 0);
    }
    for (int i = 0; i < THREAD_COUNT; i++) {
        assert(pthread_join(threads[i], NULL) == 0);
    }
    assert(atomic_load(&g_race_runs) == 1);
    assert(ursatz_once_is_done(&g_race));
}

static UrsatzOnce g_outer = URSATZ_ONCE_INIT;
static UrsatzOnce g_inner = URSATZ_ONCE_INIT;
static int g_outer_runs;
static int g_inner_runs;

static void inner_init(void *arg) {
    (void)arg;
    g_inner_runs++;
}

static void outer_init(void *arg) {
    (void)arg;
    g_outer_runs++;
    ursatz_once(&g_inner, inner_init, NULL);
    /* a call from inside its own init returns without running it again */
    ursatz_once(&g_outer, outer_init, NULL);
}

static void test_nested_init_does_not_deadlock(void) {
    ursatz_once(&g_outer, outer_init, NULL);
    assert(g_outer_runs == 1);
    assert(g_inner_runs == 1);
    ursatz_once(&g_outer, outer_init, NULL);
    ursatz_once(&g_inner, inner_init, NULL);
    assert(g_outer_runs == 1);
    assert(g_inner_runs == 1);
}

static UrsatzOnce g_arg = URSATZ_ONCE_INIT;

static void arg_init(void *arg) {
    *(int *)arg = 7;
}

static void test_arg_is_passed(void) {
    int value = 0;
    assert(!ursatz_once_is_done(&g_arg));
    ursatz_once(&g_arg, arg_init, &value);
    assert(value == 7);
    assert(ursatz_once_is_done(&g_arg));
    value = 0;
    ursatz_once(&g_arg, arg_init, &value);
    assert(value == 0);
}

static void test_null_arguments_are_ignored(void) {
    UrsatzOnce once = URSATZ_ONCE_INIT;
    ursatz_once(NULL, arg_init, NULL);
    ursatz_once(&once, NULL, NULL);
    assert(!ursatz_once_is_done(&once));
    assert(!ursatz_once_is_done(NULL));
}

int main(void) {
    test_threads_racing_one_init_run_it_once();
    test_nested_init_does_not_deadlock();
    test_arg_is_passed();
    test_null_arguments_are_ignored();
    return 0;
}
