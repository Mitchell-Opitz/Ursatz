#ifndef MUSIC_RULES_RULE_EVALUATION_H
#define MUSIC_RULES_RULE_EVALUATION_H

#include "identity/entity_id.h"
#include "interpretation/interpretation.h"

/*
 * RuleEvaluation (Category / base type): the per-Rule evaluation
 * outcome a CandidateEvaluator (generation/candidate_evaluator.h, L8)
 * builds while judging a Candidate against one Rule/Constraint/
 * Preference -- the structured unit CandidateEvaluator folds many of
 * into its own aggregate evaluations[]/violations[]/score
 * (CandidateEvaluation). This is the shape the project owner confirmed
 * (docs/modules/rules.md's former Deferred entry) after review: no
 * distinct Registry row exists for it, so this is a considered fit
 * against Rule/Constraint/Preference's existing shapes and against
 * CandidateEvaluator's consumption pattern, not a literal TDD field
 * list.
 *
 * rule_id is a required EntityID reference to the Rule (rules/rule.h)
 * this evaluation is about -- referenced by id only, never embedded,
 * the same pattern Candidate's material_id uses for its own subject
 * (generation/candidate.h).
 *
 * outcome is RuleEvalOutcome, not a boolean: RULE_EVAL_SATISFIED/
 * _VIOLATED/_NOT_APPLICABLE/_NOT_IMPLEMENTED. NOT_APPLICABLE mirrors
 * constraint_evaluate's own CONSTRAINT_EVAL_NOT_APPLICABLE
 * (rules/constraint.h) for the same reason: a CONDITIONAL/CONTEXTUAL
 * Rule whose condition does not hold is neither satisfied nor
 * violated, and conflating it with VIOLATED would corrupt any score
 * built on top of it downstream. NOT_IMPLEMENTED lets a caller record
 * "no evaluation logic is wired up yet" as a real outcome value rather
 * than a sentinel/error, consistent with constraint_evaluate and
 * preference_rank both being stubs in this pass.
 *
 * finding is an optional, owned Interpretation* (interpretation/
 * interpretation.h) explaining the outcome -- kept explainable (TDD
 * §159), the same "never a bare pass/fail with no reasoning trail"
 * treatment CandidateEvaluation gives its own evaluations/violations.
 * NULL is valid: an outcome of NOT_APPLICABLE or NOT_IMPLEMENTED
 * typically carries no finding yet.
 *
 * weight_numerator/weight_denominator carry a WEIGHTED Constraint's
 * weight fraction forward (exact rational, Law 19, never float/double)
 * so a caller aggregating many RuleEvaluations into one score does not
 * need to re-resolve the originating Constraint. A denominator of 0 is
 * the explicit "no weight recorded" sentinel -- meaningful only when
 * the underlying Rule/Constraint actually carries a weight; ignored
 * otherwise, the same ignore-when-inapplicable treatment Constraint
 * itself gives these fields for its own non-WEIGHTED types.
 *
 * RuleEvaluation has no identity of its own (no id field, no
 * *_identity_equals) -- it is a derived judgment about a Rule, not an
 * independently identified Entity, the same treatment CandidateEvaluation
 * gets in generation/candidate_evaluator.h.
 *
 * This type only defines the shape and enforces its own construction
 * invariants. Populating a real RuleEvaluation from constraint_evaluate/
 * preference_rank output against an actual candidate is
 * CandidateEvaluator's job (music-generation, L8) -- out of scope here,
 * the same L7/L8 split constraint_evaluate and preference_rank already
 * follow.
 */

typedef enum {
    RULE_EVAL_SATISFIED = 0,
    RULE_EVAL_VIOLATED,
    RULE_EVAL_NOT_APPLICABLE,
    RULE_EVAL_NOT_IMPLEMENTED
} RuleEvalOutcome;

typedef struct RuleEvaluation RuleEvaluation;

typedef enum {
    RULE_EVALUATION_OK = 0,
    RULE_EVALUATION_ERR_NULL_RULE_ID,
    RULE_EVALUATION_ERR_WEIGHT_OUT_OF_RANGE,
    RULE_EVALUATION_ERR_ALLOC
} RuleEvaluationResult;

/*
 * Takes ownership of rule_id and, if non-NULL, finding on success --
 * both destroyed by rule_evaluation_destroy. rule_id is required.
 * finding is optional -- pass NULL if not supplied. weight_denominator
 * of 0 means "no weight recorded" (weight_numerator is then ignored);
 * a non-zero weight_denominator requires weight_numerator <=
 * weight_denominator.
 */
RuleEvaluationResult rule_evaluation_create(EntityID *rule_id, RuleEvalOutcome outcome, Interpretation *finding,
                                             unsigned long weight_numerator, unsigned long weight_denominator,
                                             RuleEvaluation **out);
void rule_evaluation_destroy(RuleEvaluation *evaluation);

const EntityID *rule_evaluation_rule_id(const RuleEvaluation *evaluation);
RuleEvalOutcome rule_evaluation_outcome(const RuleEvaluation *evaluation);
/* NULL when no finding was supplied. */
const Interpretation *rule_evaluation_finding(const RuleEvaluation *evaluation);
unsigned long rule_evaluation_weight_numerator(const RuleEvaluation *evaluation);
/* 0 means "no weight recorded". */
unsigned long rule_evaluation_weight_denominator(const RuleEvaluation *evaluation);

#endif