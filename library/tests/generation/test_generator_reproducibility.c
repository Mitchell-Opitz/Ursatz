#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "generation/generation_record.h"
#include "generation/generator_dispatch.h"
#include "generator_test_util.h"

/*
 * Generation is reproducible from its seed (TDD Sec163, candidate.h): a
 * stochastic Generator (deterministic == false) consumes the seed through a
 * seeded PRNG and reproduces the same CandidateSet for the same plan, seed and
 * configuration; a different seed gives a different one. A deterministic
 * Generator (deterministic == true) gives an identical CandidateSet on a second
 * call with the same plan, seed and configuration -- dispatch never calls
 * generate() twice to check, so that contract is enforced here, on every
 * Generator a registry holds, and the check itself is shown to catch a
 * Generator that declares determinism it does not have.
 */

TEST_GENERATOR(DETERMINISTIC, "deterministic-generator", true, TEST_DETERMINISTIC_VERSION, test_deterministic_generate);
TEST_GENERATOR(STOCHASTIC, "stochastic-generator", false, TEST_STOCHASTIC_VERSION, test_stochastic_generate);

/* Declares determinism, and breaks it: every call numbers its candidates from a counter. */
static int g_call_count;
static GeneratorResult flaky_generate(const GenerationPlan *plan, unsigned long random_seed, const Value *configuration,
                                      CandidateSet **out) {
    (void)plan;
    char material_id[64];
    snprintf(material_id, sizeof(material_id), "flaky-material-%d", g_call_count++);
    Candidate *candidates[1] = {test_make_candidate("flaky-candidate", material_id, 1, 2)};
    *out = test_make_set("flaky-set", candidates, 1, random_seed, "1.0.0", configuration);
    return GENERATOR_OK;
}
TEST_GENERATOR(FLAKY, "flaky-generator", true, "1.0.0", flaky_generate);

static GeneratorRegistry *make_registry(void) {
    GeneratorRegistry *registry = NULL;
    assert(generator_registry_create(&registry) == GENERATOR_REGISTRY_OK);
    assert(generator_registry_register(registry, &DETERMINISTIC) == GENERATOR_REGISTRY_OK);
    assert(generator_registry_register(registry, &STOCHASTIC) == GENERATOR_REGISTRY_OK);
    assert(generator_registry_register(registry, &FLAKY) == GENERATOR_REGISTRY_OK);
    return registry;
}

static CandidateSet *run(const GeneratorRegistry *registry, const char *name, const GenerationPlan *plan,
                         unsigned long seed, const Value *configuration, GenerationRecord **record) {
    CandidateSet *set = NULL;
    GenerationRecord *made = NULL;
    assert(generator_dispatch(registry, name, plan, seed, configuration, &set, &made) == GENERATOR_DISPATCH_OK);
    if (record != NULL) {
        *record = made;
    } else {
        generation_record_destroy(made);
    }
    return set;
}

/* Same plan, seed and configuration, run twice: identical sets. */
static int repeats_identically(const GeneratorRegistry *registry, const char *name, const GenerationPlan *plan,
                               unsigned long seed, const Value *configuration) {
    CandidateSet *first = run(registry, name, plan, seed, configuration, NULL);
    CandidateSet *second = run(registry, name, plan, seed, configuration, NULL);
    int same = test_sets_equal(first, second);
    candidate_set_destroy(first);
    candidate_set_destroy(second);
    return same;
}

/* The candidates only (a deterministic Generator may ignore the seed, though the set records it). */
static int same_candidates(const CandidateSet *a, const CandidateSet *b) {
    if (candidate_set_candidate_count(a) != candidate_set_candidate_count(b)) {
        return 0;
    }
    for (size_t i = 0; i < candidate_set_candidate_count(a); i++) {
        const Candidate *x = candidate_set_candidate_at(a, i);
        const Candidate *y = candidate_set_candidate_at(b, i);
        if (!entity_id_equals(candidate_id(x), candidate_id(y)) ||
            !entity_id_equals(candidate_material_id(x), candidate_material_id(y)) ||
            candidate_score_numerator(x) != candidate_score_numerator(y)) {
            return 0;
        }
    }
    return 1;
}

