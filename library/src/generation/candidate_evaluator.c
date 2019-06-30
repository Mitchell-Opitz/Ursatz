#include "generation/candidate_evaluator.h"

#include <stdlib.h>

struct CandidateEvaluation {
    Interpretation **evaluations;
    size_t evaluation_count;
    Interpretation **violations;
    size_t violation_count;
    unsigned long score_numerator;
    unsigned long score_denominator;
    ProvenanceReference *provenance;
};

static Interpretation **copy_interpretation_pointers(Interpretation **interpretations, size_t count) {
    Interpretation **copy = (Interpretation **)malloc(sizeof(Interpretation *) * count);
    if (copy == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        copy[i] = interpretations[i];
    }
    return copy;
}

static void destroy_interpretations(Interpretation **interpretations, size_t count) {
    if (interpretations == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        interpretation_destroy(interpretations[i]);
    }
    free(interpretations);
}

CandidateEvaluationResult candidate_evaluation_create(Interpretation **evaluations, size_t evaluation_count,
                                                       Interpretation **violations, size_t violation_count,
                                                       unsigned long score_numerator, unsigned long score_denominator,
                                                       ProvenanceReference *provenance, CandidateEvaluation **out) {
    if (evaluation_count > 0) {
        if (evaluations == NULL) {
            return CANDIDATE_EVALUATION_ERR_NULL_EVALUATION;
        }
        for (size_t i = 0; i < evaluation_count; i++) {
            if (evaluations[i] == NULL) {
                return CANDIDATE_EVALUATION_ERR_NULL_EVALUATION;
            }
        }
    }
    if (violation_count > 0) {
        if (violations == NULL) {
            return CANDIDATE_EVALUATION_ERR_NULL_VIOLATION;
        }
        for (size_t i = 0; i < violation_count; i++) {
            if (violations[i] == NULL) {
                return CANDIDATE_EVALUATION_ERR_NULL_VIOLATION;
            }
        }
    }
    if (score_denominator == 0) {
        return CANDIDATE_EVALUATION_ERR_NON_POSITIVE_SCORE_DENOMINATOR;
    }
    if (provenance == NULL) {
        return CANDIDATE_EVALUATION_ERR_NULL_PROVENANCE;
    }

    CandidateEvaluation *evaluation = (CandidateEvaluation *)malloc(sizeof(CandidateEvaluation));
    if (evaluation == NULL) {
        return CANDIDATE_EVALUATION_ERR_ALLOC;
    }

    Interpretation **copied_evaluations = NULL;
    if (evaluation_count > 0) {
        copied_evaluations = copy_interpretation_pointers(evaluations, evaluation_count);
        if (copied_evaluations == NULL) {
            free(evaluation);
            return CANDIDATE_EVALUATION_ERR_ALLOC;
        }
    }

    Interpretation **copied_violations = NULL;
    if (violation_count > 0) {
        copied_violations = copy_interpretation_pointers(violations, violation_count);
        if (copied_violations == NULL) {
            free(copied_evaluations);
            free(evaluation);
            return CANDIDATE_EVALUATION_ERR_ALLOC;
        }
    }

    evaluation->evaluations = copied_evaluations;
    evaluation->evaluation_count = evaluation_count;
    evaluation->violations = copied_violations;
    evaluation->violation_count = violation_count;
    evaluation->score_numerator = score_numerator;
    evaluation->score_denominator = score_denominator;
    evaluation->provenance = provenance;

    *out = evaluation;
    return CANDIDATE_EVALUATION_OK;
}

void candidate_evaluation_destroy(CandidateEvaluation *evaluation) {
    if (evaluation == NULL) {
        return;
    }
    destroy_interpretations(evaluation->evaluations, evaluation->evaluation_count);
    destroy_interpretations(evaluation->violations, evaluation->violation_count);
    provenance_reference_destroy(evaluation->provenance);
    free(evaluation);
}

size_t candidate_evaluation_evaluation_count(const CandidateEvaluation *evaluation) {
    return evaluation->evaluation_count;
}

const Interpretation *candidate_evaluation_evaluation_at(const CandidateEvaluation *evaluation, size_t index) {
    return evaluation->evaluations[index];
}

size_t candidate_evaluation_violation_count(const CandidateEvaluation *evaluation) {
    return evaluation->violation_count;
}

const Interpretation *candidate_evaluation_violation_at(const CandidateEvaluation *evaluation, size_t index) {
    return evaluation->violations[index];
}

unsigned long candidate_evaluation_score_numerator(const CandidateEvaluation *evaluation) {
    return evaluation->score_numerator;
}

unsigned long candidate_evaluation_score_denominator(const CandidateEvaluation *evaluation) {
    return evaluation->score_denominator;
}

const ProvenanceReference *candidate_evaluation_provenance(const CandidateEvaluation *evaluation) {
    return evaluation->provenance;
}