#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "generation/generation_record.h"
#include "generation/generator_dispatch.h"
#include "generator_test_util.h"

/*
 * Dispatch runs one registered Generator by name and records the run. A
 * successful run returns the CandidateSet and a GenerationRecord with the exact
 * seed, the Generator's version, a copy of the configuration and no framework
 * version; every failure returns the right result, leaves both outputs
 * untouched and returns no record; the plan is never modified.
 */

TEST_GENERATOR(DETERMINISTIC, "deterministic-generator", true, TEST_DETERMINISTIC_VERSION, test_deterministic_generate);
TEST_GENERATOR(STOCHASTIC, "stochastic-generator", false, TEST_STOCHASTIC_VERSION, test_stochastic_generate);
TEST_GENERATOR(FAILING, "failing-generator", true, "1.0.0", test_failing_generate);
TEST_GENERATOR(ALLOC_FAILING, "alloc-failing-generator", true, "1.0.0", test_alloc_failing_generate);
TEST_GENERATOR(NO_SET, "no-set-generator", true, "1.0.0", test_no_set_generate);
TEST_GENERATOR(WRONG_SEED, "wrong-seed-generator", true, "1.0.0", test_wrong_seed_generate);
TEST_GENERATOR(WRONG_VERSION, "wrong-version-generator", true, "1.0.0", test_wrong_version_generate);

#define UNTOUCHED_SET ((CandidateSet *)0x1)
#define UNTOUCHED_RECORD ((GenerationRecord *)0x1)

static GeneratorRegistry *make_registry(void) {
    GeneratorRegistry *registry = NULL;
    assert(generator_registry_create(&registry) == GENERATOR_REGISTRY_OK);
    const Generator *all[] = {&DETERMINISTIC, &STOCHASTIC, &FAILING, &ALLOC_FAILING, &NO_SET, &WRONG_SEED, &WRONG_VERSION};
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        assert(generator_registry_register(registry, all[i]) == GENERATOR_REGISTRY_OK);
    }
    return registry;
}

static void test_successful_run_and_its_record(void) {
    GeneratorRegistry *registry = make_registry();
    GenerationPlan *plan = test_make_plan("plan-1");

    CandidateSet *set = NULL;
    GenerationRecord *record = NULL;
    assert(generator_dispatch(registry, "deterministic-generator", plan, 42, NULL, &set, &record) ==
           GENERATOR_DISPATCH_OK);
    assert(set != NULL && record != NULL);
    assert(candidate_set_candidate_count(set) == 3);

    /* The record carries exactly what the run used. */
    assert(generation_record_random_seed(record) == 42);
    assert(strcmp(generation_record_generator_version(record), TEST_DETERMINISTIC_VERSION) == 0);
    assert(generation_record_framework_version(record) == NULL); /* the plan has no framework, and Framework no version */
    assert(generation_record_configuration(record) == NULL);
    assert(strcmp(entity_id_value(generation_record_id(record)),
                  "generation-record:plan-1:deterministic-generator:1.2.0:42:none") == 0);
    const ProvenanceReference *provenance = generation_record_provenance(record);
    assert(provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT);
    assert(strcmp(entity_id_value(provenance_reference_target_id(provenance)), "plan-1") == 0);

    /* The record and the set agree about the run. */
    assert(candidate_set_random_seed(set) == generation_record_random_seed(record));
    assert(strcmp(candidate_set_generator_version(set), generation_record_generator_version(record)) == 0);

    generation_record_destroy(record);
    candidate_set_destroy(set);
    generation_plan_destroy(plan);
    generator_registry_destroy(registry);
}

static void test_record_exact_seed_and_version(void) {
    GeneratorRegistry *registry = make_registry();
    GenerationPlan *plan = test_make_plan("plan-seeds");
    const unsigned long seeds[] = {0, 1, 7, 123456789UL, ULONG_MAX};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); i++) {
        CandidateSet *set = NULL;
        GenerationRecord *record = NULL;
        assert(generator_dispatch(registry, "stochastic-generator", plan, seeds[i], NULL, &set, &record) ==
               GENERATOR_DISPATCH_OK);
        assert(generation_record_random_seed(record) == seeds[i]);
        assert(strcmp(generation_record_generator_version(record), TEST_STOCHASTIC_VERSION) == 0);
        char expected[160];
        snprintf(expected, sizeof(expected), "generation-record:plan-seeds:stochastic-generator:0.9.1:%lu:none", seeds[i]);
        assert(strcmp(entity_id_value(generation_record_id(record)), expected) == 0);
        generation_record_destroy(record);
        candidate_set_destroy(set);
    }
    generation_plan_destroy(plan);
    generator_registry_destroy(registry);
}

