#include <ursatz_composer/candidate.h>

#include <stdlib.h>
#include <string.h>

#include "util/util.h"

struct Candidate {
    EntityID *id;
    EntityID *material_id;
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

CandidateResult candidate_create(EntityID *id, EntityID *material_id, unsigned long score_numerator,
                                  unsigned long score_denominator, ProvenanceReference *provenance, Candidate **out) {
    if (id == NULL) {
        return CANDIDATE_ERR_NULL_ID;
    }
    if (material_id == NULL) {
        return CANDIDATE_ERR_NULL_MATERIAL_ID;
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
    candidate->id = id;
    candidate->material_id = material_id;
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
    provenance_reference_destroy(candidate->provenance);
    free(candidate);
}

unsigned long candidate_score_numerator(const Candidate *candidate) {
    return candidate->score_numerator;
}

unsigned long candidate_score_denominator(const Candidate *candidate) {
    return candidate->score_denominator;
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

    char *copied_version = util_copy_string(generator_version);
    if (copied_version == NULL) {
        free(copied_candidates);
        free(set);
        return CANDIDATE_SET_ERR_ALLOC;
    }

    char *copied_framework_version = NULL;
    if (framework_version != NULL) {
        copied_framework_version = util_copy_string(framework_version);
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

const Candidate *candidate_set_candidate_at(const CandidateSet *set, size_t index) {
    return set->candidates[index];
}
