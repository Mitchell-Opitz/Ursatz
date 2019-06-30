#ifndef MUSIC_GENERATION_CANDIDATE_EVALUATOR_H
#define MUSIC_GENERATION_CANDIDATE_EVALUATOR_H

#include <stddef.h>

#include "core/provenance_reference.h"
#include "generation/candidate.h"
#include "interpretation/interpretation.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"
#include "theory/statistical_model.h"

/*
 * CandidateEvaluation (Category / base type): the output of a
 * CandidateEvaluator (Component, Registry #28) -- evaluations,
 * violations, and score for one Candidate (generation/candidate.h),
 * kept explainable (TDD Sec159): findings are never collapsed to a
 * bare pass/fail score with no reasoning trail.
 *
 * A CandidateEvaluator never writes a bare conclusion -- it derives new
 * Interpretation objects (interpretation/interpretation.h, L6),
 * exactly as an Analyzer derives Interpretations into AnalysisResult
 * (analysis/analyzer.h, Registry #12). CandidateEvaluation cannot reuse
 * AnalysisResult itself: music-generation (L8) MUST NOT depend on its
 * L8 sibling music-analysis (Dependency & Layer Spec v2 Sec2, Sec3,
 * enforced by tests/generation/test_architecture_generation.c) -- this
 * is the same shape, defined again here rather than imported.
 *
 * evaluations and violations are independent, owned arrays-of-
 * Interpretation-pointers-plus-count, matching Candidate's own
 * evaluations/violations fields exactly (generation/candidate.h) --
 * the intended shape a caller merges into a new, evaluated Candidate
 * for the same material_id. Both may be empty (count == 0, pass NULL
 * for the corresponding array): an evaluation that raised no findings
 * either way is still a valid, empty CandidateEvaluation, the same
 * "found nothing is not an error" treatment AnalysisResult gives its
 * own arrays.
 *
 * score is an exact rational fraction (score_numerator/
 * score_denominator, denominator > 0, Law 19 -- never float/double),
 * the same aggregate-summary-alongside-the-trail treatment Candidate's
 * own score field gets: a magnitude, never a substitute for
 * evaluations/violations.
 *
 * provenance is a required ProvenanceReference: what produced this
 * evaluation (typically the CandidateEvaluator invocation itself).
 */

typedef struct CandidateEvaluation CandidateEvaluation;

typedef enum {
    CANDIDATE_EVALUATION_OK = 0,
    CANDIDATE_EVALUATION_ERR_NULL_EVALUATION,
    CANDIDATE_EVALUATION_ERR_NULL_VIOLATION,
    CANDIDATE_EVALUATION_ERR_NON_POSITIVE_SCORE_DENOMINATOR,
    CANDIDATE_EVALUATION_ERR_NULL_PROVENANCE,
    CANDIDATE_EVALUATION_ERR_ALLOC
} CandidateEvaluationResult;

/*
 * Takes ownership of each Interpretation in evaluations (if any), each
 * Interpretation in violations (if any), and provenance on success (all
 * destroyed by candidate_evaluation_destroy). evaluations/violations
 * are optional -- pass NULL with the matching count == 0 for either not
 * supplied. No entry of a non-empty array may be NULL.
 * score_denominator must be > 0.
 */
CandidateEvaluationResult candidate_evaluation_create(Interpretation **evaluations, size_t evaluation_count,
                                                       Interpretation **violations, size_t violation_count,
                                                       unsigned long score_numerator, unsigned long score_denominator,
                                                       ProvenanceReference *provenance, CandidateEvaluation **out);
void candidate_evaluation_destroy(CandidateEvaluation *evaluation);

size_t candidate_evaluation_evaluation_count(const CandidateEvaluation *evaluation);
const Interpretation *candidate_evaluation_evaluation_at(const CandidateEvaluation *evaluation, size_t index);
size_t candidate_evaluation_violation_count(const CandidateEvaluation *evaluation);
const Interpretation *candidate_evaluation_violation_at(const CandidateEvaluation *evaluation, size_t index);
unsigned long candidate_evaluation_score_numerator(const CandidateEvaluation *evaluation);
unsigned long candidate_evaluation_score_denominator(const CandidateEvaluation *evaluation);
const ProvenanceReference *candidate_evaluation_provenance(const CandidateEvaluation *evaluation);

/*
 * CandidateEvaluator (Interface / Component, Registry #28): evaluate(
 * candidate, rules, constraints, preferences, statistical_models) ->
 * CandidateEvaluation. Like Analyzer (analysis/analyzer.h) and
 * Generator (generation/generator.h), CandidateEvaluator is a
 * function-pointer interface, not a data type with its own create/
 * destroy: a concrete evaluator is a module that implements
 * CandidateEvaluatorFn and exposes it, alongside a name, as a
 * CandidateEvaluator value -- it does not subclass or extend this
 * struct.
 *
 * candidate is a borrowed, required Candidate (generation/candidate.h)
 * -- the proposal being evaluated. rules/constraints/preferences/
 * statistical_models are borrowed, optional arrays-of-borrowed-
 * pointers-plus-count (all four counts may be 0; a NULL array with
 * count 0 means "none of this kind supplied") -- the L7 vocabulary
 * (rules/rule.h, rules/constraint.h, rules/preference.h,
 * theory/statistical_model.h) this evaluation is judged against.
 * CandidateEvaluator does not own any of them: they are expected to
 * outlive the call and be reused across many evaluate() invocations,
 * the same reference-only treatment Rule/Constraint/Preference give
 * their own framework field.
 *
 * On CANDIDATE_EVALUATOR_OK, *out is a newly-created CandidateEvaluation
 * the caller owns (destroy with candidate_evaluation_destroy); on any
 * other result, *out is left untouched. A concrete evaluator MUST NOT
 * mutate candidate or any of the referenced Rule/Constraint/Preference/
 * StatisticalModel objects (TDD Sec140 referential transparency, the
 * same rule Transformation and Analyzer follow).
 */

typedef enum {
    CANDIDATE_EVALUATOR_OK = 0,
    CANDIDATE_EVALUATOR_ERR_NULL_CANDIDATE,
    CANDIDATE_EVALUATOR_ERR_NULL_OUT,
    CANDIDATE_EVALUATOR_ERR_ALLOC,
    CANDIDATE_EVALUATOR_ERR_EVALUATION_FAILED
} CandidateEvaluatorResult;

typedef CandidateEvaluatorResult (*CandidateEvaluatorFn)(const Candidate *candidate, const Rule **rules,
                                                          size_t rule_count, const Constraint **constraints,
                                                          size_t constraint_count, const Preference **preferences,
                                                          size_t preference_count,
                                                          const StatisticalModel **statistical_models,
                                                          size_t statistical_model_count, CandidateEvaluation **out);

typedef struct {
    const char *name;
    CandidateEvaluatorFn evaluate;
} CandidateEvaluator;

#endif