static void test_configuration_is_copied_and_part_of_the_id(void) {
    GeneratorRegistry *registry = make_registry();
    GenerationPlan *plan = test_make_plan("plan-config");
    Value *three = test_make_count_configuration(3);
    Value *four = test_make_count_configuration(4);

    CandidateSet *set = NULL;
    GenerationRecord *record = NULL;
    assert(generator_dispatch(registry, "stochastic-generator", plan, 5, three, &set, &record) == GENERATOR_DISPATCH_OK);
    assert(candidate_set_candidate_count(set) == 3);
    const Value *recorded = generation_record_configuration(record);
    assert(recorded != NULL && recorded != three && value_equals(recorded, three));
    const char *prefix = "generation-record:plan-config:stochastic-generator:0.9.1:5:";
    const char *id = entity_id_value(generation_record_id(record));
    assert(strncmp(id, prefix, strlen(prefix)) == 0);
    assert(strlen(id) == strlen(prefix) + 16); /* the configuration hash, 16 hex digits */
    char first_id[200];
    snprintf(first_id, sizeof(first_id), "%s", id);

    /* The copy outlives the caller's Value. */
    value_destroy(three);
    assert(value_equals(recorded, recorded));
    size_t size = 0;
    unsigned long copied;
    memcpy(&copied, value_content(recorded, &size), sizeof(copied));
    assert(size == sizeof(copied) && copied == 3);

    /* The same run gives the same id; another configuration, seed, plan or Generator gives another. */
    three = test_make_count_configuration(3);
    CandidateSet *set2 = NULL;
    GenerationRecord *record2 = NULL;
    assert(generator_dispatch(registry, "stochastic-generator", plan, 5, three, &set2, &record2) == GENERATOR_DISPATCH_OK);
    assert(strcmp(entity_id_value(generation_record_id(record2)), first_id) == 0);
    assert(generation_record_identity_equals(record, record2));
    generation_record_destroy(record2);
    candidate_set_destroy(set2);

    assert(generator_dispatch(registry, "stochastic-generator", plan, 5, four, &set2, &record2) == GENERATOR_DISPATCH_OK);
    assert(strcmp(entity_id_value(generation_record_id(record2)), first_id) != 0);
    generation_record_destroy(record2);
    candidate_set_destroy(set2);

    assert(generator_dispatch(registry, "stochastic-generator", plan, 6, three, &set2, &record2) == GENERATOR_DISPATCH_OK);
    assert(strcmp(entity_id_value(generation_record_id(record2)), first_id) != 0);
    generation_record_destroy(record2);
    candidate_set_destroy(set2);

    GenerationPlan *other_plan = test_make_plan("plan-config-other");
    assert(generator_dispatch(registry, "stochastic-generator", other_plan, 5, three, &set2, &record2) ==
           GENERATOR_DISPATCH_OK);
    assert(strcmp(entity_id_value(generation_record_id(record2)), first_id) != 0);
    generation_record_destroy(record2);
    candidate_set_destroy(set2);
    generation_plan_destroy(other_plan);

    /* A configuration with no bytes is still a configuration: it is copied, and differs from none. */
    Value *empty = NULL;
    assert(value_create("empty-configuration", "", 0, &empty) == VALUE_OK);
    assert(generator_dispatch(registry, "deterministic-generator", plan, 5, empty, &set2, &record2) ==
           GENERATOR_DISPATCH_OK);
    assert(generation_record_configuration(record2) != NULL);
    size_t empty_size = 99;
    value_content(generation_record_configuration(record2), &empty_size);
    assert(empty_size == 0);
    assert(strstr(entity_id_value(generation_record_id(record2)), ":none") == NULL);
    generation_record_destroy(record2);
    candidate_set_destroy(set2);
    value_destroy(empty);

    generation_record_destroy(record);
    candidate_set_destroy(set);
    value_destroy(three);
    value_destroy(four);
    generation_plan_destroy(plan);
    generator_registry_destroy(registry);
}

static void assert_untouched_failure(const GeneratorRegistry *registry, const char *name, const GenerationPlan *plan,
                                     GeneratorDispatchResult expected) {
    CandidateSet *set = UNTOUCHED_SET;
    GenerationRecord *record = UNTOUCHED_RECORD;
    assert(generator_dispatch(registry, name, plan, 9, NULL, &set, &record) == expected);
    assert(set == UNTOUCHED_SET && record == UNTOUCHED_RECORD);
}

