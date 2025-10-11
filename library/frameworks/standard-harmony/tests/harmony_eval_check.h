#ifndef URSATZ_STANDARD_HARMONY_TESTS_HARMONY_EVAL_CHECK_H
#define URSATZ_STANDARD_HARMONY_TESTS_HARMONY_EVAL_CHECK_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/value.h"
#include "rules/constraint.h"

/*
 * Runs a rule group through constraint_evaluate alongside its own
 * `*_rule_holds`. A test file declares the group once with
 * HARMONY_EVAL_TABLE and redefines the group's `*_rule_holds` with
 * HARMONY_EVAL_HOLDS (or _APPLIES), so every existing call also evaluates the
 * rule's Constraint on a candidate holding the same subject and asserts the
 * result agrees: PASS when the rule holds, FAIL when it does not (and
 * NOT_APPLICABLE when the rule has nothing to say about the subject).
 *
 * For each rule it also asserts, once, that a candidate of another type tag is
 * NOT_APPLICABLE and that a candidate of the right tag but the wrong size is
 * rejected. When the test program exits, every rule of the group must have
 * been seen both passing and failing, so a rule with no passing or no failing
 * candidate fails the test.
 */

#define HARMONY_EVAL_MAX_RULES 32

enum {
    HARMONY_EVAL_SEEN_PASS = 1,
    HARMONY_EVAL_SEEN_FAIL = 2,
    HARMONY_EVAL_SEEN_NOT_APPLICABLE = 4,
    HARMONY_EVAL_SEEN_OTHER_TAG = 8
};

typedef struct {
    const char *prefix;
    int count;
    unsigned seen[HARMONY_EVAL_MAX_RULES];
    bool registered;
} HarmonyEvalState;

#define HARMONY_EVAL_TABLE(state, group_prefix, rule_count) \
    static HarmonyEvalState state = {(group_prefix), (rule_count), {0}, false}

static HarmonyEvalState *g_harmony_eval_states[8];
static int g_harmony_eval_state_count;

static void harmony_eval_report(void) {
    for (int s = 0; s < g_harmony_eval_state_count; s++) {
        const HarmonyEvalState *state = g_harmony_eval_states[s];
        for (int i = 0; i < state->count; i++) {
            if ((state->seen[i] & HARMONY_EVAL_SEEN_PASS) == 0 || (state->seen[i] & HARMONY_EVAL_SEEN_FAIL) == 0) {
                fprintf(stderr, "standard-harmony/%s rule %d: constraint_evaluate saw %s, needs a passing and a failing candidate\n",
                        state->prefix, i,
                        (state->seen[i] & HARMONY_EVAL_SEEN_PASS) ? "only PASS" : (state->seen[i] & HARMONY_EVAL_SEEN_FAIL) ? "only FAIL" : "neither");
                abort();
            }
        }
    }
}

/* applies is -1 when the rule always applies, else 0 or 1. Returns holds. */
static bool harmony_eval_holds(HarmonyEvalState *state, const Constraint *constraint, int which, const void *subject,
                               size_t size, bool holds, int applies) {
    if (constraint == NULL || subject == NULL || which < 0 || which >= state->count) {
        return holds;
    }
    if (!state->registered) {
        state->registered = true;
        assert(g_harmony_eval_state_count < 8);
        if (g_harmony_eval_state_count == 0) {
            atexit(harmony_eval_report);
        }
        g_harmony_eval_states[g_harmony_eval_state_count++] = state;
    }
    assert(constraint_evaluate_fn(constraint) != NULL);

    char tag[96];
    snprintf(tag, sizeof(tag), "standard-harmony/%s", state->prefix);
    Value *candidate;
    assert(value_create(tag, subject, size, &candidate) == VALUE_OK);
    ConstraintEvalResult result = constraint_evaluate(constraint, candidate, NULL);
    value_destroy(candidate);

    if (applies == 0) {
        assert(result == CONSTRAINT_EVAL_NOT_APPLICABLE);
        state->seen[which] |= HARMONY_EVAL_SEEN_NOT_APPLICABLE;
    } else if (holds) {
        assert(result == CONSTRAINT_EVAL_PASS);
        state->seen[which] |= HARMONY_EVAL_SEEN_PASS;
    } else {
        assert(result == CONSTRAINT_EVAL_FAIL);
        state->seen[which] |= HARMONY_EVAL_SEEN_FAIL;
    }

    if ((state->seen[which] & HARMONY_EVAL_SEEN_OTHER_TAG) == 0) {
        state->seen[which] |= HARMONY_EVAL_SEEN_OTHER_TAG;
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

#define HARMONY_EVAL_HOLDS(state, constraint_fn, rule_holds_fn, subject_type, which, subject)                 \
    harmony_eval_holds(&(state), constraint_fn((which)), (int)(which), (subject), sizeof(subject_type),     \
                       (rule_holds_fn)((which), (subject)), -1)

#define HARMONY_EVAL_HOLDS_APPLIES(state, constraint_fn, rule_holds_fn, rule_applies_fn, subject_type, which, \
                                   subject)                                                                    \
    harmony_eval_holds(&(state), constraint_fn((which)), (int)(which), (subject), sizeof(subject_type),     \
                       (rule_holds_fn)((which), (subject)), (rule_applies_fn)((which), (subject)) ? 1 : 0)

#endif
