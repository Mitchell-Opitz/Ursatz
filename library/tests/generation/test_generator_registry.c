#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "generation/generator_registry.h"
#include "generator_test_util.h"

/*
 * Registration validates a Generator and keeps its name unique; the registry
 * borrows Generators (it never copies or frees them), finds them by exact name,
 * and keeps registration order.
 */

TEST_GENERATOR(DETERMINISTIC, "deterministic-generator", true, TEST_DETERMINISTIC_VERSION, test_deterministic_generate);
TEST_GENERATOR(STOCHASTIC, "stochastic-generator", false, TEST_STOCHASTIC_VERSION, test_stochastic_generate);

static void test_register_and_lookup(void) {
    GeneratorRegistry *registry = NULL;
    assert(generator_registry_create(&registry) == GENERATOR_REGISTRY_OK);
    assert(generator_registry_count(registry) == 0);
    assert(generator_registry_lookup(registry, "deterministic-generator") == NULL);
    assert(generator_registry_at(registry, 0) == NULL);

    assert(generator_registry_register(registry, &DETERMINISTIC) == GENERATOR_REGISTRY_OK);
    assert(generator_registry_register(registry, &STOCHASTIC) == GENERATOR_REGISTRY_OK);
    assert(generator_registry_count(registry) == 2);

    /* Lookup returns the very Generator registered (borrowed, not copied). */
    assert(generator_registry_lookup(registry, "deterministic-generator") == &DETERMINISTIC);
    assert(generator_registry_lookup(registry, "stochastic-generator") == &STOCHASTIC);
    assert(generator_registry_at(registry, 0) == &DETERMINISTIC);
    assert(generator_registry_at(registry, 1) == &STOCHASTIC);
    assert(generator_registry_at(registry, 2) == NULL);

    /* Names are exact: no prefix, no case folding, no empty name. */
    assert(generator_registry_lookup(registry, "deterministic") == NULL);
    assert(generator_registry_lookup(registry, "Deterministic-Generator") == NULL);
    assert(generator_registry_lookup(registry, "") == NULL);
    assert(generator_registry_lookup(registry, NULL) == NULL);
    assert(generator_registry_lookup(NULL, "deterministic-generator") == NULL);
    assert(generator_registry_count(NULL) == 0);
    assert(generator_registry_at(NULL, 0) == NULL);

    generator_registry_destroy(registry);
    generator_registry_destroy(NULL);
    /* The Generators are still intact: the registry never owned them. */
    assert(strcmp(DETERMINISTIC.name, "deterministic-generator") == 0);
}

static void test_duplicate_names(void) {
    GeneratorRegistry *registry = NULL;
    assert(generator_registry_create(&registry) == GENERATOR_REGISTRY_OK);
    assert(generator_registry_register(registry, &DETERMINISTIC) == GENERATOR_REGISTRY_OK);

    /* The same Generator again, and a different one under the same name, are both duplicates and change nothing. */
    assert(generator_registry_register(registry, &DETERMINISTIC) == GENERATOR_REGISTRY_ERR_DUPLICATE_NAME);
    static const Generator IMPOSTOR = {"deterministic-generator", "GenerationPlan", "CandidateSet", "none", "none",
                                       false, "9.9.9", test_stochastic_generate};
    assert(generator_registry_register(registry, &IMPOSTOR) == GENERATOR_REGISTRY_ERR_DUPLICATE_NAME);
    assert(generator_registry_count(registry) == 1);
    assert(generator_registry_lookup(registry, "deterministic-generator") == &DETERMINISTIC);
    generator_registry_destroy(registry);
}

static void test_validation(void) {
    GeneratorRegistry *registry = NULL;
    assert(generator_registry_create(&registry) == GENERATOR_REGISTRY_OK);

    Generator good = {"validated", "GenerationPlan", "CandidateSet", "none", "none", true, "1.0.0",
                      test_deterministic_generate};
    Generator bad;

    bad = good;
    bad.name = NULL;
    assert(generator_registry_register(registry, &bad) == GENERATOR_REGISTRY_ERR_INVALID_GENERATOR);
    bad = good;
    bad.name = "";
    assert(generator_registry_register(registry, &bad) == GENERATOR_REGISTRY_ERR_INVALID_GENERATOR);
    bad = good;
    bad.version = NULL;
    assert(generator_registry_register(registry, &bad) == GENERATOR_REGISTRY_ERR_INVALID_GENERATOR);
    bad = good;
    bad.version = "";
    assert(generator_registry_register(registry, &bad) == GENERATOR_REGISTRY_ERR_INVALID_GENERATOR);
    bad = good;
    bad.generate = NULL;
    assert(generator_registry_register(registry, &bad) == GENERATOR_REGISTRY_ERR_INVALID_GENERATOR);
    bad = good;
    bad.supported_inputs = NULL;
    assert(generator_registry_register(registry, &bad) == GENERATOR_REGISTRY_ERR_INVALID_GENERATOR);
    bad = good;
    bad.supported_outputs = NULL;
    assert(generator_registry_register(registry, &bad) == GENERATOR_REGISTRY_ERR_INVALID_GENERATOR);
    bad = good;
    bad.configuration_description = NULL;
    assert(generator_registry_register(registry, &bad) == GENERATOR_REGISTRY_ERR_INVALID_GENERATOR);
    bad = good;
    bad.constraints_description = NULL;
    assert(generator_registry_register(registry, &bad) == GENERATOR_REGISTRY_ERR_INVALID_GENERATOR);
    assert(generator_registry_count(registry) == 0);

    /* Empty descriptors are fine (only NULL is not); an unusual but complete Generator registers. */
    bad = good;
    bad.supported_inputs = "";
    assert(generator_registry_register(registry, &bad) == GENERATOR_REGISTRY_OK);
    assert(generator_registry_count(registry) == 1);

    assert(generator_registry_register(NULL, &good) == GENERATOR_REGISTRY_ERR_NULL_ARGUMENT);
    assert(generator_registry_register(registry, NULL) == GENERATOR_REGISTRY_ERR_NULL_ARGUMENT);
    assert(generator_registry_create(NULL) == GENERATOR_REGISTRY_ERR_NULL_ARGUMENT);
    generator_registry_destroy(registry);
}

static void test_many_generators(void) {
    GeneratorRegistry *registry = NULL;
    assert(generator_registry_create(&registry) == GENERATOR_REGISTRY_OK);
    static char names[40][16];
    static Generator generators[40];
    for (size_t i = 0; i < 40; i++) {
        snprintf(names[i], sizeof(names[i]), "generator-%zu", i);
        generators[i] = (Generator){names[i], "GenerationPlan", "CandidateSet", "none", "none", true, "1.0.0",
                                    test_deterministic_generate};
        assert(generator_registry_register(registry, &generators[i]) == GENERATOR_REGISTRY_OK);
    }
    assert(generator_registry_count(registry) == 40);
    for (size_t i = 0; i < 40; i++) {
        assert(generator_registry_at(registry, i) == &generators[i]);
        assert(generator_registry_lookup(registry, names[i]) == &generators[i]);
    }
    generator_registry_destroy(registry);
}

int main(void) {
    test_register_and_lookup();
    test_duplicate_names();
    test_validation();
    test_many_generators();
    printf("test_generator_registry: ok\n");
    return 0;
}
