#include "generation/generator_dispatch.h"

#include <string.h>

GeneratorDispatchResult generator_dispatch(const GeneratorRegistry *registry, const char *name,
                                           const GenerationPlan *plan, unsigned long random_seed,
                                           const Value *configuration, CandidateSet **out_set) {
    if (registry == NULL || name == NULL || plan == NULL || out_set == NULL) {
        return GENERATOR_DISPATCH_ERR_NULL_ARGUMENT;
    }
    const Generator *generator = generator_registry_lookup(registry, name);
    if (generator == NULL) {
        return GENERATOR_DISPATCH_ERR_UNKNOWN_GENERATOR;
    }

    CandidateSet *set = NULL;
    GeneratorResult generated = generator->generate(plan, random_seed, configuration, &set);
    if (generated == GENERATOR_ERR_ALLOC) {
        return GENERATOR_DISPATCH_ERR_ALLOC;
    }
    if (generated != GENERATOR_OK || set == NULL) {
        return GENERATOR_DISPATCH_ERR_GENERATION_FAILED;
    }

    if (candidate_set_random_seed(set) != random_seed ||
        strcmp(candidate_set_generator_version(set), generator->version) != 0) {
        candidate_set_destroy(set);
        return GENERATOR_DISPATCH_ERR_CONTRACT_VIOLATION;
    }

    *out_set = set;
    return GENERATOR_DISPATCH_OK;
}