static void test_failures_leave_the_outputs_untouched(void) {
    GeneratorRegistry *registry = make_registry();
    GenerationPlan *plan = test_make_plan("plan-failures");

    assert_untouched_failure(registry, "no-such-generator", plan, GENERATOR_DISPATCH_ERR_UNKNOWN_GENERATOR);
    assert_untouched_failure(registry, "", plan, GENERATOR_DISPATCH_ERR_UNKNOWN_GENERATOR);
    assert_untouched_failure(registry, "Deterministic-Generator", plan, GENERATOR_DISPATCH_ERR_UNKNOWN_GENERATOR);
    assert_untouched_failure(registry, "failing-generator", plan, GENERATOR_DISPATCH_ERR_GENERATION_FAILED);
    assert_untouched_failure(registry, "no-set-generator", plan, GENERATOR_DISPATCH_ERR_GENERATION_FAILED);
    assert_untouched_failure(registry, "alloc-failing-generator", plan, GENERATOR_DISPATCH_ERR_ALLOC);
    assert_untouched_failure(registry, "wrong-seed-generator", plan, GENERATOR_DISPATCH_ERR_CONTRACT_VIOLATION);
    assert_untouched_failure(registry, "wrong-version-generator", plan, GENERATOR_DISPATCH_ERR_CONTRACT_VIOLATION);

    /* A configuration the generator rejects is a generation failure. */
    Value *bad_configuration = NULL;
    assert(value_create("not-a-count", "x", 1, &bad_configuration) == VALUE_OK);
    CandidateSet *set = UNTOUCHED_SET;
    GenerationRecord *record = UNTOUCHED_RECORD;
    assert(generator_dispatch(registry, "stochastic-generator", plan, 1, bad_configuration, &set, &record) ==
           GENERATOR_DISPATCH_ERR_GENERATION_FAILED);
    assert(set == UNTOUCHED_SET && record == UNTOUCHED_RECORD);
    value_destroy(bad_configuration);

    /* A run after failures works as usual. */
    assert(generator_dispatch(registry, "deterministic-generator", plan, 1, NULL, &set, &record) == GENERATOR_DISPATCH_OK);
    generation_record_destroy(record);
    candidate_set_destroy(set);

    generation_plan_destroy(plan);
    generator_registry_destroy(registry);
}

static void test_null_arguments(void) {
    GeneratorRegistry *registry = make_registry();
    GenerationPlan *plan = test_make_plan("plan-nulls");
    CandidateSet *set = UNTOUCHED_SET;
    GenerationRecord *record = UNTOUCHED_RECORD;

    assert(generator_dispatch(NULL, "deterministic-generator", plan, 1, NULL, &set, &record) ==
           GENERATOR_DISPATCH_ERR_NULL_ARGUMENT);
    assert(generator_dispatch(registry, NULL, plan, 1, NULL, &set, &record) == GENERATOR_DISPATCH_ERR_NULL_ARGUMENT);
    assert(generator_dispatch(registry, "deterministic-generator", NULL, 1, NULL, &set, &record) ==
           GENERATOR_DISPATCH_ERR_NULL_ARGUMENT);
    assert(generator_dispatch(registry, "deterministic-generator", plan, 1, NULL, NULL, &record) ==
           GENERATOR_DISPATCH_ERR_NULL_ARGUMENT);
    assert(generator_dispatch(registry, "deterministic-generator", plan, 1, NULL, &set, NULL) ==
           GENERATOR_DISPATCH_ERR_NULL_ARGUMENT);
    assert(set == UNTOUCHED_SET && record == UNTOUCHED_RECORD);

    /* NULL arguments are checked before the name is looked up, so an unknown name with a NULL plan is a NULL argument. */
    assert(generator_dispatch(registry, "no-such-generator", NULL, 1, NULL, &set, &record) ==
           GENERATOR_DISPATCH_ERR_NULL_ARGUMENT);

    generation_plan_destroy(plan);
    generator_registry_destroy(registry);
}

static void test_the_plan_is_not_modified(void) {
    GeneratorRegistry *registry = make_registry();
    GenerationPlan *plan = test_make_plan("plan-unchanged");

    const EntityID *id = generation_plan_id(plan);
    const Intent *intent = generation_plan_intent(plan);
    const ConstraintExtraction *constraints = generation_plan_constraints(plan);
    const ProvenanceReference *provenance = generation_plan_provenance(plan);
    char id_value[64], intent_id_value[64];
    snprintf(id_value, sizeof(id_value), "%s", entity_id_value(id));
    snprintf(intent_id_value, sizeof(intent_id_value), "%s", entity_id_value(intent_id(intent)));

    const char *names[] = {"deterministic-generator", "stochastic-generator", "failing-generator", "no-set-generator",
                           "wrong-seed-generator", "no-such-generator"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        CandidateSet *set = NULL;
        GenerationRecord *record = NULL;
        if (generator_dispatch(registry, names[i], plan, 77, NULL, &set, &record) == GENERATOR_DISPATCH_OK) {
            generation_record_destroy(record);
            candidate_set_destroy(set);
        }
        assert(generation_plan_id(plan) == id && strcmp(entity_id_value(id), id_value) == 0);
        assert(generation_plan_intent(plan) == intent && strcmp(entity_id_value(intent_id(intent)), intent_id_value) == 0);
        assert(generation_plan_constraints(plan) == constraints);
        assert(generation_plan_provenance(plan) == provenance);
    }
    generation_plan_destroy(plan);
    generator_registry_destroy(registry);
}

int main(void) {
    test_successful_run_and_its_record();
    test_record_exact_seed_and_version();
    test_configuration_is_copied_and_part_of_the_id();
    test_failures_leave_the_outputs_untouched();
    test_null_arguments();
    test_the_plan_is_not_modified();
    printf("test_generator_dispatch: ok\n");
    return 0;
}
