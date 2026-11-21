#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include "theory/framework/framework.h"

/*
 * Eight threads make the very first call of every framework accessor an app calls at the
 * same moment, and must all get the same pointers. Run as its own process so nothing has been initialized yet. The framework
 * headers share file names (framework.h), so the accessors are declared here; enum arguments are passed as
 * int, which is how they are called.
 */

#define THREAD_COUNT 8

#define FRAMEWORKS(X)                                                                                                  \
    X(common_practice_minimal_framework)                                                                               \
    X(galant_schemas_framework)                                                                                        \
    X(scale_harmonization_framework)                                                                                   \
    X(standard_counterpoint_framework)                                                                                 \
    X(standard_form_framework)                                                                                         \
    X(standard_harmony_framework)                                                                                      \
    X(standard_markings_framework)                                                                                     \
    X(standard_meter_framework)                                                                                        \
    X(standard_texture_framework)                                                                                      \
    X(standard_tonality_framework)                                                                                     \
    X(standard_voicing_framework)

#define DECLARE_FRAMEWORK(name) const Framework *name(void);
FRAMEWORKS(DECLARE_FRAMEWORK)

#define COUNT_ONE(name) +1
enum { FRAMEWORK_COUNT = 0 FRAMEWORKS(COUNT_ONE) };

typedef struct {
    const void *frameworks[FRAMEWORK_COUNT];
} Seen;

static pthread_barrier_t g_start;
static Seen g_seen[THREAD_COUNT];

static void *worker(void *arg) {
    Seen *seen = arg;
    pthread_barrier_wait(&g_start);
    size_t i = 0;
#define CALL_FRAMEWORK(name) seen->frameworks[i++] = name();
    FRAMEWORKS(CALL_FRAMEWORK)
    return NULL;
}

int main(void) {
    pthread_t threads[THREAD_COUNT];
    assert(pthread_barrier_init(&g_start, NULL, THREAD_COUNT) == 0);
    for (int t = 0; t < THREAD_COUNT; t++) {
        assert(pthread_create(&threads[t], NULL, worker, &g_seen[t]) == 0);
    }
    for (int t = 0; t < THREAD_COUNT; t++) {
        assert(pthread_join(threads[t], NULL) == 0);
    }
    pthread_barrier_destroy(&g_start);

    for (int i = 0; i < FRAMEWORK_COUNT; i++) {
        assert(g_seen[0].frameworks[i] != NULL);
        for (int t = 1; t < THREAD_COUNT; t++) {
            assert(g_seen[t].frameworks[i] == g_seen[0].frameworks[i]);
        }
    }
    printf("concurrent first use: %d frameworks, %d threads\n", FRAMEWORK_COUNT, THREAD_COUNT);
    return 0;
}
