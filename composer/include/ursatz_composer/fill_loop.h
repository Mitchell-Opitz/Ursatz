#ifndef URSATZ_COMPOSER_FILL_LOOP_H
#define URSATZ_COMPOSER_FILL_LOOP_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/fill_target.h>
#include <ursatz_composer/piece_plan.h>

/*
 * The fill loop. One loop serves every direction: where the Given material sits
 * and which operators are registered decide whether a plan fills top-down,
 * bottom-up or from the middle.
 *
 * Each iteration:
 *   1. Pick the open target with the most constraints (fill_pick_open_target).
 *   2. Ask every applicable operator, in registration order, for candidates.
 *   3. Check each candidate. A hard-rule break rejects it; a score ranks the rest.
 *   4. Choose the best-scoring candidate; equal scores are settled by the run's
 *      seeded random stream.
 *   5. Apply it, mark the target Realized, and propagate.
 * It repeats until nothing is open.
 *
 * A target nothing can fill is set aside, not retried until a later step
 * succeeds (a success can add constraints that make an operator applicable),
 * so the loop always ends: when nothing open is left, when only unfillable
 * targets are, or at the iteration limit.
 */

/* --- the checker -------------------------------------------------------- */

#define FILL_REASON_MAX 64

/*
 * The checker's verdict on one candidate. The score is a non-negative rational,
 * as Ursatz's Candidate score is, and higher is better. A hard violation
 * rejects the candidate; reason is short text for the record.
 */
typedef struct {
    bool hard_violation;
    unsigned long score_numerator;
    unsigned long score_denominator; /* must be at least 1 */
    char reason[FILL_REASON_MAX];
    /*
     * The part of the score a style added (style_check.c), kept apart so a record can show it. A denominator of 0, as
     * a checker that knows no style leaves it, means no style; the loop never reads these two.
     */
    unsigned long style_numerator;
    unsigned long style_denominator;
} FillCheck;

/*
 * Checks one candidate for target against the plan. Real rule checking plugs in
 * here. Returns false only on failure (not for a rejected candidate).
 */
typedef bool (*FillCheckerFn)(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                              const FillCandidate *candidate, void *context, FillCheck *out);

/* Accepts everything at score 0/1. This is the checker when none is given. */
bool fill_default_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                          const FillCandidate *candidate, void *context, FillCheck *out);

/* --- running ------------------------------------------------------------ */

#define FILL_DEFAULT_MAX_ITERATIONS 10000

/* All-zero is the default configuration (seed 0). */
typedef struct {
    unsigned long seed;     /* seeds the run's one random stream */
    size_t max_iterations;  /* attempts, including failed ones; 0 means FILL_DEFAULT_MAX_ITERATIONS */
    FillCheckerFn checker;  /* NULL means fill_default_checker */
    void *checker_context;
} FillConfig;

typedef enum {
    FILL_STATUS_COMPLETE,       /* nothing is open */
    FILL_STATUS_STUCK,          /* open targets remain and none can be filled */
    FILL_STATUS_MAX_ITERATIONS, /* the iteration limit was reached */
    FILL_STATUS_ERROR           /* fill_run returned an error */
} FillStatus;

typedef enum {
    FILL_STUCK_NO_OPERATOR,  /* no registered operator applies to the target */
    FILL_STUCK_ALL_REJECTED  /* operators applied, but every candidate was rejected (or none was proposed) */
} FillStuckReason;

typedef struct {
    FillTarget target;
    FillStuckReason reason;
} FillStuck;

typedef struct {
    FillStatus status;
    size_t iterations; /* attempts made */
    size_t steps;      /* attempts that changed the plan */
    FillStuck *stuck;  /* owned; the targets still unfilled when the run ended */
    size_t stuck_count;
} FillOutcome;

void fill_outcome_destroy(FillOutcome *outcome);

/* The record of a run; see fill_record.h. */
typedef struct FillRecord FillRecord;

/*
 * Runs the loop on plan. registry and outcome are required. On FILL_OK the
 * outcome says how it ended. On an error (an operator's generate or apply
 * failed, the checker failed, allocation failed) the plan may have been changed
 * by the steps before it, and outcome->status is FILL_STATUS_ERROR.
 *
 * Every choice is made from config->seed: the same plan, operators, checker and
 * seed give the same plan and the same record. When record is not NULL, *record
 * is set to the run's record (also after an error, if one was started), which
 * the caller destroys with fill_record_destroy.
 */
FillResult fill_run(PiecePlan *plan, const FillOperatorRegistry *registry, const FillConfig *config,
                    FillOutcome *outcome, FillRecord **record);

/* --- propagation helper ------------------------------------------------- */

/*
 * The target's relatives, for operators that propagate constraints:
 *   node       its parent, its children, and the sibling before and after it
 *   lane span  its owner node, and the spans before and after it in the same
 *              lane and voice
 * Writes up to capacity targets to out (which may be NULL when capacity is 0)
 * and returns how many there are in all.
 */
size_t fill_relatives(const PiecePlan *plan, FillTarget target, FillTarget *out, size_t capacity);

#endif
