#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "theory/framework.h"

/*
 * Eight threads make the very first call of every framework accessor and one rule accessor per framework at the same
 * moment, and must all get the same pointers. Run as its own process so nothing has been initialized yet. The framework
 * headers share file names (framework.h, rule_pair.h), so the accessors are declared here; enum arguments are passed as
 * int, which is how they are called.
 */

#define THREAD_COUNT 8

#define FRAMEWORKS(X)                                                                                                  \
    X(baroque_ground_bass_framework)                                                                                   \
    X(common_practice_minimal_framework)                                                                               \
    X(galant_schemas_framework)                                                                                        \
    X(high_baroque_fugue_framework)                                                                                    \
    X(scale_harmonization_framework)                                                                                   \
    X(sixteenth_century_framework)                                                                                     \
    X(standard_counterpoint_framework)                                                                                 \
    X(standard_form_framework)                                                                                         \
    X(standard_harmony_framework)                                                                                      \
    X(standard_markings_framework)                                                                                     \
    X(standard_meter_framework)                                                                                        \
    X(standard_notation_framework)                                                                                     \
    X(standard_rhythm_framework)                                                                                       \
    X(standard_texture_framework)                                                                                      \
    X(standard_tonality_framework)                                                                                     \
    X(standard_voicing_framework)                                                                                      \
    X(western_periods_framework)

#define RULES(X)                                                                                                       \
    X(ground_bass_rule)                                                                                                \
    X(common_practice_triad_rule)                                                                                      \
    X(galant_matching_rule)                                                                                            \
    X(fugue_answer_rule)                                                                                               \
    X(scale_octave_rule)                                                                                               \
    X(sixteenth_century_dissonance_rule)                                                                               \
    X(standard_counterpoint_cantus_rule)                                                                               \
    X(standard_form_beginning_rule)                                                                                    \
    X(harmony_applied_rule)                                                                                            \
    X(standard_markings_articulation_rule)                                                                             \
    X(standard_meter_anacrusis_rule)                                                                                   \
    X(standard_notation_accidental_placement_rule)                                                                     \
    X(standard_rhythm_dot_rule)                                                                                        \
    X(standard_texture_mixed_rule)                                                                                     \
    X(standard_tonality_chord_symbol_rule)                                                                             \
    X(standard_voicing_doubling_rule)                                                                                  \
    X(western_periods_rule)

#define DECLARE_FRAMEWORK(name) const Framework *name(void);
#define DECLARE_RULE(name) const Rule *name(int which);
FRAMEWORKS(DECLARE_FRAMEWORK)
RULES(DECLARE_RULE)

#define COUNT_ONE(name) +1
enum { FRAMEWORK_COUNT = 0 FRAMEWORKS(COUNT_ONE), RULE_COUNT = 0 RULES(COUNT_ONE) };

typedef struct {
    const void *frameworks[FRAMEWORK_COUNT];
    const void *rules[RULE_COUNT];
} Seen;

static pthread_barrier_t g_start;
static Seen g_seen[THREAD_COUNT];

static void *worker(void *arg) {
    Seen *seen = arg;
    pthread_barrier_wait(&g_start);
    size_t i = 0;
#define CALL_FRAMEWORK(name) seen->frameworks[i++] = name();
    FRAMEWORKS(CALL_FRAMEWORK)
    i = 0;
#define CALL_RULE(name) seen->rules[i++] = name(0);
    RULES(CALL_RULE)
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
    for (int i = 0; i < RULE_COUNT; i++) {
        assert(g_seen[0].rules[i] != NULL);
        for (int t = 1; t < THREAD_COUNT; t++) {
            assert(g_seen[t].rules[i] == g_seen[0].rules[i]);
        }
    }
    printf("concurrent first use: %d frameworks, %d rule accessors, %d threads\n", FRAMEWORK_COUNT, RULE_COUNT,
           THREAD_COUNT);
    return 0;
}
