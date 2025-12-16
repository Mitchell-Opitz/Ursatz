#ifndef URSATZ_TESTS_GENERATION_GENERATOR_TEST_UTIL_H
#define URSATZ_TESTS_GENERATION_GENERATOR_TEST_UTIL_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "generation/candidate.h"
#include "generation/constraint_extraction.h"
#include "generation/generation_plan.h"
#include "generation/generator.h"
#include "generation/intent.h"
#include "identity/entity_id.h"

/*
 * Test-only Generators and helpers for the dispatch tests. Nothing here ships
 * in src/: a concrete Generator is out of scope for the library.
 *
 *   deterministic-generator  three fixed candidates; ignores the seed (it is
 *                            declared deterministic) but records it in the set
 *   stochastic-generator     consumes the seed through a seeded PRNG
 *                            (splitmix64): how many candidates (2 to 6, unless
 *                            the configuration -- one unsigned long -- says),
 *                            and each candidate's material and score. The same
 *                            plan, seed and configuration give the same set.
 *
 * and generators that misbehave on purpose: one that fails, one that runs out
 * of memory, one that returns OK without a set, and two that return a set
 * recording another seed or another version.
 */

static inline ProvenanceReference *test_absent_provenance(void) {
    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static inline EntityID *test_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static inline GenerationPlan *test_make_plan(const char *plan_id) {
    Intent *intent = NULL;
    char intent_id[128];
    snprintf(intent_id, sizeof(intent_id), "intent-for-%s", plan_id);
    assert(intent_create(test_id(intent_id), "counter-melody", "8-measure phrase", NULL, NULL, NULL, NULL, NULL,
                         &intent) == INTENT_OK);
    ConstraintExtraction *constraints = NULL;
    assert(constraint_extraction_create(NULL, 0, NULL, 0, &constraints) == CONSTRAINT_EXTRACTION_OK);
    GenerationPlan *plan = NULL;
    assert(generation_plan_create(test_id(plan_id), intent, constraints, test_absent_provenance(), &plan) ==
           GENERATION_PLAN_OK);
    return plan;
}

static inline Value *test_make_count_configuration(unsigned long count) {
    Value *configuration = NULL;
    assert(value_create("candidate-count", &count, sizeof(count), &configuration) == VALUE_OK);
    return configuration;
}

/* splitmix64: a small seeded PRNG, so the stochastic generator needs nothing from the C library. */
static inline unsigned long long test_prng_next(unsigned long long *state) {
    unsigned long long z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static inline Candidate *test_make_candidate(const char *candidate_id, const char *material_id, unsigned long score_numerator,
                                             unsigned long score_denominator) {
    Candidate *candidate = NULL;
    assert(candidate_create(test_id(candidate_id), test_id(material_id), NULL, 0, NULL, 0, score_numerator,
                            score_denominator, test_absent_provenance(), &candidate) == CANDIDATE_OK);
    return candidate;
}

/* A CandidateSet of n candidates built by `make(index)`, recording the given seed and version. */
static inline CandidateSet *test_make_set(const char *set_id, Candidate **candidates, size_t count,
                                          unsigned long random_seed, const char *version, const Value *configuration) {
    Value *configuration_copy = NULL;
    if (configuration != NULL) {
        size_t size = 0;
        const void *content = value_content(configuration, &size);
        assert(value_create(value_type(configuration), content, size, &configuration_copy) == VALUE_OK);
    }
    CandidateSet *set = NULL;
    assert(candidate_set_create(test_id(set_id), candidates, count, random_seed, version, configuration_copy, NULL, 0,
                                NULL, test_absent_provenance(), &set) == CANDIDATE_SET_OK);
    return set;
}

#define TEST_DETERMINISTIC_VERSION "1.2.0"
#define TEST_STOCHASTIC_VERSION "0.9.1"

static inline GeneratorResult test_deterministic_generate(const GenerationPlan *plan, unsigned long random_seed,
                                                   const Value *configuration, CandidateSet **out) {
    if (plan == NULL) {
        return GENERATOR_ERR_NULL_PLAN;
    }
    if (out == NULL) {
        return GENERATOR_ERR_NULL_OUT;
    }
    Candidate **candidates = (Candidate **)calloc(3, sizeof(*candidates));
    assert(candidates != NULL);
    candidates[0] = test_make_candidate("deterministic-candidate-0", "deterministic-material-0", 1, 4);
    candidates[1] = test_make_candidate("deterministic-candidate-1", "deterministic-material-1", 2, 4);
    candidates[2] = test_make_candidate("deterministic-candidate-2", "deterministic-material-2", 3, 4);
    *out = test_make_set("deterministic-set", candidates, 3, random_seed, TEST_DETERMINISTIC_VERSION, configuration);
    free(candidates);
    return GENERATOR_OK;
}

static inline GeneratorResult test_stochastic_generate(const GenerationPlan *plan, unsigned long random_seed,
                                                const Value *configuration, CandidateSet **out) {
    if (plan == NULL) {
        return GENERATOR_ERR_NULL_PLAN;
    }
    if (out == NULL) {
        return GENERATOR_ERR_NULL_OUT;
    }
    unsigned long long state = (unsigned long long)random_seed;
    size_t count = 2 + (size_t)(test_prng_next(&state) % 5);
    if (configuration != NULL) {
        size_t size = 0;
        const void *content = value_content(configuration, &size);
        if (size != sizeof(unsigned long)) {
            return GENERATOR_ERR_GENERATION_FAILED;
        }
        unsigned long requested;
        memcpy(&requested, content, sizeof(requested));
        count = requested;
    }
    Candidate **candidates = (Candidate **)calloc(count == 0 ? 1 : count, sizeof(*candidates));
    assert(candidates != NULL);
    for (size_t i = 0; i < count; i++) {
        char candidate_id[64], material_id[64];
        snprintf(candidate_id, sizeof(candidate_id), "stochastic-candidate-%zu", i);
        snprintf(material_id, sizeof(material_id), "stochastic-material-%llu", test_prng_next(&state) % 1000000ULL);
        candidates[i] = test_make_candidate(candidate_id, material_id, (unsigned long)(test_prng_next(&state) % 101), 100);
    }
    *out = test_make_set("stochastic-set", candidates, count, random_seed, TEST_STOCHASTIC_VERSION, configuration);
    free(candidates);
    return GENERATOR_OK;
}

static inline GeneratorResult test_failing_generate(const GenerationPlan *plan, unsigned long random_seed,
                                             const Value *configuration, CandidateSet **out) {
    (void)plan;
    (void)random_seed;
    (void)configuration;
    (void)out;
    return GENERATOR_ERR_GENERATION_FAILED;
}

static inline GeneratorResult test_alloc_failing_generate(const GenerationPlan *plan, unsigned long random_seed,
                                                   const Value *configuration, CandidateSet **out) {
    (void)plan;
    (void)random_seed;
    (void)configuration;
    (void)out;
    return GENERATOR_ERR_ALLOC;
}

static inline GeneratorResult test_no_set_generate(const GenerationPlan *plan, unsigned long random_seed,
                                            const Value *configuration, CandidateSet **out) {
    (void)plan;
    (void)random_seed;
    (void)configuration;
    (void)out;
    return GENERATOR_OK; /* claims success but produced nothing */
}

static inline GeneratorResult test_wrong_seed_generate(const GenerationPlan *plan, unsigned long random_seed,
                                                const Value *configuration, CandidateSet **out) {
    (void)plan;
    *out = test_make_set("wrong-seed-set", NULL, 0, random_seed + 1, "1.0.0", configuration);
    return GENERATOR_OK;
}

static inline GeneratorResult test_wrong_version_generate(const GenerationPlan *plan, unsigned long random_seed,
                                                   const Value *configuration, CandidateSet **out) {
    (void)plan;
    *out = test_make_set("wrong-version-set", NULL, 0, random_seed, "9.9.9", configuration);
    return GENERATOR_OK;
}

#define TEST_GENERATOR(variable, generator_name, is_deterministic, generator_version, function)                    \
    static const Generator variable = {generator_name, "GenerationPlan", "CandidateSet", "none", "none",          \
                                       is_deterministic, generator_version, function}

/* Two sets are the same when every part of what a run reproduces is: the set, its seed, version and configuration,
   and each candidate's id, material and score. (candidate_set_identity_equals compares ids only.) */
static inline int test_sets_equal(const CandidateSet *a, const CandidateSet *b) {
    if (!entity_id_equals(candidate_set_id(a), candidate_set_id(b)) ||
        candidate_set_random_seed(a) != candidate_set_random_seed(b) ||
        strcmp(candidate_set_generator_version(a), candidate_set_generator_version(b)) != 0 ||
        candidate_set_candidate_count(a) != candidate_set_candidate_count(b)) {
        return 0;
    }
    const Value *configuration_a = candidate_set_configuration(a);
    const Value *configuration_b = candidate_set_configuration(b);
    if ((configuration_a == NULL) != (configuration_b == NULL) ||
        (configuration_a != NULL && !value_equals(configuration_a, configuration_b))) {
        return 0;
    }
    for (size_t i = 0; i < candidate_set_candidate_count(a); i++) {
        const Candidate *x = candidate_set_candidate_at(a, i);
        const Candidate *y = candidate_set_candidate_at(b, i);
        if (!entity_id_equals(candidate_id(x), candidate_id(y)) ||
            !entity_id_equals(candidate_material_id(x), candidate_material_id(y)) ||
            candidate_score_numerator(x) != candidate_score_numerator(y) ||
            candidate_score_denominator(x) != candidate_score_denominator(y)) {
            return 0;
        }
    }
    return 1;
}

#endif
