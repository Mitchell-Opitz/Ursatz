#ifndef URSATZ_STANDARD_COUNTERPOINT_TESTS_EVAL_CHECK_H
#define URSATZ_STANDARD_COUNTERPOINT_TESTS_EVAL_CHECK_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/value.h"
#include "counterpoint_subject.h"
#include "rules/constraint.h"

/*
 * Runs a rule group through constraint_evaluate alongside its own
 * `*_rule_holds`. A test file declares the group once with
 * STANDARD_COUNTERPOINT_EVAL_TABLE and redefines the group's `*_rule_holds`
 * with the STANDARD_COUNTERPOINT_EVAL_HOLDS macro that fits its arguments, so
 * every existing call also evaluates the rule's Constraint on a candidate
 * holding the same subject and asserts the result agrees: PASS when the rule
 * holds, FAIL when it does not.
 *
 * For each rule it also asserts, once, that a candidate of another type tag is
 * NOT_APPLICABLE and that a candidate of the right tag but the wrong size is
 * rejected. When the test program exits, every rule of the group must have
 * been seen both passing and failing, so a rule with no passing or no failing
 * candidate fails the test.
 */

#define STANDARD_COUNTERPOINT_EVAL_MAX_RULES 32

enum {
    STANDARD_COUNTERPOINT_EVAL_SEEN_PASS = 1,
    STANDARD_COUNTERPOINT_EVAL_SEEN_FAIL = 2,
    STANDARD_COUNTERPOINT_EVAL_SEEN_OTHER_TAG = 8
};

typedef struct {
    const char *prefix;
    int count;
    unsigned seen[STANDARD_COUNTERPOINT_EVAL_MAX_RULES];
    bool registered;
} StandardCounterpointEvalState;

#define STANDARD_COUNTERPOINT_EVAL_TABLE(state, group_prefix, rule_count) \
    static StandardCounterpointEvalState state = {(group_prefix), (rule_count), {0}, false}

static StandardCounterpointEvalState *g_counterpoint_eval_states[8];
static int g_counterpoint_eval_state_count;

static void counterpoint_eval_report(void) {
    for (int s = 0; s < g_counterpoint_eval_state_count; s++) {
        const StandardCounterpointEvalState *state = g_counterpoint_eval_states[s];
        for (int i = 0; i < state->count; i++) {
            unsigned seen = state->seen[i];
            if ((seen & STANDARD_COUNTERPOINT_EVAL_SEEN_PASS) == 0 ||
                (seen & STANDARD_COUNTERPOINT_EVAL_SEEN_FAIL) == 0) {
                fprintf(stderr,
                        "standard-counterpoint/%s rule %d: constraint_evaluate saw %s, needs a passing and a "
                        "failing candidate\n",
                        state->prefix, i,
                        (seen & STANDARD_COUNTERPOINT_EVAL_SEEN_PASS)   ? "only PASS"
                        : (seen & STANDARD_COUNTERPOINT_EVAL_SEEN_FAIL) ? "only FAIL"
                                                                          : "neither");
                abort();
            }
        }
    }
}

/* Returns holds. */
static bool counterpoint_eval_holds(StandardCounterpointEvalState *state, const Constraint *constraint, int which,
                                    const void *subject, size_t size, bool holds) {
    if (constraint == NULL || subject == NULL || which < 0 || which >= state->count) {
        return holds;
    }
    if (!state->registered) {
        state->registered = true;
        assert(g_counterpoint_eval_state_count < 8);
        if (g_counterpoint_eval_state_count == 0) {
            atexit(counterpoint_eval_report);
        }
        g_counterpoint_eval_states[g_counterpoint_eval_state_count++] = state;
    }
    assert(constraint_evaluate_fn(constraint) != NULL);

    char tag[96];
    snprintf(tag, sizeof(tag), "standard-counterpoint/%s", state->prefix);
    Value *candidate;
    assert(value_create(tag, subject, size, &candidate) == VALUE_OK);
    ConstraintEvalResult result = constraint_evaluate(constraint, candidate, NULL);
    value_destroy(candidate);

    if (holds) {
        assert(result == CONSTRAINT_EVAL_PASS);
        state->seen[which] |= STANDARD_COUNTERPOINT_EVAL_SEEN_PASS;
    } else {
        assert(result == CONSTRAINT_EVAL_FAIL);
        state->seen[which] |= STANDARD_COUNTERPOINT_EVAL_SEEN_FAIL;
    }

    if ((state->seen[which] & STANDARD_COUNTERPOINT_EVAL_SEEN_OTHER_TAG) == 0) {
        state->seen[which] |= STANDARD_COUNTERPOINT_EVAL_SEEN_OTHER_TAG;
        Value *other;
        assert(value_create("not-this-group", subject, size, &other) == VALUE_OK);
        assert(constraint_evaluate(constraint, other, NULL) == CONSTRAINT_EVAL_NOT_APPLICABLE);
        value_destroy(other);
        Value *short_candidate;
        assert(value_create(tag, subject, size - 1, &short_candidate) == VALUE_OK);
        assert(constraint_evaluate(constraint, short_candidate, NULL) == CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);
        value_destroy(short_candidate);
    }
    return holds;
}

/* The rule takes its whole subject as one pointer. */
#define STANDARD_COUNTERPOINT_EVAL_HOLDS(state, constraint_fn, rule_holds_fn, subject_type, which, subject)      \
    counterpoint_eval_holds(&(state), constraint_fn((which)), (int)(which), (subject), sizeof(subject_type),   \
                            (rule_holds_fn)((which), (subject)))

/* The rule takes its subject as two arguments; subject_type is the struct they make up. */
#define STANDARD_COUNTERPOINT_EVAL_HOLDS2(state, constraint_fn, rule_holds_fn, subject_type, which, a, b)        \
    counterpoint_eval_holds(&(state), constraint_fn((which)), (int)(which), &(subject_type){(a), (b)},         \
                            sizeof(subject_type), (rule_holds_fn)((which), (a), (b)))

/* The rule takes its subject as three arguments. */
#define STANDARD_COUNTERPOINT_EVAL_HOLDS3(state, constraint_fn, rule_holds_fn, subject_type, which, a, b, c)     \
    counterpoint_eval_holds(&(state), constraint_fn((which)), (int)(which), &(subject_type){(a), (b), (c)},    \
                            sizeof(subject_type), (rule_holds_fn)((which), (a), (b), (c)))

#endif
