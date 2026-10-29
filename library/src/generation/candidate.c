#include "generation/candidate.h"

#include <stdlib.h>
#include <string.h>

#include "core/string_copy.h"

struct Candidate {
    EntityID *id;
    EntityID *material_id;
    Interpretation **evaluations;
    size_t evaluation_count;
    Interpretation **violations;
    size_t violation_count;
    unsigned long score_numerator;
    unsigned long score_denominator;
    ProvenanceReference *provenance;
};

struct CandidateSet {
    EntityID *id;
    Candidate **candidates;
    size_t candidate_count;
    unsigned long random_seed;
    char *generator_version;
    Value *configuration;
    EntityID **input_ids;
    size_t input_id_count;
    char *framework_version;
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

CandidateResult candidate_create(EntityID *id, EntityID *material_id, Interpretation **evaluations,
                                  size_t evaluation_count, Interpretation **violations, size_t violation_count,
                                  unsigned long score_numerator, unsigned long score_denominator,
                                  ProvenanceReference *provenance, Candidate **out) {
    if (id == NULL) {
        return CANDIDATE_ERR_NULL_ID;
    }
    if (material_id == NULL) {
        return CANDIDATE_ERR_NULL_MATERIAL_ID;
    }
    if (evaluation_count > 0) {
        if (evaluations == NULL) {
            return CANDIDATE_ERR_NULL_EVALUATION;
        }
        for (size_t i = 0; i < evaluation_count; i++) {
            if (evaluations[i] == NULL) {
                return CANDIDATE_ERR_NULL_EVALUATION;
            }
        }
    }
    if (violation_count > 0) {
        if (violations == NULL) {
            return CANDIDATE_ERR_NULL_VIOLATION;
        }
        for (size_t i = 0; i < violation_count; i++) {
            if (violations[i] == NULL) {
                return CANDIDATE_ERR_NULL_VIOLATION;
            }
        }
    }
    if (score_denominator == 0) {
        return CANDIDATE_ERR_NON_POSITIVE_SCORE_DENOMINATOR;
    }
    if (provenance == NULL) {
        return CANDIDATE_ERR_NULL_PROVENANCE;
    }

    Candidate *candidate = (Candidate *)malloc(sizeof(Candidate));
    if (candidate == NULL) {
        return CANDIDATE_ERR_ALLOC;
    }

    Interpretation **copied_evaluations = NULL;
    if (evaluation_count > 0) {
        copied_evaluations = copy_interpretation_pointers(evaluations, evaluation_count);
        if (copied_evaluations == NULL) {
            free(candidate);
            return CANDIDATE_ERR_ALLOC;
        }
    }

    Interpretation **copied_violations = NULL;
    if (violation_count > 0) {
        copied_violations = copy_interpretation_pointers(violations, violation_count);
        if (copied_violations == NULL) {
            free(copied_evaluations);
            free(candidate);
            return CANDIDATE_ERR_ALLOC;
        }
    }

    candidate->id = id;
    candidate->material_id = material_id;
    candidate->evaluations = copied_evaluations;
    candidate->evaluation_count = evaluation_count;
    candidate->violations = copied_violations;
    candidate->violation_count = violation_count;
    candidate->score_numerator = score_numerator;
    candidate->score_denominator = score_denominator;
    candidate->provenance = provenance;

    *out = candidate;
    return CANDIDATE_OK;
}

void candidate_destroy(Candidate *candidate) {
    if (candidate == NULL) {
        return;
    }
    entity_id_destroy(candidate->id);
    entity_id_destroy(candidate->material_id);
    destroy_interpretations(candidate->evaluations, candidate->evaluation_count);
    destroy_interpretations(candidate->violations, candidate->violation_count);
    provenance_reference_destroy(candidate->provenance);
    free(candidate);
}

const EntityID *candidate_id(const Candidate *candidate) {
    return candidate->id;
}

const EntityID *candidate_material_id(const Candidate *candidate) {
    return candidate->material_id;
}

size_t candidate_evaluation_count(const Candidate *candidate) {
    return candidate->evaluation_count;
}

const Interpretation *candidate_evaluation_at(const Candidate *candidate, size_t index) {
    return candidate->evaluations[index];
}

size_t candidate_violation_count(const Candidate *candidate) {
    return candidate->violation_count;
}

const Interpretation *candidate_violation_at(const Candidate *candidate, size_t index) {
    return candidate->violations[index];
}

unsigned long candidate_score_numerator(const Candidate *candidate) {
    return candidate->score_numerator;
}

unsigned long candidate_score_denominator(const Candidate *candidate) {
    return candidate->score_denominator;
}

const ProvenanceReference *candidate_provenance(const Candidate *candidate) {
    return candidate->provenance;
}

bool candidate_identity_equals(const Candidate *a, const Candidate *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}

CandidateSetResult candidate_set_create(EntityID *id, Candidate **candidates, size_t candidate_count,
                                         unsigned long random_seed, const char *generator_version,
                                         Value *configuration, EntityID **input_ids, size_t input_id_count,
                                         const char *framework_version, ProvenanceReference *provenance,
                                         CandidateSet **out) {
    if (id == NULL) {
        return CANDIDATE_SET_ERR_NULL_ID;
    }
    if (candidate_count > 0) {
        if (candidates == NULL) {
            return CANDIDATE_SET_ERR_NULL_CANDIDATE;
        }
        for (size_t i = 0; i < candidate_count; i++) {
            if (candidates[i] == NULL) {
                return CANDIDATE_SET_ERR_NULL_CANDIDATE;
            }
        }
    }
    if (generator_version == NULL || generator_version[0] == '\0') {
        return CANDIDATE_SET_ERR_NULL_GENERATOR_VERSION;
    }
    if (input_id_count > 0) {
        if (input_ids == NULL) {
            return CANDIDATE_SET_ERR_NULL_INPUT_ID;
        }
        for (size_t i = 0; i < input_id_count; i++) {
            if (input_ids[i] == NULL) {
                return CANDIDATE_SET_ERR_NULL_INPUT_ID;
            }
        }
    }
    if (provenance == NULL) {
        return CANDIDATE_SET_ERR_NULL_PROVENANCE;
    }

    CandidateSet *set = (CandidateSet *)malloc(sizeof(CandidateSet));
    if (set == NULL) {
        return CANDIDATE_SET_ERR_ALLOC;
    }

    Candidate **copied_candidates = NULL;
    if (candidate_count > 0) {
        copied_candidates = (Candidate **)malloc(sizeof(Candidate *) * candidate_count);
        if (copied_candidates == NULL) {
            free(set);
            return CANDIDATE_SET_ERR_ALLOC;
        }
        for (size_t i = 0; i < candidate_count; i++) {
            copied_candidates[i] = candidates[i];
        }
    }

    char *copied_version = ursatz_string_copy(generator_version);
    if (copied_version == NULL) {
        free(copied_candidates);
        free(set);
        return CANDIDATE_SET_ERR_ALLOC;
    }

    char *copied_framework_version = NULL;
    if (framework_version != NULL) {
        copied_framework_version = ursatz_string_copy(framework_version);
        if (copied_framework_version == NULL) {
            free(copied_version);
            free(copied_candidates);
            free(set);
            return CANDIDATE_SET_ERR_ALLOC;
        }
    }

    EntityID **copied_input_ids = NULL;
    if (input_id_count > 0) {
        copied_input_ids = (EntityID **)malloc(sizeof(EntityID *) * input_id_count);
        if (copied_input_ids == NULL) {
            free(copied_framework_version);
            free(copied_version);
            free(copied_candidates);
            free(set);
            return CANDIDATE_SET_ERR_ALLOC;
        }
        for (size_t i = 0; i < input_id_count; i++) {
            copied_input_ids[i] = input_ids[i];
        }
    }

    set->id = id;
    set->candidates = copied_candidates;
    set->candidate_count = candidate_count;
    set->random_seed = random_seed;
    set->generator_version = copied_version;
    set->configuration = configuration;
    set->input_ids = copied_input_ids;
    set->input_id_count = input_id_count;
    set->framework_version = copied_framework_version;
    set->provenance = provenance;

    *out = set;
    return CANDIDATE_SET_OK;
}

void candidate_set_destroy(CandidateSet *set) {
    if (set == NULL) {
        return;
    }
    entity_id_destroy(set->id);
    if (set->candidates != NULL) {
        for (size_t i = 0; i < set->candidate_count; i++) {
            candidate_destroy(set->candidates[i]);
        }
        free(set->candidates);
    }
    free(set->generator_version);
    value_destroy(set->configuration);
    if (set->input_ids != NULL) {
        for (size_t i = 0; i < set->input_id_count; i++) {
            entity_id_destroy(set->input_ids[i]);
        }
        free(set->input_ids);
    }
    free(set->framework_version);
    provenance_reference_destroy(set->provenance);
    free(set);
}

const EntityID *candidate_set_id(const CandidateSet *set) {
    return set->id;
}

size_t candidate_set_candidate_count(const CandidateSet *set) {
    return set->candidate_count;
}

const Candidate *candidate_set_candidate_at(const CandidateSet *set, size_t index) {
    return set->candidates[index];
}

unsigned long candidate_set_random_seed(const CandidateSet *set) {
    return set->random_seed;
}

const char *candidate_set_generator_version(const CandidateSet *set) {
    return set->generator_version;
}

const Value *candidate_set_configuration(const CandidateSet *set) {
    return set->configuration;
}

size_t candidate_set_input_id_count(const CandidateSet *set) {
    return set->input_id_count;
}

const EntityID *candidate_set_input_id_at(const CandidateSet *set, size_t index) {
    return set->input_ids[index];
}

const char *candidate_set_framework_version(const CandidateSet *set) {
    return set->framework_version;
}

const ProvenanceReference *candidate_set_provenance(const CandidateSet *set) {
    return set->provenance;
}

bool candidate_set_identity_equals(const CandidateSet *a, const CandidateSet *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}