static void test_the_same_seed_reproduces_the_set(void) {
    GeneratorRegistry *registry = make_registry();
    GenerationPlan *plan = test_make_plan("plan-repro");
    for (unsigned long seed = 0; seed < 25; seed++) {
        assert(repeats_identically(registry, "stochastic-generator", plan, seed, NULL));
    }
    Value *configuration = test_make_count_configuration(4);
    assert(repeats_identically(registry, "stochastic-generator", plan, 11, configuration));
    value_destroy(configuration);

    /* Reproduced however many other runs came between (no hidden state carried from run to run). */
    CandidateSet *first = run(registry, "stochastic-generator", plan, 5, NULL, NULL);
    CandidateSet *other = run(registry, "stochastic-generator", plan, 6, NULL, NULL);
    CandidateSet *third = run(registry, "stochastic-generator", plan, 5, NULL, NULL);
    assert(test_sets_equal(first, third));
    assert(!test_sets_equal(first, other));
    candidate_set_destroy(first);
    candidate_set_destroy(other);
    candidate_set_destroy(third);
    generation_plan_destroy(plan);
    generator_registry_destroy(registry);
}

static void test_different_seeds_give_different_sets(void) {
    GeneratorRegistry *registry = make_registry();
    GenerationPlan *plan = test_make_plan("plan-seeds");
    CandidateSet *sets[10];
    for (size_t i = 0; i < 10; i++) {
        sets[i] = run(registry, "stochastic-generator", plan, (unsigned long)(i + 1), NULL, NULL);
    }
    for (size_t i = 0; i < 10; i++) {
        for (size_t j = i + 1; j < 10; j++) {
            assert(!test_sets_equal(sets[i], sets[j]));
            assert(!same_candidates(sets[i], sets[j]));
        }
    }
    for (size_t i = 0; i < 10; i++) {
        candidate_set_destroy(sets[i]);
    }
    generation_plan_destroy(plan);
    generator_registry_destroy(registry);
}

static void test_a_run_is_reproducible_from_its_record(void) {
    GeneratorRegistry *registry = make_registry();
    GenerationPlan *plan = test_make_plan("plan-from-record");
    Value *configuration = test_make_count_configuration(5);

    GenerationRecord *record = NULL;
    CandidateSet *original = run(registry, "stochastic-generator", plan, 31337, configuration, &record);
    value_destroy(configuration);

    /* Everything needed to run it again is in the record: the seed, the configuration and the Generator's version. */
    assert(strcmp(generation_record_generator_version(record), generator_registry_lookup(registry, "stochastic-generator")->version) == 0);
    GenerationRecord *second_record = NULL;
    CandidateSet *again =
        run(registry, "stochastic-generator", plan, generation_record_random_seed(record),
            generation_record_configuration(record), &second_record);
    assert(test_sets_equal(original, again));
    assert(generation_record_identity_equals(record, second_record));

    generation_record_destroy(record);
    generation_record_destroy(second_record);
    candidate_set_destroy(original);
    candidate_set_destroy(again);
    generation_plan_destroy(plan);
    generator_registry_destroy(registry);
}

static void test_the_deterministic_contract(void) {
    GeneratorRegistry *registry = make_registry();
    GenerationPlan *plan = test_make_plan("plan-deterministic");
    Value *configuration = test_make_count_configuration(2);

    /* Every Generator that declares deterministic == true repeats identically for the same plan, seed and configuration. */
    size_t checked = 0;
    for (size_t i = 0; i < generator_registry_count(registry); i++) {
        const Generator *generator = generator_registry_at(registry, i);
        if (!generator->deterministic || strcmp(generator->name, "flaky-generator") == 0) {
            continue;
        }
        for (unsigned long seed = 0; seed < 5; seed++) {
            assert(repeats_identically(registry, generator->name, plan, seed, NULL));
            assert(repeats_identically(registry, generator->name, plan, seed, configuration));
        }
        checked++;
    }
    assert(checked == 1);

    /* It may ignore the seed: another seed gives the same candidates (the set still records the seed it was given). */
    CandidateSet *seed_one = run(registry, "deterministic-generator", plan, 1, NULL, NULL);
    CandidateSet *seed_two = run(registry, "deterministic-generator", plan, 2, NULL, NULL);
    assert(same_candidates(seed_one, seed_two));
    assert(candidate_set_random_seed(seed_one) == 1 && candidate_set_random_seed(seed_two) == 2);
    candidate_set_destroy(seed_one);
    candidate_set_destroy(seed_two);

    /* The check has teeth: a Generator that declares determinism it does not have is caught. */
    assert(generator_registry_lookup(registry, "flaky-generator")->deterministic);
    assert(!repeats_identically(registry, "flaky-generator", plan, 1, NULL));

    /* And a stochastic Generator really consumes the seed: it is declared so, and different seeds differ. */
    assert(!generator_registry_lookup(registry, "stochastic-generator")->deterministic);

    value_destroy(configuration);
    generation_plan_destroy(plan);
    generator_registry_destroy(registry);
}

int main(void) {
    test_the_same_seed_reproduces_the_set();
    test_different_seeds_give_different_sets();
    test_a_run_is_reproducible_from_its_record();
    test_the_deterministic_contract();
    printf("test_generator_reproducibility: ok\n");
    return 0;
}
