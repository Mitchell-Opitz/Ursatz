#include "interpretation/hypothesis.h"

#include <stdlib.h>
#include <string.h>

struct Hypothesis {
    EntityID *id;
    EntityID **observation_ids;
    size_t observation_id_count;
    EntityID **evidence_ids;
    size_t evidence_id_count;
    HypothesisCandidate *candidates;
    size_t candidate_count;
};

static HypothesisResult copy_ids(EntityID **ids, size_t count, EntityID ***out) {
    EntityID **copies = (EntityID **)malloc(sizeof(EntityID *) * count);
    if (copies == NULL) {
        return HYPOTHESIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        copies[i] = ids[i];
    }
    *out = copies;
    return HYPOTHESIS_OK;
}

static void free_ids(EntityID **ids, size_t count) {
    for (size_t i = 0; i < count; i++) {
        entity_id_destroy(ids[i]);
    }
    free(ids);
}

static void free_candidates(HypothesisCandidate *candidates, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(candidates[i].label);
    }
    free(candidates);
}

HypothesisResult hypothesis_create(EntityID *id, EntityID **observation_ids, size_t observation_id_count,
                                    EntityID **evidence_ids, size_t evidence_id_count,
                                    const HypothesisCandidate *candidates, size_t candidate_count,
                                    Hypothesis **out) {
    if (id == NULL) {
        return HYPOTHESIS_ERR_NULL_ID;
    }
    if (observation_ids == NULL || observation_id_count == 0) {
        return HYPOTHESIS_ERR_NULL_OBSERVATION_IDS;
    }
    for (size_t i = 0; i < observation_id_count; i++) {
        if (observation_ids[i] == NULL) {
            return HYPOTHESIS_ERR_NULL_OBSERVATION_ID;
        }
    }
    if (evidence_id_count > 0) {
        if (evidence_ids == NULL) {
            return HYPOTHESIS_ERR_NULL_EVIDENCE_ID;
        }
        for (size_t i = 0; i < evidence_id_count; i++) {
            if (evidence_ids[i] == NULL) {
                return HYPOTHESIS_ERR_NULL_EVIDENCE_ID;
            }
        }
    }
    if (candidates == NULL || candidate_count == 0) {
        return HYPOTHESIS_ERR_NULL_CANDIDATES;
    }
    for (size_t i = 0; i < candidate_count; i++) {
        if (candidates[i].label == NULL) {
            return HYPOTHESIS_ERR_NULL_CANDIDATE_LABEL;
        }
    }

    Hypothesis *hypothesis = (Hypothesis *)malloc(sizeof(Hypothesis));
    if (hypothesis == NULL) {
        return HYPOTHESIS_ERR_ALLOC;
    }

    HypothesisResult observation_result =
        copy_ids(observation_ids, observation_id_count, &hypothesis->observation_ids);
    if (observation_result != HYPOTHESIS_OK) {
        free(hypothesis);
        return observation_result;
    }

    hypothesis->evidence_ids = NULL;
    if (evidence_id_count > 0) {
        HypothesisResult evidence_result = copy_ids(evidence_ids, evidence_id_count, &hypothesis->evidence_ids);
        if (evidence_result != HYPOTHESIS_OK) {
            free(hypothesis->observation_ids);
            free(hypothesis);
            return evidence_result;
        }
    }

    HypothesisCandidate *owned_candidates =
        (HypothesisCandidate *)malloc(sizeof(HypothesisCandidate) * candidate_count);
    if (owned_candidates == NULL) {
        free(hypothesis->evidence_ids);
        free(hypothesis->observation_ids);
        free(hypothesis);
        return HYPOTHESIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < candidate_count; i++) {
        size_t len = strlen(candidates[i].label);
        owned_candidates[i].label = (char *)malloc(len + 1);
        if (owned_candidates[i].label == NULL) {
            for (size_t j = 0; j < i; j++) {
                free(owned_candidates[j].label);
            }
            free(owned_candidates);
            free(hypothesis->evidence_ids);
            free(hypothesis->observation_ids);
            free(hypothesis);
            return HYPOTHESIS_ERR_ALLOC;
        }
        memcpy(owned_candidates[i].label, candidates[i].label, len + 1);
        owned_candidates[i].plausibility_numerator = candidates[i].plausibility_numerator;
        owned_candidates[i].plausibility_denominator = candidates[i].plausibility_denominator;
    }

    hypothesis->id = id;
    hypothesis->observation_id_count = observation_id_count;
    hypothesis->evidence_id_count = evidence_id_count;
    hypothesis->candidates = owned_candidates;
    hypothesis->candidate_count = candidate_count;

    *out = hypothesis;
    return HYPOTHESIS_OK;
}

void hypothesis_destroy(Hypothesis *hypothesis) {
    if (hypothesis == NULL) {
        return;
    }
    entity_id_destroy(hypothesis->id);
    free_ids(hypothesis->observation_ids, hypothesis->observation_id_count);
    free_ids(hypothesis->evidence_ids, hypothesis->evidence_id_count);
    free_candidates(hypothesis->candidates, hypothesis->candidate_count);
    free(hypothesis);
}

const EntityID *hypothesis_id(const Hypothesis *hypothesis) {
    return hypothesis->id;
}

size_t hypothesis_observation_id_count(const Hypothesis *hypothesis) {
    return hypothesis->observation_id_count;
}

const EntityID *hypothesis_observation_id_at(const Hypothesis *hypothesis, size_t index) {
    return hypothesis->observation_ids[index];
}

size_t hypothesis_evidence_id_count(const Hypothesis *hypothesis) {
    return hypothesis->evidence_id_count;
}

const EntityID *hypothesis_evidence_id_at(const Hypothesis *hypothesis, size_t index) {
    return hypothesis->evidence_ids[index];
}

size_t hypothesis_candidate_count(const Hypothesis *hypothesis) {
    return hypothesis->candidate_count;
}

const char *hypothesis_candidate_label_at(const Hypothesis *hypothesis, size_t index) {
    return hypothesis->candidates[index].label;
}

unsigned long hypothesis_candidate_plausibility_numerator_at(const Hypothesis *hypothesis, size_t index) {
    return hypothesis->candidates[index].plausibility_numerator;
}

unsigned long hypothesis_candidate_plausibility_denominator_at(const Hypothesis *hypothesis, size_t index) {
    return hypothesis->candidates[index].plausibility_denominator;
}